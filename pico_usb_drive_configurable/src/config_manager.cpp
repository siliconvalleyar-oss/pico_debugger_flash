/*
 * config_manager.cpp - Runtime configuration (config.txt) and hot-plug watcher
 *
 * SPDX-License-Identifier: MIT
 *
 * Responsibilities:
 *   1. On first boot: if config.txt does not exist, create it (in the pendrive
 *      root) filled with the defaults from config.h.
 *   2. Parse config.txt allowing:
 *        - comment lines starting with '#'
 *        - whitespace around keys, '=' and values
 *        - key = value  (case-insensitive key names)
 *   3. Detect hot-plug changes: a "config watcher" re-reads the file every
 *      CONFIG_POLL_INTERVAL_MS comparing a simple content hash; on change it
 *      re-applies the settings (e.g. READ_ONLY toggling, volume label, OLED
 *      on/off) without needing a reboot.
 *
 * Uses FatFS for file access. Because the FAT layer is C, these functions are
 * compiled as C++ but all FatFS calls are extern "C" (ff.h already guards).
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "ff.h"
#include "config_manager.h"

extern "C" {
#include "bsp/board_api.h"
}

/* (re)apply a freshly parsed config into the global g_cfg struct. */
static void cfg_apply_defaults(const pendrive_cfg_t *src) {
    g_cfg = *src;                     // struct copy
    g_cfg.config_valid = true;
}

/*
 * A very light hash so we can detect if config.txt changed while mounted.
 * We do not need cryptographic security, only change detection.
 */
static uint32_t fnv1a_hash(const uint8_t *data, uint32_t len) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

/*
 * Read the whole config.txt into a RAM buffer and return its size/hash.
 * Returns < 0 if the file does not exist.
 */
static int config_read_file(char *buf, uint32_t bufsz, uint32_t *out_hash) {
    FIL f;
    FRESULT fr = f_open(&f, CONFIG_FILE, FA_READ);
    if (fr != FR_OK) return -1;

    unsigned br = 0;
    /* NOTE: f_size() may exceed bufsz; we cap the read. */
    uint32_t to_read = (uint32_t) f_size(&f);
    if (to_read > bufsz - 1) to_read = bufsz - 1;

    fr = f_read(&f, buf, to_read, &br);
    f_close(&f);
    if (fr != FR_OK) return -1;

    buf[br] = '\0';
    if (out_hash) *out_hash = fnv1a_hash((const uint8_t *) buf, br);
    return (int) br;
}

/*
 * Parse a "key=value" line, trimming spaces and skipping comments/blank lines.
 * Returns 1 if a key was parsed, 0 otherwise.
 * Writes the key (uppercased) and value (trimmed, may be empty).
 */
static int config_parse_line(const char *line, char *key, size_t key_sz,
                             char *value, size_t val_sz) {
    const char *p = line;
    /* skip leading spaces */
    while (*p && isspace((unsigned char) *p)) p++;

    /* blank or comment line */
    if (*p == '\0' || *p == '#') return 0;

    /* find '=' */
    const char *eq = strchr(p, '=');
    if (!eq) return 0;

    /* extract key (before '=') */
    size_t klen = (size_t)(eq - p);
    while (klen > 0 && isspace((unsigned char) p[klen - 1])) klen--;
    if (klen == 0 || klen >= key_sz) return 0;
    for (size_t i = 0; i < klen; i++) {
        key[i] = (char) toupper((unsigned char) p[i]);   // case-insensitive
    }
    key[klen] = '\0';

    /* extract value (after '=') */
    const char *v = eq + 1;
    while (*v && isspace((unsigned char) *v)) v++;
    size_t vlen = strlen(v);
    while (vlen > 0 && isspace((unsigned char) v[vlen - 1])) vlen--;
    while (vlen > 0 && (v[vlen - 1] == '\r' || v[vlen - 1] == '\n')) vlen--;
    if (vlen >= val_sz) vlen = val_sz - 1;
    memcpy(value, v, vlen);
    value[vlen] = '\0';

    return 1;
}

/*
 * Apply a parsed key/value pair into the runtime config struct.
 * Returns false if the key is unknown or the value is invalid (syntax error),
 * which is reported by the config-valid state and shown on the OLED.
 */
static bool config_apply_pair(const char *key, const char *value) {
    if (strcmp(key, "VOLUME_LABEL") == 0) {
        size_t n = strlen(value);
        if (n == 0 || n > 11) return false;               // FAT label max 11
        strncpy(g_cfg.volume_label, value, 11);
        g_cfg.volume_label[11] = '\0';
        return true;
    }
    if (strcmp(key, "READ_ONLY") == 0) {
        if (strcmp(value, "0") == 0) { g_cfg.read_only = 0; return true; }
        if (strcmp(value, "1") == 0) { g_cfg.read_only = 1; return true; }
        return false;
    }
    if (strcmp(key, "ENABLE_OLED") == 0) {
        if (strcmp(value, "0") == 0) { g_cfg.enable_oled = 0; return true; }
        if (strcmp(value, "1") == 0) { g_cfg.enable_oled = 1; return true; }
        return false;
    }
    if (strcmp(key, "LED_ON_CONNECT") == 0) {
        if (strcmp(value, "0") == 0) { g_cfg.led_on_connect = 0; return true; }
        if (strcmp(value, "1") == 0) { g_cfg.led_on_connect = 1; return true; }
        return false;
    }
    if (strcmp(key, "AUTO_MOUNT_DELAY_MS") == 0) {
        char *end;
        long v = strtol(value, &end, 10);
        if (end == value || v < 0 || v > 60000) return false;
        g_cfg.auto_mount_delay_ms = (uint32_t) v;
        return true;
    }
    /* Unknown key: report as error so the user sees it on the OLED. */
    return false;
}

