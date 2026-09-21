#pragma once

#include <cstdint>
#include <cstddef>

/// Un único evento de teclado a emitir por USB HID.
/// keycode == 0 con delay_ms > 0 representa un "solo esperar" (\SLEEP).
struct KeyEvent {
    uint8_t modifier;  ///< Máscara de modificadores HID (Ctrl/Shift/Alt/GUI)
    uint8_t keycode;   ///< Usage ID HID del teclado (0 = ninguna tecla)
    uint16_t delay_ms; ///< Retardo sugerido antes de soltar la tecla / entre eventos
};

/// Buffer circular thread-safe-lite (single producer / single consumer)
/// para desacoplar la llegada de datos BLE (ráfagas) del envío USB HID
/// (limitado por el polling interval del host, ~1-8ms).
class KeyQueue {
public:
    static constexpr size_t kCapacity = 256;

    KeyQueue();

    /// Encola un evento. Devuelve false si la cola está llena (se descarta
    /// el evento más nuevo; no se bloquea nunca).
    bool push(const KeyEvent &ev);

    /// Extrae el siguiente evento. Devuelve false si la cola está vacía.
    bool pop(KeyEvent &out);

    bool empty() const;
    bool full() const;
    size_t size() const;

private:
    KeyEvent buffer_[kCapacity];
    volatile size_t head_; ///< índice de próxima escritura
    volatile size_t tail_; ///< índice de próxima lectura
    volatile size_t count_;
};
