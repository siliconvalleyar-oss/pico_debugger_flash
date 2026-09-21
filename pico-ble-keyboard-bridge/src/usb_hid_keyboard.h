#pragma once

#include <cstdint>
#include <cstddef>

/**
 * @file usb_hid_keyboard.h
 * @brief Emulación de teclado USB HID (Boot Protocol) usando TinyUSB.
 *
 * Expone una cola de eventos que se van consumiendo en cada iteración
 * del loop principal (usb_kbd_task), respetando el intervalo de polling
 * que soporta el host USB. Ninguna función bloquea ni hace polling activo
 * dentro de un callback de TinyUSB.
 */

/// Inicializa la pila TinyUSB en modo dispositivo HID teclado.
/// Debe llamarse una sola vez al arrancar, antes de usb_kbd_task().
void usb_kbd_init(void);

/// Tarea cooperativa de USB: hay que llamarla repetidamente desde el loop
/// principal (equivalente a tud_task() + drenado de la cola de teclas).
/// No bloquea.
void usb_kbd_task(void);

/// Encola la escritura de una cadena completa como si se tipeara tecla
/// por tecla. Si la cadena no entra en el espacio libre del buffer interno,
/// se trocea automáticamente: se encola lo que entra y se descarta el resto
/// (se retorna la cantidad de caracteres efectivamente encolados).
///
/// @param str Cadena ASCII terminada en NUL.
/// @return Cantidad de caracteres encolados (puede ser menor a strlen(str)).
size_t usb_kbd_type_string(const char *str);

/// Encola la pulsación de una tecla concreta con un modificador dado.
/// El "release" (soltar la tecla) se encola automáticamente después.
///
/// @param keycode  Usage ID HID (ver ascii_to_hid.h para constantes usuales).
/// @param modifier Máscara de modificadores HID (ver HidMod::*).
void usb_kbd_press(uint8_t keycode, uint8_t modifier);

/// Encola un reporte HID "todo en cero" (soltar todas las teclas).
/// Se usa automáticamente entre pulsaciones, pero también se expone
/// como API pública por si se necesita forzar un release manual.
void usb_kbd_release(void);

/// Encola un retardo puro (usado por el comando \SLEEP). No bloquea al
/// llamador: el delay se aplica en usb_kbd_task() antes de continuar
/// con el siguiente evento de la cola.
///
/// @param ms Milisegundos a esperar antes del próximo evento.
void usb_kbd_delay(uint16_t ms);
