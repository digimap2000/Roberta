#include <roberta/transport/serial_transport.hpp>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <span>
#include <string>
#include <string_view>
#include <termios.h>
#include <unistd.h>

namespace roberta::transport {
namespace {

[[nodiscard]] roberta::core::Status set_common_attributes(int fd, int baud_rate)
{
    termios options{};
    if (tcgetattr(fd, &options) != 0) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::io, std::strerror(errno)));
    }

    cfmakeraw(&options);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CRTSCTS;

    speed_t speed = B115200;
    switch (baud_rate) {
    case 9600:
        speed = B9600;
        break;
    case 19200:
        speed = B19200;
        break;
    case 38400:
        speed = B38400;
        break;
    case 57600:
        speed = B57600;
        break;
    case 115200:
        speed = B115200;
        break;
    default:
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::unsupported, "unsupported serial baud rate"));
    }

    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::io, std::strerror(errno)));
    }

    return roberta::core::ok_status();
}

[[nodiscard]] roberta::core::Status write_all(int fd, std::span<const std::uint8_t> bytes)
{
    std::size_t written_total = 0;
    while (written_total < bytes.size()) {
        const auto written = ::write(fd, bytes.data() + written_total, bytes.size() - written_total);
        if (written < 0) {
            return std::unexpected(roberta::core::make_error(
                roberta::core::ErrorCode::io, std::strerror(errno)));
        }
        written_total += static_cast<std::size_t>(written);
    }
    return roberta::core::ok_status();
}

[[nodiscard]] roberta::core::Result<std::vector<std::uint8_t>> read_exact(
    int fd, std::size_t expected_bytes, std::chrono::milliseconds timeout)
{
    std::vector<std::uint8_t> response;
    response.reserve(expected_bytes);

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (response.size() < expected_bytes && std::chrono::steady_clock::now() < deadline) {
        std::uint8_t buffer[256];
        const auto count = ::read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return std::unexpected(roberta::core::make_error(
                roberta::core::ErrorCode::io, std::strerror(errno)));
        }
        if (count == 0) {
            continue;
        }
        response.insert(response.end(), buffer, buffer + count);
    }

    if (response.size() < expected_bytes) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::timeout, "serial receive timeout"));
    }

    response.resize(expected_bytes);
    return response;
}

[[nodiscard]] roberta::core::Result<std::string> read_line(int fd, std::chrono::milliseconds timeout)
{
    std::string response;
    bool saw_any_byte = false;
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        char ch{};
        const auto count = ::read(fd, &ch, 1);
        if (count < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return std::unexpected(roberta::core::make_error(
                roberta::core::ErrorCode::io, std::strerror(errno)));
        }
        if (count == 0) {
            continue;
        }
        saw_any_byte = true;
        if (ch == '\n') {
            break;
        }
        if (ch != '\r') {
            response.push_back(ch);
        }
    }

    if (!saw_any_byte) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::timeout, "serial line timeout"));
    }

    return response;
}

void drain_pending_input(int fd)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (std::chrono::steady_clock::now() < deadline) {
        std::uint8_t buffer[256];
        const auto count = ::read(fd, buffer, sizeof(buffer));
        if (count <= 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                break;
            }
            usleep(1000);
            continue;
        }
    }
}

roberta::core::Status prime_console_prompt(int fd)
{
    static constexpr std::string_view k_prime = "\r\n";
    const auto bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(k_prime.data()), k_prime.size());
    auto status = write_all(fd, bytes);
    if (!status) {
        return status;
    }
    tcdrain(fd);
    drain_pending_input(fd);
    return roberta::core::ok_status();
}

[[nodiscard]] std::string trim_prompt(std::string response)
{
    const std::string_view prompt = "axis> ";
    if (response.rfind(prompt, 0) == 0) {
        response.erase(0, prompt.size());
    }
    return response;
}

[[nodiscard]] bool is_ignorable_console_line(std::string_view line)
{
    if (line.empty()) {
        return true;
    }

    if (line == "axis>" || line == "axis> ") {
        return true;
    }

    return false;
}

} // namespace

