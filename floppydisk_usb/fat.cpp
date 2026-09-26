#include "fat.h"

#include <string.h>

#include "pico/stdlib.h"
#include "sd_spi.h"

/* ---------------------------------------------------------------- helpers -- */

static inline uint16_t le16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline void put_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static inline void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline uint32_t cluster_lba(fat_vfs_t *fs, uint32_t cluster) {
    return fs->data_begin + (cluster - 2) * fs->spc;
}

/* ------------------------------------------------ block read/write (LRU) --- */
/* Small write-through LRU cache for 512B blocks.  Every write goes to the card
 * immediately and also refreshes a matching slot, so cached data can never go
 * stale.  This speeds up repeated directory walks and the single-slotor FAT
 * reads (image scan + FAT lookup) which dominate during imaging.            */

#define BLK_CACHE_SLOTS 8u

typedef struct {
    uint32_t lba;
    uint32_t age;
    bool valid;
    uint8_t data[512];
} blk_slot_t;

static blk_slot_t g_blk[BLK_CACHE_SLOTS];
static uint32_t g_blk_age;

static void blk_invalidate(void) {
    memset(g_blk, 0, sizeof(g_blk));
    g_blk_age = 0;
}

static bool blk_write(fat_vfs_t *fs, uint32_t lba, const uint8_t *buf) {
    uint32_t abs_lba = lba + fs->partition_start;
    if (!sd_write_block(abs_lba, buf)) return false;
    for (int i = 0; i < BLK_CACHE_SLOTS; i++) {
        if (g_blk[i].valid && g_blk[i].lba == lba) {
            memcpy(g_blk[i].data, buf, 512);
            g_blk[i].age = ++g_blk_age;
            return true;
        }
    }
    return true;
}

static bool blk_read(fat_vfs_t *fs, uint32_t lba, uint8_t *buf) {
    for (int i = 0; i < BLK_CACHE_SLOTS; i++) {
        if (g_blk[i].valid && g_blk[i].lba == lba) {
            memcpy(buf, g_blk[i].data, 512);
            g_blk[i].age = ++g_blk_age;
            return true;
        }
    }
    uint32_t abs_lba = lba + fs->partition_start;
    if (!sd_read_block(abs_lba, buf)) return false;
    int lru = 0;
    for (int i = 1; i < BLK_CACHE_SLOTS; i++) {
        if (g_blk[i].age < g_blk[lru].age) lru = i;
    }
    g_blk[lru].lba = lba;
    g_blk[lru].age = ++g_blk_age;
    g_blk[lru].valid = true;
    memcpy(g_blk[lru].data, buf, 512);
    return true;
}

/* ------------------------------------------------------------ FAT region -- */

static uint8_t fat_cache[512];
static uint32_t fat_cache_sector = 0xFFFFFFFFu;
static bool fat_cache_dirty = false;

static void fat_flush(fat_vfs_t *fs) {
    if (fat_cache_dirty && fat_cache_sector != 0xFFFFFFFFu) {
        blk_write(fs, fat_cache_sector, fat_cache);
        fat_cache_dirty = false;
    }
}

static void fat_load_sector(fat_vfs_t *fs, uint32_t sector) {
    if (sector == fat_cache_sector) return;
    fat_flush(fs);
    blk_read(fs, sector, fat_cache);
    fat_cache_sector = sector;
}

static uint32_t fat_get(fat_vfs_t *fs, uint32_t entry) {
    if (fs->type == 32) {
        fat_load_sector(fs, fs->fat_begin + entry / 128);
        return le32(fat_cache + (entry % 128) * 4) & 0x0FFFFFFFu;
    }
    fat_load_sector(fs, fs->fat_begin + entry / 256);
    return le16(fat_cache + (entry % 256) * 2);
}

