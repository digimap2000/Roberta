#include <roberta/transport/tcp_transport.hpp>

#include <roberta/transport/serial_transport.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace roberta::transport {
namespace {

using Clock = std::chrono::steady_clock;

[[nodiscard]] roberta::core::Error io_error(std::string_view what)
{
    return roberta::core::make_error(
        roberta::core::ErrorCode::io, std::format("{}: {}", what, std::strerror(errno)));
}

[[nodiscard]] int remaining_ms(Clock::time_point deadline)
{
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
    return static_cast<int>(std::max<std::chrono::milliseconds::rep>(remaining.count(), 0));
}

[[nodiscard]] bool ends_with_prompt(std::string_view text)
{
    return text.size() >= k_console_prompt.size() &&
           text.substr(text.size() - k_console_prompt.size()) == k_console_prompt;
}

// Connects without blocking past the deadline; returns the connected socket.
[[nodiscard]] roberta::core::Result<int> connect_with_timeout(const addrinfo& address, Clock::time_point deadline)
{
    const int fd = ::socket(address.ai_family, address.ai_socktype, address.ai_protocol);
    if (fd < 0) {
        return std::unexpected(io_error("socket"));
    }

    const auto fail = [fd](roberta::core::Error error) -> roberta::core::Result<int> {
        ::close(fd);
        return std::unexpected(std::move(error));
    };

    if (::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK) != 0) {
        return fail(io_error("fcntl"));
    }

    if (::connect(fd, address.ai_addr, address.ai_addrlen) != 0) {
        if (errno != EINPROGRESS) {
            return fail(io_error("connect"));
        }

        pollfd waiter{.fd = fd, .events = POLLOUT, .revents = 0};
        const int ready = ::poll(&waiter, 1, remaining_ms(deadline));
        if (ready == 0) {
            return fail(roberta::core::make_error(roberta::core::ErrorCode::timeout, "connect timeout"));
        }
        if (ready < 0) {
            return fail(io_error("poll"));
        }

        int socket_error = 0;
        socklen_t length = sizeof(socket_error);
        if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &length) != 0) {
            return fail(io_error("getsockopt"));
        }
        if (socket_error != 0) {
            errno = socket_error;
            return fail(io_error("connect"));
        }
    }

    const int enabled = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
#ifdef SO_NOSIGPIPE
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
    return fd;
}

} // namespace

std::optional<TcpEndpointConfig> parse_tcp_url(std::string_view url)
{
    if (!url.starts_with(k_tcp_url_scheme)) {
        return std::nullopt;
    }

    auto authority = url.substr(k_tcp_url_scheme.size());
    if (const auto slash = authority.find('/'); slash != std::string_view::npos) {
        authority = authority.substr(0, slash);
    }

    TcpEndpointConfig config;
    if (const auto colon = authority.rfind(':'); colon != std::string_view::npos) {
        const auto port_text = authority.substr(colon + 1);
        unsigned int port = 0;
        const auto [end, error] = std::from_chars(port_text.data(), port_text.data() + port_text.size(), port);
        if (error != std::errc{} || end != port_text.data() + port_text.size() || port == 0 || port > 65535) {
            return std::nullopt;
        }
        config.port = static_cast<std::uint16_t>(port);
        authority = authority.substr(0, colon);
    }

    if (authority.empty()) {
        return std::nullopt;
    }

    config.host = std::string(authority);
    return config;
}

PosixTcpTextTransport::PosixTcpTextTransport(TcpEndpointConfig config)
    : config_(std::move(config))
{
}

PosixTcpTextTransport::~PosixTcpTextTransport()
{
    close();
}

