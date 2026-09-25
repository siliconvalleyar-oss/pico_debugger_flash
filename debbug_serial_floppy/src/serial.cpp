#include "serial.h"
#include <cstring>
#include <cerrno>
#include <iostream>

SerialPort::SerialPort() = default;

SerialPort::~SerialPort() {
    close();
}

SerialPort::SerialPort(SerialPort&& other) noexcept
    : fd_(other.fd_), old_tio_(other.old_tio_), has_old_tio_(other.has_old_tio_) {
    other.fd_ = -1;
    other.has_old_tio_ = false;
}

SerialPort& SerialPort::operator=(SerialPort&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        old_tio_ = other.old_tio_;
        has_old_tio_ = other.has_old_tio_;
        other.fd_ = -1;
        other.has_old_tio_ = false;
    }
    return *this;
}

int SerialPort::baudToConstant(int baudrate) const {
    switch (baudrate) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 921600: return B921600;
        default:     return B115200;
    }
}

bool SerialPort::open(const std::string& device, int baudrate) {
    if (isOpen()) {
        std::cerr << "Port already open\n";
        return false;
    }

    fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        std::cerr << "Error opening " << device << ": " << strerror(errno) << "\n";
        return false;
    }

    if (!configure(baudrate)) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    return true;
}

void SerialPort::close() {
    if (fd_ >= 0) {
        if (has_old_tio_) {
            tcsetattr(fd_, TCSANOW, &old_tio_);
        }
        ::close(fd_);
        fd_ = -1;
        has_old_tio_ = false;
    }
}

bool SerialPort::configure(int baudrate) {
    struct termios tio{};

    if (tcgetattr(fd_, &old_tio_) == 0) {
        has_old_tio_ = true;
    }

    tio.c_cflag = baudToConstant(baudrate) | CS8 | CLOCAL | CREAD;
    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;  // Raw mode

    // VMIN=0, VTIME=0 -> non-blocking read (we handle timeout manually)
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    tcflush(fd_, TCIOFLUSH);

    if (tcsetattr(fd_, TCSANOW, &tio) != 0) {
        std::cerr << "Error configuring port: " << strerror(errno) << "\n";
        return false;
    }

    return true;
}

ssize_t SerialPort::write(const uint8_t* data, size_t len) {
    if (!isOpen()) return -1;
    return ::write(fd_, data, len);
}

ssize_t SerialPort::write(const std::string& str) {
    return write(reinterpret_cast<const uint8_t*>(str.c_str()), str.size());
}

ssize_t SerialPort::read(uint8_t* buffer, size_t max_len, int timeout_ms) {
    if (!isOpen()) return -1;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(fd_ + 1, &fds, nullptr, nullptr, &tv);
    if (ret < 0) {
        if (errno == EINTR) return 0;
        std::cerr << "Select error: " << strerror(errno) << "\n";
        return -1;
    }
    if (ret == 0) {
        return 0;  // Timeout
    }

    return ::read(fd_, buffer, max_len);
}

std::string SerialPort::readLine(int timeout_ms) {
    std::string line;
    uint8_t ch;
    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeout_ms) break;

        int remaining = timeout_ms - elapsed;
        ssize_t n = read(&ch, 1, remaining);
        if (n == 1) {
            if (ch == '\n') break;
            if (ch != '\r') line.push_back(static_cast<char>(ch));
        } else if (n < 0) {
            break;
        }
    }
    return line;
}

std::vector<std::string> SerialPort::sendCommand(const std::string& cmd,
                                                  const std::string& prompt,
                                                  int timeout_ms) {
    std::vector<std::string> lines;

    // Send command with CR+LF
    write(cmd + "\r\n");

    auto start = std::chrono::steady_clock::now();
    std::string line;

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeout_ms) break;

        int remaining = timeout_ms - elapsed;
        line = readLine(remaining);

        if (!line.empty()) {
            if (line.find(prompt) != std::string::npos) {
                break;  // Got prompt, command complete
            }
            lines.push_back(line);
        }
    }

    return lines;
}

void SerialPort::flush() {
    if (isOpen()) {
        tcflush(fd_, TCIOFLUSH);
    }
}