static void fat_set(fat_vfs_t *fs, uint32_t entry, uint32_t value) {
    if (fs->type == 32) {
        fat_load_sector(fs, fs->fat_begin + entry / 128);
        uint32_t *p = (uint32_t *)(fat_cache + (entry % 128) * 4);
        *p = (*p & 0xF0000000u) | (value & 0x0FFFFFFFu);
    } else {
        fat_load_sector(fs, fs->fat_begin + entry / 256);
        put_le16(fat_cache + (entry % 256) * 2, (uint16_t)value);
    }
    fat_cache_dirty = true;
}

/* MBR partition types for FAT */
#define MBR_FAT12       0x01
#define MBR_FAT16_SMALL 0x04  /* FAT16 < 32MB */
#define MBR_EXTENDED    0x05
#define MBR_FAT16       0x06  /* FAT16 >= 32MB */
#define MBR_NTFS        0x07
#define MBR_FAT32_CHS   0x0B  /* FAT32 CHS */
#define MBR_FAT32_LBA   0x0C  /* FAT32 LBA */
#define MBR_FAT16_LBA   0x0E  /* FAT16 LBA */
#define MBR_EXTENDED_LBA 0x0F

static bool is_fat_partition(uint8_t type) {
    return type == MBR_FAT12 || type == MBR_FAT16_SMALL || type == MBR_FAT16 ||
           type == MBR_FAT16_LBA || type == MBR_FAT32_CHS || type == MBR_FAT32_LBA;
}

/* Read BPB from sector, verify signature, parse FAT params */
static bool parse_bpb(fat_vfs_t *fs, uint32_t sector, const uint8_t *bpb) {
    if (bpb[510] != 0x55 || bpb[511] != 0xAA) return false;

    uint32_t bps = le16(bpb + 11);
    uint8_t spc = bpb[13];
    uint32_t rsvd = le16(bpb + 14);
    uint8_t nfats = bpb[16];
    uint32_t root_entries = le16(bpb + 17);
    uint32_t total16 = le16(bpb + 19);
    uint32_t fat16 = le16(bpb + 22);
    uint32_t total32 = le32(bpb + 32);
    uint32_t fat32 = le32(bpb + 36);
    uint32_t root_cluster = le32(bpb + 44);

    if (bps != 512 || spc == 0 || nfats == 0) return false;
    if (total16 == 0 && total32 == 0) return false;

    fs->bps = bps;
    fs->spc = spc;
    fs->nfats = nfats;
    fs->rsvd = rsvd;
    fs->root_entries = root_entries;
    fs->total_sectors = total16 ? total16 : total32;
    fs->fat_size = fat16 ? fat16 : fat32;
    fs->fat_begin = rsvd;
    fs->root_cluster = root_cluster;

    /* FAT16 volumes have FATSz16 != 0 and no root cluster; FAT32 the reverse. */
    if (fat16 == 0 && fat32 != 0) {
        /* FAT32 */
        fs->type = 32;
        fs->data_begin = rsvd + nfats * fs->fat_size;
        fs->root_begin = 0;
    } else {
        fs->type = 16;
        fs->root_begin = rsvd + nfats * fs->fat_size;
        fs->data_begin = fs->root_begin + (root_entries * 32) / bps;
    }
    fs->clusters = (fs->total_sectors - fs->data_begin) / spc;
    fs->next_free = 2;
    return true;
}

static bool raw_read(uint32_t lba, uint8_t *buf) {
    return sd_read_block(lba, buf);
}