/*
 * Default config.txt body (always written with CRLF so Windows Notepad is happy).
 */
#define DEFAULT_CONFIG_BODY \
    "# Pico Configurable USB Pendrive\r\n" \
    "# Edite estos valores; se aplican al reconectar o al detectar cambio.\r\n" \
    "# Las lineas que comienzan con '#' son comentarios.\r\n" \
    "VOLUME_LABEL=" CFG_DEFAULT_VOLUME_LABEL "\r\n" \
    "READ_ONLY=" STRINGIFY(CFG_DEFAULT_READ_ONLY) "\r\n" \
    "ENABLE_OLED=" STRINGIFY(CFG_DEFAULT_ENABLE_OLED) "\r\n" \
    "LED_ON_CONNECT=" STRINGIFY(CFG_DEFAULT_LED_ON_CONNECT) "\r\n" \
    "AUTO_MOUNT_DELAY_MS=" STRINGIFY(CFG_DEFAULT_AUTO_MOUNT_DELAY_MS) "\r\n"

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x)      STRINGIFY_IMPL(x)

/*
 * Create config.txt if missing. Steps on FAT:
 *   1. f_mount (to have a working FS).
 *   2. f_open with FA_CREATE_NEW; if FR_EXIST already exists -> nothing to do.
 *   3. Write the default body and close (f_close flushes).
 */
static void config_create_if_missing(void) {
    FIL f;
    FRESULT fr = f_open(&f, CONFIG_FILE, FA_CREATE_NEW | FA_WRITE);
    if (fr == FR_EXIST) {
        /* already there - nothing to do */
        return;
    }
    if (fr != FR_OK) return;   // FS not ready yet (formatted later)

    unsigned bw;
    f_write(&f, DEFAULT_CONFIG_BODY, strlen(DEFAULT_CONFIG_BODY), &bw);
    f_close(&f);               // -> f_sync'ed
}

/*
 * Reload config.txt from disk into g_cfg.
 * Fallbacks to config.h defaults if the file is missing or invalid.
 */
void config_manager_init(void) {
    /* defaults first */
    pendrive_cfg_t def;
    memset(&def, 0, sizeof(def));
    strncpy(def.volume_label, CFG_DEFAULT_VOLUME_LABEL, 11);
    def.read_only          = CFG_DEFAULT_READ_ONLY;
    def.enable_oled        = CFG_DEFAULT_ENABLE_OLED;
    def.led_on_connect     = CFG_DEFAULT_LED_ON_CONNECT;
    def.auto_mount_delay_ms= CFG_DEFAULT_AUTO_MOUNT_DELAY_MS;
    def.config_valid       = true;

    char content[512];
    uint32_t hash = 0;
    int n = config_read_file(content, sizeof(content), &hash);
    if (n < 0) {
        /* no file yet: create it with defaults and keep defaults loaded */
        config_create_if_missing();
        cfg_apply_defaults(&def);
        return;
    }

    /* parse line by line */
    g_cfg = def;                     // start from defaults
    g_cfg.config_valid = true;       // reset, will clear on error

    char line[128];
    char *save = content;
    char *l;
    while ((l = strtok_r(save, "\r\n", &save)) != NULL) {
        char key[32], value[64];
        if (config_parse_line(l, key, sizeof(key), value, sizeof(value))) {
            if (!config_apply_pair(key, value)) {
                g_cfg.config_valid = false;   // syntax / unknown key error
            }
        }
    }
}

/*
 * Hot-plug watcher. Should be called periodically (every CONFIG_POLL_INTERVAL_MS)
 * from the main loop. It re-reads config.txt; if the content hash changed it
 * reapplies the values so changes take effect without a reboot.
 *
 * This is deliberately side-effect light: it only updates g_cfg. The USB layer
 * uses g_cfg.read_only live, and the OLED re-renders from g_cfg each frame.
 */
void config_manager_poll(void) {
    static uint32_t last_hash = 0;
    static bool     first = true;

    char content[512];
    uint32_t hash = 0;
    int n = config_read_file(content, sizeof(content), &hash);
    if (n < 0) return;               // file not present yet

    if (first) {
        last_hash = hash;
        first = false;
        return;
    }
    if (hash == last_hash) return;   // unchanged

    /* changed: re-parse and re-apply */
    last_hash = hash;

    pendrive_cfg_t def;
    memset(&def, 0, sizeof(def));
    strncpy(def.volume_label, CFG_DEFAULT_VOLUME_LABEL, 11);
    def.read_only          = CFG_DEFAULT_READ_ONLY;
    def.enable_oled        = CFG_DEFAULT_ENABLE_OLED;
    def.led_on_connect     = CFG_DEFAULT_LED_ON_CONNECT;
    def.auto_mount_delay_ms= CFG_DEFAULT_AUTO_MOUNT_DELAY_MS;
    def.config_valid       = true;

    g_cfg = def;
    g_cfg.config_valid = true;

    char line[128];
    char *save = content;
    char *l;
    while ((l = strtok_r(save, "\r\n", &save)) != NULL) {
        char key[32], value[64];
        if (config_parse_line(l, key, sizeof(key), value, sizeof(value))) {
            if (!config_apply_pair(key, value)) {
                g_cfg.config_valid = false;
            }
        }
    }
}
