#include "serial.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <csignal>
#include <algorithm>

static volatile bool g_running = true;

void signalHandler(int) {
    g_running = false;
}

void printUsage(const char* prog) {
    std::cout << "Uso: " << prog << " [opciones] <puerto> [comando]\n\n";
    std::cout << "Opciones:\n";
    std::cout << "  -b, --baud <rate>   Baudrate (default: 115200)\n";
    std::cout << "  -t, --timeout <ms>  Timeout lectura (default: 3000)\n";
    std::cout << "  -p, --prompt <str>  Prompt esperado (default: \"floppydisk> \")\n";
    std::cout << "  -l, --list          Lista puertos serie disponibles\n";
    std::cout << "  -h, --help          Muestra esta ayuda\n\n";
    std::cout << "Modos:\n";
    std::cout << "  " << prog << " /dev/ttyACM0                    # Modo interactivo\n";
    std::cout << "  " << prog << " /dev/ttyACM0 sdtest             # Comando único\n";
    std::cout << "  " << prog << " /dev/ttyACM0 \"help\"             # Comando con espacios\n\n";
    std::cout << "Ejemplos:\n";
    std::cout << "  " << prog << " /dev/ttyACM0\n";
    std::cout << "  " << prog << " -b 115200 /dev/ttyACM0 sdtest\n";
    std::cout << "  " << prog << " --list\n";
}

std::vector<std::string> listSerialPorts() {
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

int main(int argc, char* argv[]) {
    std::string port;
    int baudrate = 115200;
    int timeout_ms = 3000;
    std::string prompt = "floppydisk> ";
    std::string command;
    bool list_ports = false;
    bool interactive = false;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-l" || arg == "--list") {
            list_ports = true;
        } else if (arg == "-b" || arg == "--baud") {
            if (++i < argc) baudrate = std::atoi(argv[i]);
        } else if (arg == "-t" || arg == "--timeout") {
            if (++i < argc) timeout_ms = std::atoi(argv[i]);
        } else if (arg == "-p" || arg == "--prompt") {
            if (++i < argc) prompt = argv[i];
        } else if (arg[0] != '-') {
            if (port.empty()) {
                port = arg;
            } else {
                // Remaining args are the command
                command = arg;
                for (int j = i + 1; j < argc; ++j) {
                    command += " ";
                    command += argv[j];
                }
                break;
            }
        }
    }

    if (list_ports) {
        auto ports = listSerialPorts();
        if (ports.empty()) {
            std::cout << "No serial ports found\n";
        } else {
            std::cout << "Available serial ports:\n";
            for (const auto& p : ports) {
                std::cout << "  " << p << "\n";
            }
        }
        return 0;
    }

    if (port.empty()) {
        std::cerr << "Error: Puerto serie requerido\n\n";
        printUsage(argv[0]);
        return 1;
    }

    if (command.empty()) {
        interactive = true;
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    SerialPort serial;
    if (!serial.open(port, baudrate)) {
        return 1;
    }

    std::cout << "Conectado a " << port << " @ " << baudrate << " baud\n";
    if (interactive) {
        std::cout << "Modo interactivo. Escribe 'exit' o 'quit' para salir.\n";
        std::cout << "Comandos especiales: !list, !baud <rate>, !timeout <ms>, !prompt <str>\n\n";
    }

    // Initial flush and get prompt
    serial.flush();
    usleep(100000);
    serial.readLine(500);  // Discard any pending data

    if (!interactive) {
        // Single command mode
        auto lines = serial.sendCommand(command, prompt, timeout_ms);
        for (const auto& line : lines) {
            std::cout << line << "\n";
        }
        return 0;
    }

    // Interactive mode
    std::string input;
    while (g_running) {
        std::cout << "\033[1;32mpico>\033[0m ";
        std::cout.flush();
        
        if (!std::getline(std::cin, input)) break;
        if (!g_running) break;

        // Trim
        input.erase(0, input.find_first_not_of(" \t\r\n"));
        input.erase(input.find_last_not_of(" \t\r\n") + 1);
        
        if (input.empty()) continue;
        if (input == "exit" || input == "quit") break;

        // Special commands
        if (input == "!list") {
            auto ports = listSerialPorts();
            for (const auto& p : ports) std::cout << "  " << p << "\n";
            continue;
        }
        if (input.rfind("!baud ", 0) == 0) {
            baudrate = std::atoi(input.substr(6).c_str());
            std::cout << "Changing baudrate to " << baudrate << "...\n";
            serial.close();
            if (!serial.open(port, baudrate)) return 1;
            continue;
        }
        if (input.rfind("!timeout ", 0) == 0) {
            timeout_ms = std::atoi(input.substr(9).c_str());
            std::cout << "Timeout set to " << timeout_ms << "ms\n";
            continue;
        }
        if (input.rfind("!prompt ", 0) == 0) {
            prompt = input.substr(8);
            std::cout << "Prompt set to: '" << prompt << "'\n";
            continue;
        }

        // Send command
        auto lines = serial.sendCommand(input, prompt, timeout_ms);
        for (const auto& line : lines) {
            std::cout << line << "\n";
        }
    }

    std::cout << "\nDesconectando...\n";
    serial.close();
    return 0;
}