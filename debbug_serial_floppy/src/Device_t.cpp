#include "Device_t.hpp"
#include <algorithm>

namespace Device {

Device_t::Device_t(const std::string& version)
    : version_(version) {
    // Constructor: solo guarda la versión
}

Device_t::~Device_t() {
    closePort();
}

int Device_t::baudToConstant(int baudrate) const {
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

bool Device_t::openPort() {
    if (fd_ >= 0) {
        std::cerr << "Puerto ya abierto\n";
        return false;
    }

    fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        std::cerr << "Error abriendo " << port_ << ": " << strerror(errno) << "\n";
        return false;
    }

    if (!configurePort(baudrate_)) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    return true;
}

void Device_t::closePort() {
    if (fd_ >= 0) {
        if (has_old_tio_) {
            tcsetattr(fd_, TCSANOW, &old_tio_);
        }
        ::close(fd_);
        fd_ = -1;
        has_old_tio_ = false;
    }
}

bool Device_t::configurePort(int baudrate) {
    struct termios tio{};

    // Guardar configuración original
    if (tcgetattr(fd_, &old_tio_) == 0) {
        has_old_tio_ = true;
    }

    // Configurar: 8N1, raw mode, sin flow control
    tio.c_cflag = baudToConstant(baudrate) | CS8 | CLOCAL | CREAD;
    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;  // Raw mode: sin echo, sin canonical, sin señales

    // VMIN=0, VTIME=0 -> non-blocking (manejamos timeout con select)
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    tcflush(fd_, TCIOFLUSH);

    if (tcsetattr(fd_, TCSANOW, &tio) != 0) {
        std::cerr << "Error configurando puerto: " << strerror(errno) << "\n";
        return false;
    }

    return true;
}

ssize_t Device_t::writePort(const uint8_t* data, size_t len) {
    if (fd_ < 0) return -1;
    return ::write(fd_, data, len);
}

ssize_t Device_t::writePort(const std::string& str) {
    return writePort(reinterpret_cast<const uint8_t*>(str.c_str()), str.size());
}

ssize_t Device_t::readPort(uint8_t* buffer, size_t max_len, int timeout_ms) {
    if (fd_ < 0) return -1;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(fd_ + 1, &fds, nullptr, nullptr, &tv);
    if (ret < 0) {
        if (errno == EINTR) return 0;
        std::cerr << "Error en select: " << strerror(errno) << "\n";
        return -1;
    }
    if (ret == 0) {
        return 0;  // Timeout
    }

    return ::read(fd_, buffer, max_len);
}

std::string Device_t::readLine(int timeout_ms) {
    std::string line;
    uint8_t ch;
    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeout_ms) break;

        int remaining = timeout_ms - elapsed;
        ssize_t n = readPort(&ch, 1, remaining);
        if (n == 1) {
            if (ch == '\n') break;
            if (ch != '\r') line.push_back(static_cast<char>(ch));
        } else if (n < 0) {
            break;
        }
    }
    return line;
}

std::vector<std::string> Device_t::sendCommand(const std::string& cmd,
                                                const std::string& prompt,
                                                int timeout_ms) {
    std::vector<std::string> lines;
    std::string effective_prompt = prompt.empty() ? prompt_ : prompt;
    int effective_timeout = (timeout_ms > 0) ? timeout_ms : timeout_ms_;

    // Enviar comando con CR+LF
    writePort(cmd + "\r\n");

    auto start = std::chrono::steady_clock::now();
    std::string line;

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= effective_timeout) break;

        int remaining = effective_timeout - elapsed;
        line = readLine(remaining);

        if (!line.empty()) {
            if (line.find(effective_prompt) != std::string::npos) {
                break;  // Prompt detectado, comando completado
            }
            lines.push_back(line);
        }
    }

    return lines;
}

void Device_t::flushPort() {
    if (fd_ >= 0) {
        tcflush(fd_, TCIOFLUSH);
    }
}

bool Device_t::parseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return false;
        } else if (arg == "--version") {
            show_version_ = true;
            return false;
        } else if (arg == "-l" || arg == "--list") {
            list_ports_ = true;
        } else if (arg == "-b" || arg == "--baud") {
            if (++i < argc) baudrate_ = std::atoi(argv[i]);
        } else if (arg == "-t" || arg == "--timeout") {
            if (++i < argc) timeout_ms_ = std::atoi(argv[i]);
        } else if (arg == "-p" || arg == "--prompt") {
            if (++i < argc) prompt_ = argv[i];
        } else if (arg[0] != '-') {
            if (port_.empty()) {
                port_ = arg;
            } else {
                // Resto son el comando
                single_command_ = arg;
                for (int j = i + 1; j < argc; ++j) {
                    single_command_ += " ";
                    single_command_ += argv[j];
                }
                break;
            }
        }
    }

    if (show_version_) {
        std::cout << "pico_serial_debug v" << version_ << "\n";
        return false;
    }

    if (list_ports_) {
        auto ports = listSerialPorts();
        if (ports.empty()) {
            std::cout << "No serial ports found\n";
        } else {
            std::cout << "Available serial ports:\n";
            for (const auto& p : ports) {
                std::cout << "  " << p << "\n";
            }
        }
        return false;
    }

    if (port_.empty()) {
        std::cerr << "Error: Puerto serie requerido\n\n";
        printUsage(argv[0]);
        return false;
    }

    interactive_ = single_command_.empty();
    return true;
}

