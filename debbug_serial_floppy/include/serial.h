#pragma once

#include <string>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdint>
#include <chrono>
#include <vector>

class SerialPort {
public:
    explicit SerialPort();
    ~SerialPort();

    // No copy
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    // Move allowed
    SerialPort(SerialPort&& other) noexcept;
    SerialPort& operator=(SerialPort&& other) noexcept;

    bool open(const std::string& device, int baudrate = 115200);
    void close();

    bool isOpen() const { return fd_ >= 0; }

    // Configure port: 8N1, no flow control, raw mode
    bool configure(int baudrate);

    // Write bytes
    ssize_t write(const uint8_t* data, size_t len);
    ssize_t write(const std::string& str);

    // Read with timeout (milliseconds)
    // Returns number of bytes read, 0 on timeout, -1 on error
    ssize_t read(uint8_t* buffer, size_t max_len, int timeout_ms = 1000);

    // Read line (until \n or timeout)
    // Returns empty string on timeout/error
    std::string readLine(int timeout_ms = 1000);

    // Send command + newline, read response lines until prompt or timeout
    std::vector<std::string> sendCommand(const std::string& cmd, 
                                          const std::string& prompt = "floppydisk> ",
                                          int timeout_ms = 3000);

    // Flush buffers
    void flush();

    // Get file descriptor (for select/poll)
    int getFd() const { return fd_; }

private:
    int fd_ = -1;
    struct termios old_tio_;
    bool has_old_tio_ = false;

    int baudToConstant(int baudrate) const;
};