bool fat_mount(fat_vfs_t *fs) {
    memset(fs, 0, sizeof(*fs));
    fs->next_free = 0;
    fat_cache_sector = 0xFFFFFFFFu;
    fat_cache_dirty = false;
    blk_invalidate();

    uint8_t buf[512];

    /* Try sector 0 first (superfloppy / no MBR) */
    if (raw_read(0, buf)) {
        if (buf[510] == 0x55 && buf[511] == 0xAA) {
            /* Check if it's a valid BPB (not MBR) */
            uint32_t bps = le16(buf + 11);
            uint8_t spc = buf[13];
            uint32_t rsvd = le16(buf + 14);
            uint8_t nfats = buf[16];
            if (bps == 512 && spc != 0 && nfats != 0) {
                /* Looks like a valid BPB, not MBR */
                if (parse_bpb(fs, 0, buf)) return true;
            }
        }
    }

    /* Sector 0 might be MBR - check partition table at offset 0x1BE */
    if (raw_read(0, buf)) {
        if (buf[510] == 0x55 && buf[511] == 0xAA) {
            /* MBR signature present, check partition entries at 0x1BE */
            for (int i = 0; i < 4; i++) {
                uint8_t *p = buf + 0x1BE + i * 16;
                uint8_t status = p[0];
                uint8_t type = p[4];
                uint32_t start_lba = le32(p + 8);
                uint32_t num_sectors = le32(p + 12);

                if (is_fat_partition(type) && start_lba != 0 && num_sectors != 0) {
                    /* Found FAT partition, read its BPB */
                    if (raw_read(start_lba, buf)) {
                        if (parse_bpb(fs, start_lba, buf)) {
                            fs->partition_start = start_lba;
                            fs->partition_sectors = num_sectors;
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

uint32_t fat_get_free_clusters(fat_vfs_t *fs) {
    uint32_t n = 0;
    for (uint32_t c = 2; c < fs->clusters; c++) {
        if (fat_get(fs, c) == 0) n++;
    }
    return n;
}

/* ---------------------------------------------------------- cluster alloc -- */

static bool alloc_one(fat_vfs_t *fs, uint32_t *out) {
    uint32_t start = fs->next_free;
    for (uint32_t c = start; c < fs->clusters; c++) {
        if (fat_get(fs, c) == 0) {
            fs->next_free = c + 1;
            *out = c;
            return true;
        }
    }
    for (uint32_t c = 2; c < start && c < fs->clusters; c++) {
        if (fat_get(fs, c) == 0) {
            fs->next_free = c + 1;
            *out = c;
            return true;
        }
    }
    return false;
}

/* Allocate n contiguous-in-logic clusters, linking them in the FAT. */
static uint32_t alloc_chain(fat_vfs_t *fs, uint32_t ncl, uint32_t *chain,
                            uint32_t cap) {
    if (ncl == 0 || ncl > cap) return 0;
    for (uint32_t i = 0; i < ncl; i++) {
        uint32_t c;
        if (!alloc_one(fs, &c)) {
            return 0; /* caller leaves partial chain */
        }
        chain[i] = c;
    }
    for (uint32_t i = 0; i + 1 < ncl; i++) {
        fat_set(fs, chain[i], chain[i + 1]);
    }
    fat_set(fs, chain[ncl - 1], fs->type == 32 ? FAT_EOC32 : FAT_EOC16);
    return ncl;
}

/* ------------------------------------------------------------- 8.3 names --- */

static void make_83(const char *base8, const char *ext3, uint8_t out[11]) {
    memset(out, 0x20, 11);
    for (int i = 0; i < 8 && base8 && base8[i]; i++) out[i] = (uint8_t)base8[i];
    if (ext3) {
        for (int i = 0; i < 3 && ext3[i]; i++) out[8 + i] = (uint8_t)ext3[i];
    }
}

/* ------------------------------------------------------------ dir walker --- */

/* shared chain table used by fat_create_img for pre-allocated images */
static uint32_t g_chain[FAT_MAX_CHAIN];

typedef struct {
    fat_vfs_t *fs;
    bool root16;
    uint32_t root_sectors;   /* FAT16 root: fixed region size in sectors */
    uint32_t chain[32];      /* subdir / FAT32-root cluster chain */
    uint32_t chain_len;
    uint32_t blocks;         /* total directory size in 512B blocks */
} fdir_t;

static bool fdir_open(fat_vfs_t *fs, uint32_t first_cluster, fdir_t *d) {
    memset(d, 0, sizeof(*d));
    d->fs = fs;
    if (fs->type == 16 && first_cluster == 0) {
        d->root16 = true;
        d->root_sectors = (fs->root_entries * 32) / fs->bps;
        d->blocks = d->root_sectors;
        return true;
    }
    uint32_t c = first_cluster;
    while (c >= 2 && c < fs->clusters && d->chain_len < 32) {
        d->chain[d->chain_len++] = c;
        uint32_t nxt = fat_get(fs, c);
        if (nxt == (fs->type == 32 ? FAT_EOC32 : FAT_EOC16) || nxt < 2) break;
        c = nxt;
        if (c == first_cluster) break; /* loop guard */
    }
    d->blocks = d->chain_len * fs->spc;
    return d->chain_len > 0;
}

static bool fdir_read_block(fdir_t *d, uint32_t idx, uint8_t *buf) {
    if (idx >= d->blocks) return false;
    if (d->root16) {
return blk_read(d->fs, d->fs->root_begin + idx, buf);
    }
    return blk_read(d->fs, cluster_lba(d->fs, cl) + (idx % d->fs->spc), buf);
}

static bool fdir_grow(fdir_t *d) {
    if (d->root16 || d->chain_len >= 32) return false;
    uint32_t c;
    uint32_t last = d->chain[d->chain_len - 1];
    if (!alloc_one(d->fs, &c)) return false;
    /* zero the freshly allocated cluster so directory scans stop cleanly */
    static const uint8_t zero[512] = {0};
    for (uint32_t s = 0; s < d->fs->spc; s++) {
        blk_write(d->fs, cluster_lba(d->fs, c) + s, zero);
    }
    fat_set(d->fs, last, c);
    fat_set(d->fs, c, d->fs->type == 32 ? FAT_EOC32 : FAT_EOC16);
    d->chain[d->chain_len++] = c;
    d->blocks = d->chain_len * d->fs->spc;
    return true;
}

/* find directory entry by 11-byte name. */
typedef struct {
    uint32_t sector; /* LBA of 512B block */
    uint8_t offset;  /* byte offset in block */
} dent_loc_t;

static bool dent_find(fdir_t *d, const uint8_t name11[11], dent_loc_t *loc) {
    uint8_t b[512];
    for (uint32_t blk = 0; blk < d->blocks; blk++) {
        if (!fdir_read_block(d, blk, b)) return false;
        for (int off = 0; off < 512; off += 32) {
            uint8_t first = b[off];
            if (first == 0x00) return false; /* end of dir */
            if (first == 0xE5) continue;
            if ((b[off + 11] & FAT_ATTR_LFN) == FAT_ATTR_LFN) continue;
            if (memcmp(b + off, name11, 11) == 0) {
                if (loc) {
                    loc->sector = d->root16 ? (d->fs->root_begin + blk)
                                            : cluster_lba(d->fs, d->chain[blk / d->fs->spc]) + (blk % d->fs->spc);
                    loc->offset = (uint8_t)off;
                }
                return true;
            }
        }
    }
    return false;
}

/* find a free entry, growing the directory if needed. */
static bool dent_alloc(fdir_t *d, dent_loc_t *loc) {
    uint8_t b[512];
    for (;;) {
        for (uint32_t blk = 0; blk < d->blocks; blk++) {
            if (!fdir_read_block(d, blk, b)) return false;
            for (int off = 0; off < 512; off += 32) {
                uint8_t first = b[off];
                if (first == 0x00 || first == 0xE5) {
                    loc->sector = d->root16
                                      ? (d->fs->root_begin + blk)
                                      : cluster_lba(d->fs, d->chain[blk / d->fs->spc]) + (blk % d->fs->spc);
                    loc->offset = (uint8_t)off;
                    return true;
                }
            }
        }
        if (!fdir_grow(d)) return false;
    }
}

/* ------------------------------------------------------------------ IMG ---- */

bool fat_ensure_img_dir(fat_vfs_t *fs) {
    fdir_t root;
    if (!fdir_open(fs, fs->root_cluster, &root)) return false;

    uint8_t img_name[11];
    make_83(IMG_DIR_NAME, NULL, img_name);
    dent_loc_t loc;
    if (dent_find(&root, img_name, &loc)) return true; /* already there */

    /* create "IMG" entry in root */
    if (!dent_alloc(&root, &loc)) return false;

    /* allocate one cluster for the new directory */
    uint32_t cl;
    if (!alloc_one(fs, &cl)) return false;
    fat_set(fs, cl, fs->type == 32 ? FAT_EOC32 : FAT_EOC16);

    /* zero the whole directory cluster, then write "." and ".." */
    static const uint8_t zero[512] = {0};
    for (uint32_t s = 0; s < fs->spc; s++) {
        blk_write(fs, cluster_lba(fs, cl) + s, zero);
    }
    uint8_t b[512];
    memset(b, 0, sizeof(b));
    memset(b, 0x20, 11); /* "." */
    b[0] = 0x2E;
    b[11] = FAT_ATTR_DIR;
    b[26] = (uint8_t)cl;
    b[27] = (uint8_t)(cl >> 8);
    memset(b + 32, 0x20, 11); /* ".." -> parent is root */
    b[32] = 0x2E;
    b[33] = 0x2E;
    b[43] = FAT_ATTR_DIR;
    b[58] = 0;
    b[59] = 0;
    blk_write(fs, cluster_lba(fs, cl), b);

    /* write the IMG dir entry in root */
    {
        uint8_t e[512];
        blk_read(fs, loc.sector, e);
        memset(e + loc.offset, 0, 32);
        make_83(IMG_DIR_NAME, NULL, e + loc.offset);
        e[loc.offset + 11] = FAT_ATTR_DIR;
        e[loc.offset + 26] = (uint8_t)cl;
        e[loc.offset + 27] = (uint8_t)(cl >> 8);
        if (fs->type == 32) {
            e[loc.offset + 20] = (uint8_t)(cl >> 16);
            e[loc.offset + 21] = (uint8_t)(cl >> 24);
        }
blk_write(fs, loc.sector, e);
    }
    return true;
}

static const uint16_t WRT_TIME = (12 << 11) | (0 << 5);  /* 12:00 */
static const uint16_t WRT_DATE = (46 << 9) | (9 << 5) | 23; /* 2026-09-23 */

static bool fdir_open_img(fat_vfs_t *fs, fdir_t *d) {
    fdir_t root;
    if (!fdir_open(fs, fs->root_cluster, &root)) return false;
    uint8_t img_name[11];
    make_83(IMG_DIR_NAME, NULL, img_name);
    dent_loc_t loc;
    if (!dent_find(&root, img_name, &loc)) return false;

    uint8_t e[512];
    blk_read(fs, loc.sector, e);
    uint32_t cl = le16(e + loc.offset + 26);
    if (fs->type == 32) cl |= (uint32_t)le16(e + loc.offset + 20) << 16;
    return fdir_open(fs, cl, d);
}

int fat_scan_img(fat_vfs_t *fs, fat_scan_entry_t *out, int maxn) {
    if (!fat_ensure_img_dir(fs)) return 0;
    fdir_t img;
    if (!fdir_open_img(fs, &img)) return 0;

    uint8_t b[512];
    int n = 0;
    for (uint32_t blk = 0; blk < img.blocks && n < maxn; blk++) {
        if (!fdir_read_block(&img, blk, b)) break;
        for (int off = 0; off < 512; off += 32) {
            uint8_t first = b[off];
            if (first == 0x00) goto done;
            if (first == 0xE5) continue;
            if ((b[off + 11] & FAT_ATTR_LFN) == FAT_ATTR_LFN) continue;
            if (b[off + 11] & (FAT_ATTR_DIR | FAT_ATTR_VOLUME)) continue;
            /* match *.IMA / *.HFE (write-able disk image formats) */
            if (memcmp(b + off + 8, "IMA", 3) != 0 &&
                memcmp(b + off + 8, "HFE", 3) != 0) {
                continue;
            }
            if (n >= maxn) goto done;
            int k = 0;
            for (int i = 0; i < 8; i++) {
                char c = (char)b[off + i];
                if (c != ' ') out[n].name8[k++] = c;
            }
            out[n].name8[k] = 0;
            k = 0;
            for (int i = 0; i < 3; i++) {
                char c = (char)b[off + 8 + i];
                if (c != ' ') out[n].ext4[k++] = c;
            }
            out[n].ext4[k] = 0;
            out[n].size = le32(b + off + 28);
            n++;
        }
    }
done:
    /* sort by (name, ext) so slot navigation is in a stable order */
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            int cmp = strcmp(out[i].name8, out[j].name8);
            if (cmp > 0 || (cmp == 0 && strcmp(out[i].ext4, out[j].ext4) > 0)) {
                fat_scan_entry_t t = out[i];
                out[i] = out[j];
                out[j] = t;
            }
        }
    }
    return n;
}

/* ---------------------------------------------------------------- files ---- */

bool fat_open_img(fat_vfs_t *fs, const char *base8, const char *ext3,
                  fat_file_t *f, uint32_t *chain, uint32_t chain_alloc) {
    memset(f, 0, sizeof(*f));
    f->fs = fs;
    f->chain = chain;
    f->chain_alloc = chain_alloc;

    fdir_t img;
    if (!fdir_open_img(fs, &img)) return false;

    uint8_t name11[11];
    make_83(base8, ext3, name11);
    dent_loc_t loc;
    if (!dent_find(&img, name11, &loc)) return false;

    uint8_t e[512];
    blk_read(fs, loc.sector, e);
    f->dir_sector = loc.sector;
    f->dir_offset = loc.offset;
    memcpy(f->dir_name, e + loc.offset, 8);
    f->first_cluster = le16(e + loc.offset + 26);
    if (fs->type == 32) f->first_cluster |= (uint32_t)le16(e + loc.offset + 20) << 16;
    f->size = le32(e + loc.offset + 28);
    f->cur_cluster = f->first_cluster;
    return true;
}

/* Open an arbitrary 8.3 file in the volume ROOT (e.g. FF.CFG, HFE images can
 * live anywhere but we use root + /IMG).  If `chain` is provided, ram-caches
 * the whole cluster chain so fat_read_block() can jump to any byte cheaply. */
bool fat_open_root(fat_vfs_t *fs, const char *base8, const char *ext3,
                   fat_file_t *f, uint32_t *chain, uint32_t chain_alloc) {
    memset(f, 0, sizeof(*f));
    f->fs = fs;
    f->chain = chain;
    f->chain_alloc = chain_alloc;

    fdir_t root;
    if (!fdir_open(fs, fs->root_cluster, &root)) return false;

    uint8_t name11[11];
    make_83(base8, ext3, name11);
    dent_loc_t loc;
    if (!dent_find(&root, name11, &loc)) return false;

    uint8_t e[512];
    blk_read(fs, loc.sector, e);
    f->dir_sector = loc.sector;
    f->dir_offset = loc.offset;
    memcpy(f->dir_name, e + loc.offset, 8);
    f->first_cluster = le16(e + loc.offset + 26);
    if (fs->type == 32)
        f->first_cluster |= (uint32_t)le16(e + loc.offset + 20) << 16;
    f->size = le32(e + loc.offset + 28);
    f->cur_cluster = f->first_cluster;

    if (chain && chain_alloc > 0) {
        uint32_t c = f->first_cluster;
        uint32_t i = 0;
        uint32_t eoc = fs->type == 32 ? FAT_EOC32 : FAT_EOC16;
        while (c >= 2 && c < fs->clusters && i < chain_alloc) {
            chain[i++] = c;
            uint32_t nxt = fat_get(fs, c);
            if (nxt < 2 || nxt == eoc) break;
            c = nxt;
        }
        f->chain_len = i;
        if (i == 0) return false;
    }
    return true;
}

bool fat_create_img(fat_vfs_t *fs, const char *base8, const char *ext3,
                    uint32_t size_bytes, fat_file_t *f) {
    memset(f, 0, sizeof(*f));
    f->fs = fs;
    f->size = size_bytes;

    if (!fat_ensure_img_dir(fs)) return false;
    fdir_t img;
    if (!fdir_open_img(fs, &img)) return false;

    uint8_t name11[11];
    make_83(base8, ext3, name11);
    dent_loc_t loc;
    if (!dent_alloc(&img, &loc)) return false;

    uint32_t ncl = (size_bytes + (fs->bps * fs->spc) - 1) / (fs->bps * fs->spc);
    if (ncl > FAT_MAX_CHAIN) return false;

    f->chain = g_chain;
    f->chain_alloc = FAT_MAX_CHAIN;
    uint32_t got = alloc_chain(fs, ncl, g_chain, FAT_MAX_CHAIN);
    if (got != ncl) return false;
    f->chain_len = ncl;
    f->first_cluster = g_chain[0];
    f->dir_sector = loc.sector;
    f->dir_offset = loc.offset;

    /* write directory entry */
    uint8_t e[512];
    blk_read(fs, loc.sector, e);
    memset(e + loc.offset, 0, 32);
    make_83(base8, ext3, e + loc.offset);
    e[loc.offset + 11] = FAT_ATTR_ARCHIVE;
    e[loc.offset + 12] = 0;
    put_le16(e + loc.offset + 14, WRT_TIME);
    put_le16(e + loc.offset + 16, WRT_DATE);
    put_le16(e + loc.offset + 18, WRT_DATE);
    put_le16(e + loc.offset + 22, WRT_TIME);
    put_le16(e + loc.offset + 24, WRT_DATE);
    put_le16(e + loc.offset + 26, (uint16_t)(g_chain[0] & 0xFFFF));
    if (fs->type == 32) put_le16(e + loc.offset + 20, (uint16_t)(g_chain[0] >> 16));
    put_le32(e + loc.offset + 28, 0);
    blk_write(fs, loc.sector, e);

    f->cur_cluster = g_chain[0];
    return true;
}

bool fat_finalize(fat_file_t *f) {
    uint8_t e[512];
    if (!blk_read(f->fs, f->dir_sector, e)) return false;
    put_le32(e + f->dir_offset + 28, f->size);
    if (f->fs->type == 32) {
        put_le16(e + f->dir_offset + 20, (uint16_t)(f->first_cluster >> 16));
    }
    put_le16(e + f->dir_offset + 26, (uint16_t)(f->first_cluster & 0xFFFF));
    if (!blk_write(f->fs, f->dir_sector, e)) {
        fat_flush(f->fs);
        return false;
    }
    fat_flush(f->fs);
    return true;
}

/* walk to the cluster index (sequential access of opened files) */
static bool file_cluster_at(fat_file_t *f, uint32_t cluster_idx,
                            uint32_t *cluster) {
    if (f->chain && f->chain_len) {
        if (cluster_idx >= f->chain_len) return false;
        *cluster = f->chain[cluster_idx];
        return true;
    }
    if (cluster_idx < f->cur_cluster_idx) {
        f->cur_cluster = f->first_cluster;
        f->cur_cluster_idx = 0;
    }
    while (f->cur_cluster_idx < cluster_idx) {
        uint32_t nxt = fat_get(f->fs, f->cur_cluster);
        uint32_t eoc = f->fs->type == 32 ? FAT_EOC32 : FAT_EOC16;
        if (nxt < 2 || nxt == eoc) return false;
        f->cur_cluster = nxt;
        f->cur_cluster_idx++;
    }
    *cluster = f->cur_cluster;
    return true;
}

bool fat_read_block(fat_file_t *f, uint32_t idx, uint8_t *buf) {
    uint32_t cluster_idx = idx / f->fs->spc;
    uint32_t cl;
    if (!file_cluster_at(f, cluster_idx, &cl)) return false;
    return blk_read(f->fs, cluster_lba(f->fs, cl) + (idx % f->fs->spc), buf);
}

bool fat_write_block(fat_file_t *f, uint32_t idx, const uint8_t *buf) {
    uint32_t cluster_idx = idx / f->fs->spc;
    uint32_t cl;
    if (!file_cluster_at(f, cluster_idx, &cl)) return false;
    return blk_write(f->fs, cluster_lba(f->fs, cl) + (idx % f->fs->spc), buf);
}