void Device_t::printUsage(const char* prog) const {
    std::cout << "Uso: " << prog << " [opciones] <puerto> [comando]\n\n";
    std::cout << "Opciones:\n";
    std::cout << "  -b, --baud <rate>     Baudrate (default: 115200)\n";
    std::cout << "  -t, --timeout <ms>    Timeout lectura (default: 3000)\n";
    std::cout << "  -p, --prompt <str>    Prompt esperado (default: \"floppydisk> \")\n";
    std::cout << "  -l, --list            Lista puertos serie disponibles\n";
    std::cout << "  --version             Muestra versión y sale\n";
    std::cout << "  -h, --help            Muestra esta ayuda\n\n";
    std::cout << "Modos:\n";
    std::cout << "  " << prog << " /dev/ttyACM1                    # Modo interactivo\n";
    std::cout << "  " << prog << " /dev/ttyACM1 sdtest              # Comando único\n";
    std::cout << "  " << prog << " /dev/ttyACM1 \"help\"              # Comando con espacios\n\n";
    std::cout << "Ejemplos:\n";
    std::cout << "  " << prog << " /dev/ttyACM1\n";
    std::cout << "  " << prog << " -b 115200 /dev/ttyACM1 sdtest\n";
    std::cout << "  " << prog << " --list\n";
}

std::vector<std::string> Device_t::listSerialPorts() const {
    std::vector<std::string> ports;
    const char* patterns[] = {"/dev/ttyACM*", "/dev/ttyUSB*", "/dev/ttyS*", nullptr};
    
    for (int i = 0; patterns[i]; ++i) {
        std::string cmd = "ls " + std::string(patterns[i]) + " 2>/dev/null";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[256];
            while (fgets(buf, sizeof(buf), pipe)) {
                std::string port = buf;
                port.erase(port.find_last_not_of("\n\r") + 1);
                if (!port.empty()) ports.push_back(port);
            }
            pclose(pipe);
        }
    }
    return ports;
}

int Device_t::runSingleCommand() {
    if (!openPort()) return 1;

    std::cout << "Conectado a " << port_ << " @ " << baudrate_ << " baud\n";

    // Espera estabilización del puerto (crítico para USB CDC)
    flushPort();
    usleep(500000);  // 500ms para estabilizar USB CDC
    readLine(1000);  // Descarta cualquier dato inicial

    auto lines = sendCommand(single_command_);
    for (const auto& line : lines) {
        std::cout << line << "\n";
    }

    return 0;
}

int Device_t::runInteractive() {
    if (!openPort()) return 1;

    std::cout << "Conectado a " << port_ << " @ " << baudrate_ << " baud\n";
    std::cout << "Modo interactivo. Escribe 'exit' o 'quit' para salir.\n";
    std::cout << "Comandos especiales: !list, !baud <rate>, !timeout <ms>, !prompt <str>\n\n";

    // Flush inicial y descartar datos pendientes (crítico para USB CDC)
    flushPort();
    usleep(500000);  // 500ms para estabilizar USB CDC
    readLine(1000);  // Descarta cualquier dato inicial

    std::string input;
    while (true) {
        std::cout << "\033[1;32mpico>\033[0m ";
        std::cout.flush();
        
        if (!std::getline(std::cin, input)) break;

        input = trim(input);
        if (input.empty()) continue;
        if (input == "exit" || input == "quit") break;

        if (processSpecialCommand(input)) continue;

        auto lines = sendCommand(input);
        for (const auto& line : lines) {
            std::cout << line << "\n";
        }
    }

    std::cout << "\nDesconectando...\n";
    return 0;
}

void Device_t::printMenu() const {
    std::cout << "\nComandos predefinidos:\n";
    for (const auto& cmd : commands_) {
        std::cout << "  " << cmd.key << ") " << cmd.cmd;
        std::cout << std::string(12 - strlen(cmd.cmd), ' ') << "- " << cmd.desc << "\n";
    }
    std::cout << "\n  c) Comando personalizado\n";
    std::cout << "  m) Monitor continuo (Ctrl+C para salir)\n";
    std::cout << "  r) Reconectar puerto\n";
    std::cout << "  q) Salir\n\n";
}

bool Device_t::processSpecialCommand(const std::string& input) {
    if (input == "!list") {
        auto ports = listSerialPorts();
        for (const auto& p : ports) std::cout << "  " << p << "\n";
        return true;
    }
    if (input.rfind("!baud ", 0) == 0) {
        int new_baud = std::atoi(input.substr(6).c_str());
        std::cout << "Cambiando baudrate a " << new_baud << "...\n";
        closePort();
        baudrate_ = new_baud;
        if (!openPort()) return true;
        return true;
    }
    if (input.rfind("!timeout ", 0) == 0) {
        timeout_ms_ = std::atoi(input.substr(9).c_str());
        std::cout << "Timeout: " << timeout_ms_ << "ms\n";
        return true;
    }
    if (input.rfind("!prompt ", 0) == 0) {
        prompt_ = input.substr(8);
        std::cout << "Prompt: '" << prompt_ << "'\n";
        return true;
    }
    if (input == "!menu" || input == "!help") {
        printMenu();
        return true;
    }
    return false;
}

std::string Device_t::toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return result;
}

std::string Device_t::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

int Device_t::run(int argc, char* argv[]) {
    // Mostrar versión al iniciar
    std::cout << "pico_serial_debug v" << version_ << "\n";

    if (!parseArgs(argc, argv)) {
        return show_version_ || list_ports_ ? 0 : 1;
    }

    if (interactive_) {
        return runInteractive();
    } else {
        return runSingleCommand();
    }
}

} // namespace Device