#pragma once

#include <memory>
#include <string>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <cstring>
#include <cerrno>

/**
 * @namespace Device
 * @brief Espacio de nombres para la aplicación de depuración serie del Pico
 */
namespace Device {

/**
 * @class Device_t
 * @brief Clase principal para depuración de comunicación serie USB con Raspberry Pi Pico
 * 
 * Proporciona funcionalidad para:
 * - Abrir y configurar puertos serie (termios, 8N1, raw mode)
 * - Enviar comandos y recibir respuestas con timeout
 * - Modo interactivo y modo comando único
 * - Detección automática de puertos disponibles
 * - Gestión de versión en tiempo de compilación
 */
class Device_t {
public:
    /**
     * @brief Constructor
     * @param version Versión de la aplicación (definida en tiempo de compilación)
     */
    explicit Device_t(const std::string& version);

    /**
     * @brief Destructor - cierra el puerto si está abierto
     */
    ~Device_t();

    // No copyable
    Device_t(const Device_t&) = delete;
    Device_t& operator=(const Device_t&) = delete;

    // Movable
    Device_t(Device_t&&) noexcept = default;
    Device_t& operator=(Device_t&&) noexcept = default;

    /**
     * @brief Ejecuta la aplicación principal
     * @param argc Número de argumentos
     * @param argv Array de argumentos
     * @return Código de salida (0 = éxito)
     */
    int run(int argc, char* argv[]);

private:
    // Configuración
    std::string version_;           ///< Versión de la aplicación
    std::string port_;              ///< Puerto serie (ej. /dev/ttyACM1)
    int baudrate_ = 115200;         ///< Baudrate
    int timeout_ms_ = 3000;         ///< Timeout de lectura en ms
    std::string prompt_ = "floppydisk> "; ///< Prompt esperado del shell Pico
    bool interactive_ = false;      ///< Modo interactivo
    std::string single_command_;    ///< Comando único a ejecutar
    bool list_ports_ = false;       ///< Solo listar puertos
    bool show_version_ = false;     ///< Mostrar versión y salir

    // Estado del puerto serie
    int fd_ = -1;                   ///< File descriptor del puerto
    struct termios old_tio_;        ///< Configuración original del terminal
    bool has_old_tio_ = false;      ///< Flag si se guardó configuración original

    // Comandos predefinidos para el shell floppydisk_usb
    struct Command {
        const char* key;
        const char* cmd;
        const char* desc;
    };
    
    static constexpr Command commands_[8] = {
        {"1", "help",       "Muestra ayuda de comandos del shell Pico"},
        {"2", "sdtest",     "Test completo microSD (CMD0, CMD8, ACMD41, CMD58, CMD16, CMD9, CMD17)"},
        {"3", "fat",        "Info sistema archivos FAT (libre/total KB)"},
        {"4", "ls",         "Lista imágenes en /IMG"},
        {"5", "version",    "Versión firmware, build y commit git"},
        {"6", "reboot",     "Reinicia el Pico"},
        {"7", "echo on",    "Activa eco de comandos"},
        {"8", "echo off",   "Desactiva eco de comandos"}
    };

    // --- Métodos de configuración y parsing ---

    /**
     * @brief Parsea argumentos de línea de comandos
     * @param argc Número de argumentos
     * @param argv Array de argumentos
     * @return true si parsing exitoso, false si error o --version/--help
     */
    bool parseArgs(int argc, char* argv[]);

    /**
     * @brief Imprime uso del programa
     * @param prog Nombre del programa
     */
    void printUsage(const char* prog) const;

    /**
     * @brief Lista puertos serie disponibles en el sistema
     * @return Vector con rutas de puertos encontrados
     */
    std::vector<std::string> listSerialPorts() const;

    // --- Métodos de puerto serie ---

    /**
     * @brief Convierte baudrate entero a constante termios
     * @param baudrate Baudrate (9600, 115200, etc.)
     * @return Constante termios (B115200, etc.)
     */
    int baudToConstant(int baudrate) const;

    /**
     * @brief Abre y configura el puerto serie
     * @return true si éxito, false si error
     */
    bool openPort();

    /**
     * @brief Cierra el puerto serie y restaura configuración original
     */
    void closePort();

    /**
     * @brief Configura el puerto serie (8N1, raw mode, sin flow control)
     * @param baudrate Baudrate a configurar
     * @return true si éxito
     */
    bool configurePort(int baudrate);

    /**
     * @brief Escribe datos al puerto serie
     * @param data Puntero a datos
     * @param len Longitud en bytes
     * @return Bytes escritos o -1 en error
     */
    ssize_t writePort(const uint8_t* data, size_t len);

    /**
     * @brief Escribe string al puerto serie
     * @param str String a enviar
     * @return Bytes escritos o -1 en error
     */
    ssize_t writePort(const std::string& str);

    /**
     * @brief Lee datos del puerto con timeout
     * @param buffer Buffer de destino
     * @param max_len Máximo bytes a leer
     * @param timeout_ms Timeout en milisegundos
     * @return Bytes leídos, 0 = timeout, -1 = error
     */
    ssize_t readPort(uint8_t* buffer, size_t max_len, int timeout_ms);

    /**
     * @brief Lee una línea (hasta \n o timeout)
     * @param timeout_ms Timeout en milisegundos
     * @return Línea leída (vacía si timeout/error)
     */
    std::string readLine(int timeout_ms);

    /**
     * @brief Envía comando y lee respuesta hasta detectar prompt
     * @param cmd Comando a enviar
     * @param prompt Prompt esperado (default: "floppydisk> ")
     * @param timeout_ms Timeout total en ms
     * @return Vector de líneas de respuesta (sin el prompt)
     */
    std::vector<std::string> sendCommand(const std::string& cmd,
                                          const std::string& prompt = "",
                                          int timeout_ms = -1);

    /**
     * @brief Limpia buffers de entrada/salida
     */
    void flushPort();

    // --- Métodos de modos de operación ---

    /**
     * @brief Ejecuta modo comando único
     * @return 0 si éxito
     */
    int runSingleCommand();

    /**
     * @brief Ejecuta modo interactivo
     * @return 0 si éxito
     */
    int runInteractive();

    /**
     * @brief Imprime menú de comandos predefinidos
     */
    void printMenu() const;

    /**
     * @brief Procesa comandos especiales internos (prefijo !)
     * @param input Línea de entrada
     * @return true si fue comando especial, false si comando normal
     */
    bool processSpecialCommand(const std::string& input);

    // --- Utilidades ---

    /**
     * @brief Convierte string a minúsculas
     * @param s String a convertir
     * @return String en minúsculas
     */
    static std::string toLower(const std::string& s);

    /**
     * @brief Trim de espacios en blanco
     * @param s String a recortar
     * @return String sin espacios al inicio/final
     */
    static std::string trim(const std::string& s);
};

} // namespace Device