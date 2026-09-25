#include "cfg.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "fat.h"
#include "mfm.h"

#define CFG_MAX_BYTES 4096

static char *ltrim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* parse one "key = value" line; returns true if a key was handled */
static bool parse_line(char *line, floppy_cfg_t *out) {
    char *p = ltrim(line);
    if (*p == 0 || *p == '#' || *p == ';') return false;
    char *eq = strchr(p, '=');
    if (!eq) return false;
    *eq = 0;
    char *key = p;
    char *val = eq + 1;
    while (*val == ' ' || *val == '\t') val++;

    if (strcasecmp(key, "gap3") == 0) {
        long v = strtol(val, NULL, 10);
        if (v > 0) {
            out->gap3_512 = (uint16_t)v;
            /* clamp + apply through the MFM encoder's own guard */
            mfm_set_gap3_512((uint16_t)v);
        }
        return true;
    }
    if (strcasecmp(key, "density") == 0) {
        if (strcasecmp(val, "hd") == 0) out->density = DENSITY_HD;
        else if (strcasecmp(val, "dd") == 0) out->density = DENSITY_DD;
        else out->density = DENSITY_AUTO;
        return true;
    }
    return false;
}

bool cfg_load(void *fs_impl, floppy_cfg_t *out) {
    memset(out, 0, sizeof(*out));
    out->density = DENSITY_AUTO;

    fat_vfs_t *fs = (fat_vfs_t *)fs_impl;
    fat_file_t f;
    uint32_t chain[64]; /* FF.CFG is tiny; 64 clusters = 1MB is plenty */
    if (!fat_open_root(fs, "FF", "CFG", &f, chain, 64)) return false;

    static uint8_t buf[CFG_MAX_BYTES];
    size_t n = 0;
    uint8_t blk[512];
    while (n < sizeof(buf) && n < f.size) {
        if (n % 512 == 0) {
            if (!fat_read_block(&f, (uint32_t)(n / 512), blk)) break;
        }
        buf[n] = blk[n % 512];
        n++;
    }

    uint32_t pos = 0;
    while (pos < n) {
        uint32_t e = pos;
        while (e < n && buf[e] != '\n') e++;
        size_t len = e - pos;
        char line[96];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, buf + pos, len);
        line[len] = 0;
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ' ||
                           line[len - 1] == '\t')) {
            line[--len] = 0;
        }
        if (parse_line(line, out)) out->present = true;
        pos = e + 1;
    }
    return out->present;
}