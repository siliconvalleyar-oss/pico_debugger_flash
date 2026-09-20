# SKILL.md — Habilidad: puente BLE→USB HID (pico_keyboard_bridge)

> Si la tarea involucra compilar/flashear el puente **keyboard** desde este repo
> (rama `keyboard`), carga este skill para hacerlo **para no romper 4 scripts a
> la vez**: los 4 `scripts/flash_nosudo*multi.sh` heredan la MISMA cadena.

## Antes de tocar nada (3 verificaciones Verified, cuestan 3× en esta sesión)
1. **`cycle_check` en disco**: `PROJECT=keyboard` en `scripts/config.sh`
   (Verified heredado por los 4 flash). Si `PROJECT` no es `keyboard`, el
   puente no se compila: **no toques `config.sh` hasta confirmarlo en disco.**
2. **Ninja instalado**: `ninja --version` (Verified instalado). Si falta,
   `apt install ninja-build` — pero en esta rama YA está.
3. **`scripts/build.sh` EXISTE** (era la dependencia real faltante — los 4
   flash lo invocan en la línea ~135). Con él + `config.sh` la cadena es
   completa.

## Cadena de build REAL (lo que YA está en disco)
```
config.sh ──PROJECT=keyboard──▶ build.sh ──ninja -C keyboard/build──▶ ELF
                                    ▲ heredado por flash_nosudo*multi.sh
```
- `config.sh`: `PROJECT_DIR=${REPO_ROOT}/keyboard`, target
  `pico_keyboard_bridge`.
- `keyboard/src/CMakeLists.txt` (puente 1:1 doorbell Verified): cyw43_arch se
  enlaza en el **executable** (`pico_cyw43_arch_none`), con
  `pico_cyw43_arch_none` en `target_link_libraries` del bridge, NO en una lib
  STATIC aparte (esa fue la desobediencia que dejó `pico/cyw43_arch.h` sin
  llegar a main.c).

## El flasheo — la ÚNICA promesa que manda (lección 3× esta sesión)
> **NO se flashea sin**
> `ninja=0` sobre `keyboard/build` **y** `[100%] Built target
> pico_keyboard_bridge` (o línea `Built target`) **y** ELF > 150 KB en
> `keyboard/build` (o la build que config hereda).
> Si ninja=1 o ELF < 150 KB: **se dice con honestidad** — jamás inventar un
> `[100%]` (3 heredocs rotos ≠ firmware, doorbell Verified 100% es LA prueba).

Con esas 3 condiciones + ELF>150KB → SWD local (probe `2e8a:000c`):

```bash
./scripts/flash_nosudo_multi.sh keyboard/
```

## Si algo falla (consejo de la sesión, pagado en heredocs)
- Error `pico/cyw43_arch.h: No such file`: cyw43_arch va en el executable
  (patrón doorbell), no en la lib estática.
- Error de doorbell.h: el GATT se genera con
  `pico_btstack_make_gatt_header` y el .gatt en la MISMA dir que CMakeLists
  apunta (doorbell.gatt Verified 106B en `keyboard/src/`).
- **Nunca escribir el CMakeLists heredoc a ciegas**: copiar 1:1 del
  `ble_doorbell` del SDK (`pico-examples/bluetooth/ble_doorbell`) — ese es el
  Verified que compiló `[100%]`.
