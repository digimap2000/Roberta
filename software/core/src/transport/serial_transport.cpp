#include <roberta/transport/serial_transport.hpp>

#include <cerrno>
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
        if (ch == '\n') {
            break;
        }
        if (ch != '\r') {
            response.push_back(ch);
        }
    }

    if (response.empty()) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::timeout, "serial line timeout"));
    }

    return response;
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
    return configure_port();
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
    return configure_port();
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

    const auto request = exchange.request_line + "\n";
    const auto bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(request.data()), request.size());

    auto status = write_all(fd_, bytes);
    if (!status) {
        return std::unexpected(status.error());
    }

    tcdrain(fd_);
    return read_line(fd_, exchange.timeout);
}

} // namespace roberta::transport