roberta::core::Status PosixTcpTextTransport::open()
{
    close();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    const auto port = std::to_string(config_.port);

    // getaddrinfo resolves ".local" names through mDNS on macOS.
    if (const int result = ::getaddrinfo(config_.host.c_str(), port.c_str(), &hints, &addresses); result != 0) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::io,
            std::format("cannot resolve {}: {}", config_.host, ::gai_strerror(result))));
    }

    const auto deadline = Clock::now() + config_.connect_timeout;
    roberta::core::Error last_error = roberta::core::make_error(
        roberta::core::ErrorCode::io, std::format("no addresses for {}", config_.host));

    for (const addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        auto connected = connect_with_timeout(*address, deadline);
        if (connected) {
            fd_ = *connected;
            break;
        }
        last_error = connected.error();
    }
    ::freeaddrinfo(addresses);

    if (fd_ < 0) {
        return std::unexpected(roberta::core::make_error(
            last_error.code,
            std::format("{}:{}: {}", config_.host, config_.port, last_error.message)));
    }

    // The controller greets each client with a banner ending in a prompt. If it
    // was missed, nudge the console for a fresh one.
    auto banner = read_until_prompt(std::chrono::milliseconds(remaining_ms(deadline)));
    if (banner) {
        return roberta::core::ok_status();
    }
    if (!is_open()) {
        return std::unexpected(banner.error());
    }

    auto status = write_line("");
    if (status) {
        auto prompt = read_until_prompt(config_.minimum_response_timeout);
        if (prompt) {
            return roberta::core::ok_status();
        }
        status = std::unexpected(prompt.error());
    }

    close();
    return std::unexpected(roberta::core::make_error(
        roberta::core::ErrorCode::timeout,
        std::format("{}:{} connected but no console prompt: {}", config_.host, config_.port, status.error().message)));
}

void PosixTcpTextTransport::close() noexcept
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool PosixTcpTextTransport::is_open() const noexcept
{
    return fd_ >= 0;
}

roberta::core::Result<std::string> PosixTcpTextTransport::transact(const TextExchange& exchange)
{
    if (!is_open()) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::failed_precondition, "tcp console transport is not open"));
    }

    discard_pending_input();

    auto status = write_line(exchange.request_line);
    if (!status) {
        return std::unexpected(status.error());
    }

    const auto timeout = std::max(exchange.timeout, config_.minimum_response_timeout);
    const auto deadline = Clock::now() + timeout;
    while (Clock::now() < deadline) {
        auto response = read_until_prompt(std::chrono::milliseconds(remaining_ms(deadline)));
        if (!response) {
            return std::unexpected(response.error());
        }

        auto normalized = normalize_console_response(std::move(*response));
        if (!normalized.empty()) {
            return normalized;
        }
    }

    return std::unexpected(roberta::core::make_error(
        roberta::core::ErrorCode::timeout, "tcp console prompt-only response"));
}

roberta::core::Result<std::string> PosixTcpTextTransport::read_until_prompt(std::chrono::milliseconds timeout)
{
    std::string response;
    const auto deadline = Clock::now() + timeout;

    while (true) {
        pollfd waiter{.fd = fd_, .events = POLLIN, .revents = 0};
        const int ready = ::poll(&waiter, 1, remaining_ms(deadline));
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected(io_error("poll"));
        }
        if (ready == 0) {
            return std::unexpected(roberta::core::make_error(
                roberta::core::ErrorCode::timeout,
                response.empty() ? "tcp console timeout" : "tcp console prompt timeout"));
        }

        // Read one byte at a time so bytes after the prompt stay queued.
        char ch{};
        const auto count = ::recv(fd_, &ch, 1, 0);
        if (count == 0) {
            close();
            return std::unexpected(roberta::core::make_error(
                roberta::core::ErrorCode::io, "axis controller closed the connection"));
        }
        if (count < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            auto error = io_error("recv");
            close();
            return std::unexpected(std::move(error));
        }

        response.push_back(ch);
        if (ends_with_prompt(response)) {
            return response;
        }
    }
}

roberta::core::Status PosixTcpTextTransport::write_line(std::string_view line)
{
    std::string request(line);
    request.append("\r\n");

    std::size_t sent_total = 0;
    while (sent_total < request.size()) {
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        const auto sent = ::send(fd_, request.data() + sent_total, request.size() - sent_total, flags);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                pollfd waiter{.fd = fd_, .events = POLLOUT, .revents = 0};
                ::poll(&waiter, 1, 100);
                continue;
            }
            auto error = io_error("send");
            close();
            return std::unexpected(std::move(error));
        }
        sent_total += static_cast<std::size_t>(sent);
    }

    return roberta::core::ok_status();
}

void PosixTcpTextTransport::discard_pending_input()
{
    std::array<char, 256> buffer{};
    while (::recv(fd_, buffer.data(), buffer.size(), MSG_DONTWAIT) > 0) {
    }
}

std::unique_ptr<ITextTransport> make_console_text_transport(std::string_view endpoint, int baud_rate)
{
    if (auto config = parse_tcp_url(endpoint)) {
        return std::make_unique<PosixTcpTextTransport>(std::move(*config));
    }

    return std::make_unique<PosixTextTransport>(SerialPortConfig{
        .device_path = std::string(endpoint),
        .baud_rate = baud_rate,
    });
}

} // namespace roberta::transport
