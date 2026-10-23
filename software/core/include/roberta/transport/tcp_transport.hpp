#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <roberta/transport/text_transport.hpp>

namespace roberta::transport {

inline constexpr std::uint16_t k_default_axis_tcp_port = 7300;
inline constexpr std::string_view k_tcp_url_scheme = "tcp://";

struct TcpEndpointConfig final {
    std::string host;
    std::uint16_t port{k_default_axis_tcp_port};
    std::chrono::milliseconds connect_timeout{3000};
    // WiFi round trips are slower and burstier than USB, so short exchange
    // timeouts are raised to this floor.
    std::chrono::milliseconds minimum_response_timeout{1000};
};

// Parses "tcp://host" or "tcp://host:port". Hosts may be IPv4 literals or
// names such as "roberta-j1.local".
[[nodiscard]] std::optional<TcpEndpointConfig> parse_tcp_url(std::string_view url);

// Axis console over a TCP connection to a WiFi axis controller.
class PosixTcpTextTransport final : public ITextTransport {
public:
    explicit PosixTcpTextTransport(TcpEndpointConfig config);
    ~PosixTcpTextTransport() override;

    PosixTcpTextTransport(const PosixTcpTextTransport&) = delete;
    PosixTcpTextTransport& operator=(const PosixTcpTextTransport&) = delete;

    roberta::core::Status open() override;
    void close() noexcept override;
    [[nodiscard]] bool is_open() const noexcept override;
    [[nodiscard]] roberta::core::Result<std::string> transact(const TextExchange& exchange) override;

    [[nodiscard]] const TcpEndpointConfig& config() const noexcept { return config_; }

private:
    [[nodiscard]] roberta::core::Result<std::string> read_until_prompt(std::chrono::milliseconds timeout);
    [[nodiscard]] roberta::core::Status write_line(std::string_view line);
    void discard_pending_input();

    TcpEndpointConfig config_;
    int fd_{-1};
};

// Picks the console transport for an endpoint string: "tcp://..." connects
// over the network, anything else is treated as a serial device path.
[[nodiscard]] std::unique_ptr<ITextTransport> make_console_text_transport(
    std::string_view endpoint, int baud_rate = 115200);

} // namespace roberta::transport
