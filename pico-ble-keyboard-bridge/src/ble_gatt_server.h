#pragma once

/**
 * @file ble_gatt_server.h
 * @brief Servidor GATT BLE ("Pico-KB-Bridge") con bonding obligatorio.
 *
 * Expone un único característica de escritura por la que un dispositivo
 * ya emparejado (bonded) puede mandar texto o comandos especiales, que
 * se traducen a pulsaciones de teclado vía usb_hid_keyboard.h.
 *
 * SEGURIDAD: el emparejamiento usa "Just Works" (LE Secure Connections)
 * con bonding=true. Esto significa que:
 *   - La PRIMERA vez que un teléfono/PC se conecta, hay que confirmar el
 *     emparejamiento (según el cliente BLE, puede pedir una confirmación
 *     en pantalla, sin PIN visible).
 *   - Una vez emparejado (bonded), las claves quedan guardadas en la
 *     flash de la Pico y en el dispositivo remoto: las próximas conexiones
 *     son automáticas para ESE dispositivo puntual.
 *   - Dispositivos NO emparejados no pueden escribir en la característica:
 *     BTstack rechaza la escritura si el link no está encriptado/bonded.
 *
 * "Just Works" no protege contra un atacante activo en el momento exacto
 * del primer emparejamiento (man-in-the-middle), pero sí evita que
 * cualquier tercero se conecte más adelante sin haber estado presente
 * en ese primer pairing. Para uso doméstico/personal es un compromiso
 * razonable entre seguridad y comodidad.
 */

/// Inicializa BTstack, el stack BLE y el servidor GATT.
/// Debe llamarse una sola vez, después de que el radio (CYW43) esté listo.
void ble_gatt_server_init(void);

/// Arranca el advertising BLE (visibilidad como "Pico-KB-Bridge").
/// Se puede llamar de nuevo tras una desconexión para volver a ser visible.
void ble_gatt_server_start_advertising(void);