PosixSerialTransport::PosixSerialTransport(SerialPortConfig config)
    : config_(std::move(config))
{
}

PosixSerialTransport::~PosixSerialTransport()
{
    close();
}

roberta::core::Status PosixSerialTransport::open()
{
    fd_ = ::open(config_.device_path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::io, std::strerror(errno)));
    }
    auto status = configure_port();
    if (!status) {
        close();
        return status;
    }
    drain_pending_input(fd_);
    return roberta::core::ok_status();
}

void PosixSerialTransport::close() noexcept
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool PosixSerialTransport::is_open() const noexcept
{
    return fd_ >= 0;
}

roberta::core::Status PosixSerialTransport::configure_port() const
{
    return set_common_attributes(fd_, config_.baud_rate);
}

roberta::core::Result<std::vector<std::uint8_t>> PosixSerialTransport::transact(const FrameExchange& exchange)
{
    if (!is_open()) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::failed_precondition, "serial transport is not open"));
    }

    auto status = write_all(fd_, exchange.request);
    if (!status) {
        return std::unexpected(status.error());
    }

    tcdrain(fd_);
    if (exchange.expected_response_size == 0) {
        return std::vector<std::uint8_t>{};
    }

    return read_exact(fd_, exchange.expected_response_size, exchange.timeout_policy.request_timeout);
}

PosixTextTransport::PosixTextTransport(SerialPortConfig config)
    : config_(std::move(config))
{
}

PosixTextTransport::~PosixTextTransport()
{
    close();
}

roberta::core::Status PosixTextTransport::open()
{
    fd_ = ::open(config_.device_path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::io, std::strerror(errno)));
    }
    auto status = configure_port();
    if (!status) {
        close();
        return status;
    }
    drain_pending_input(fd_);
    status = prime_console_prompt(fd_);
    if (!status) {
        close();
        return status;
    }
    return roberta::core::ok_status();
}

void PosixTextTransport::close() noexcept
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool PosixTextTransport::is_open() const noexcept
{
    return fd_ >= 0;
}

roberta::core::Status PosixTextTransport::configure_port() const
{
    return set_common_attributes(fd_, config_.baud_rate);
}

roberta::core::Result<std::string> PosixTextTransport::transact(const TextExchange& exchange)
{
    if (!is_open()) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::failed_precondition, "text serial transport is not open"));
    }

    const auto request = exchange.request_line + "\r\n";
    const auto bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(request.data()), request.size());

    auto status = write_all(fd_, bytes);
    if (!status) {
        return std::unexpected(status.error());
    }

    tcdrain(fd_);
    const auto deadline = std::chrono::steady_clock::now() + exchange.timeout;
    std::string collected;
    while (std::chrono::steady_clock::now() < deadline) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (remaining <= std::chrono::milliseconds::zero()) {
            break;
        }

        auto line = read_line(fd_, remaining);
        if (!line) {
            return std::unexpected(line.error());
        }

        auto trimmed = trim_prompt(std::move(*line));
        if (is_ignorable_console_line(trimmed)) {
            continue;
        }

        collected = std::move(trimmed);

        auto quiet_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        while (std::chrono::steady_clock::now() < quiet_deadline) {
            auto quiet_remaining = std::chrono::duration_cast<std::chrono::milliseconds>(quiet_deadline - std::chrono::steady_clock::now());
            if (quiet_remaining <= std::chrono::milliseconds::zero()) {
                break;
            }

            auto extra_line = read_line(fd_, quiet_remaining);
            if (!extra_line) {
                break;
            }

            auto extra_trimmed = trim_prompt(std::move(*extra_line));
            if (is_ignorable_console_line(extra_trimmed)) {
                continue;
            }

            collected.append("\n");
            collected.append(extra_trimmed);
            quiet_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        }

        drain_pending_input(fd_);
        return collected;
    }

    return std::unexpected(roberta::core::make_error(
        roberta::core::ErrorCode::timeout, "serial line timeout"));
}

} // namespace roberta::transport
