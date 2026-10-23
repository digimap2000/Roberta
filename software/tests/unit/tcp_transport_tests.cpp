#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <atomic>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include <roberta/transport/serial_transport.hpp>
#include <roberta/transport/tcp_transport.hpp>

namespace {

using namespace std::chrono_literals;

// Minimal stand-in for the firmware's WiFi console: greets with a banner,
// answers "ping" and echoes anything else, one client at a time.
class FakeConsoleServer final {
public:
    FakeConsoleServer()
    {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        const int enabled = 1;
        ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        ::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address));
        ::listen(listen_fd_, 1);

        socklen_t length = sizeof(address);
        ::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&address), &length);
        port_ = ntohs(address.sin_port);

        thread_ = std::thread([this] { serve(); });
    }

    ~FakeConsoleServer()
    {
        stop_ = true;
        ::shutdown(listen_fd_, SHUT_RDWR);
        ::close(listen_fd_);
        if (const int client = client_fd_.exchange(-1); client >= 0) {
            ::shutdown(client, SHUT_RDWR);
        }
        thread_.join();
    }

    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

    void drop_client()
    {
        if (const int client = client_fd_.exchange(-1); client >= 0) {
            ::shutdown(client, SHUT_RDWR);
        }
    }

private:
    void send_text(int fd, std::string_view text)
    {
        (void)::send(fd, text.data(), text.size(), 0);
    }

    void serve()
    {
        while (!stop_) {
            const int client = ::accept(listen_fd_, nullptr, nullptr);
            if (client < 0) {
                return;
            }
            client_fd_ = client;
            send_text(client, "axiscontroller ready\r\naxis> ");

            std::string line;
            char ch{};
            while (::recv(client, &ch, 1, 0) == 1) {
                if (ch == '\n') {
                    continue;
                }
                if (ch != '\r') {
                    line.push_back(ch);
                    continue;
                }
                if (line == "ping") {
                    send_text(client, "pong\r\naxis> ");
                } else if (line.empty()) {
                    send_text(client, "axis> ");
                } else {
                    send_text(client, "echo " + line + "\r\naxis> ");
                }
                line.clear();
            }
            ::close(client);
        }
    }

    int listen_fd_{-1};
    std::uint16_t port_{0};
    std::atomic<int> client_fd_{-1};
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

[[nodiscard]] roberta::transport::TcpEndpointConfig loopback(std::uint16_t port)
{
    return roberta::transport::TcpEndpointConfig{
        .host = "127.0.0.1",
        .port = port,
        .connect_timeout = 1000ms,
        .minimum_response_timeout = 500ms,
    };
}

} // namespace

TEST(TcpTransportTests, ParsesTcpUrls)
{
    const auto named = roberta::transport::parse_tcp_url("tcp://roberta-j1.local");
    ASSERT_TRUE(named.has_value());
    EXPECT_EQ(named->host, "roberta-j1.local");
    EXPECT_EQ(named->port, roberta::transport::k_default_axis_tcp_port);

    const auto addressed = roberta::transport::parse_tcp_url("tcp://192.168.1.42:7400/");
    ASSERT_TRUE(addressed.has_value());
    EXPECT_EQ(addressed->host, "192.168.1.42");
    EXPECT_EQ(addressed->port, 7400);

    EXPECT_FALSE(roberta::transport::parse_tcp_url("/dev/cu.usbmodem101").has_value());
    EXPECT_FALSE(roberta::transport::parse_tcp_url("tcp://").has_value());
    EXPECT_FALSE(roberta::transport::parse_tcp_url("tcp://host:").has_value());
    EXPECT_FALSE(roberta::transport::parse_tcp_url("tcp://host:70000").has_value());
    EXPECT_FALSE(roberta::transport::parse_tcp_url("tcp://host:12ab").has_value());
}

TEST(TcpTransportTests, FactoryPicksTransportFromEndpoint)
{
    auto tcp = roberta::transport::make_console_text_transport("tcp://127.0.0.1:7300");
    EXPECT_NE(dynamic_cast<roberta::transport::PosixTcpTextTransport*>(tcp.get()), nullptr);

    auto serial = roberta::transport::make_console_text_transport("/dev/cu.usbmodem101");
    EXPECT_NE(dynamic_cast<roberta::transport::PosixTextTransport*>(serial.get()), nullptr);
}

TEST(TcpTransportTests, ExchangesConsoleLinesWithController)
{
    FakeConsoleServer server;
    roberta::transport::PosixTcpTextTransport transport(loopback(server.port()));

    ASSERT_TRUE(transport.open().has_value());
    ASSERT_TRUE(transport.is_open());

    auto pong = transport.transact({.request_line = "ping"});
    ASSERT_TRUE(pong.has_value()) << pong.error().message;
    EXPECT_EQ(*pong, "pong");

    auto echo = transport.transact({.request_line = "reg read STATUS_WORD"});
    ASSERT_TRUE(echo.has_value()) << echo.error().message;
    EXPECT_EQ(*echo, "echo reg read STATUS_WORD");
}

TEST(TcpTransportTests, ReportsClosedConnectionAndCanReconnect)
{
    FakeConsoleServer server;
    roberta::transport::PosixTcpTextTransport transport(loopback(server.port()));
    ASSERT_TRUE(transport.open().has_value());

    server.drop_client();
    auto result = transport.transact({.request_line = "ping"});
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(transport.is_open());

    ASSERT_TRUE(transport.open().has_value());
    auto pong = transport.transact({.request_line = "ping"});
    ASSERT_TRUE(pong.has_value()) << pong.error().message;
    EXPECT_EQ(*pong, "pong");
}

TEST(TcpTransportTests, FailsFastWhenNothingIsListening)
{
    std::uint16_t port = 0;
    {
        FakeConsoleServer server;
        port = server.port();
    }

    roberta::transport::PosixTcpTextTransport transport(loopback(port));
    auto status = transport.open();
    EXPECT_FALSE(status.has_value());
    EXPECT_FALSE(transport.is_open());
}
