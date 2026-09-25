#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "config.h"

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLUME 0x08
#define FAT_ATTR_DIR 0x10
#define FAT_ATTR_ARCHIVE 0x20
#define FAT_ATTR_LFN 0x0F

#define FAT_EOC16 0xFFFFu
#define FAT_EOC32 0x0FFFFFFFu

typedef struct {
    uint32_t bps;          /* bytes per sector (512)              */
    uint8_t spc;           /* sectors per cluster                 */
    uint8_t nfats;         /* number of FATs                      */
    uint32_t rsvd;         /* reserved sectors                    */
    uint32_t root_entries; /* FAT16 root entries                  */
    uint32_t fat_size;     /* sectors per FAT                     */
    uint32_t total_sectors;
    uint32_t fat_begin;    /* LBA of first FAT                    */
    uint32_t root_begin;   /* FAT16: LBA of root                  */
    uint32_t root_cluster; /* FAT32: first cluster of root        */
    uint32_t data_begin;   /* LBA of cluster 2                    */
    uint32_t clusters;     /* total clusters on volume            */
    uint32_t next_free;    /* cluster-allocation hint             */
    int type;              /* 16 or 32                            */
} fat_vfs_t;

typedef struct {
    fat_vfs_t *fs;
    uint32_t first_cluster;  /* 0xff.. if not a file              */
    uint32_t size;           /* file size in bytes                */
    uint32_t dir_sector;     /* LBA of 512B block holding dir entry */
    uint8_t dir_offset;      /* entry offset (0..480)             */
    uint32_t dir_name[2];    /* first 8 bytes of 8.3 name, for FAT32 high cluster */
    uint32_t dir_cluster_lo; /* low word of first cluster, for FAT32 high word  */

    /* pre-allocated (created) images: full cluster chain in RAM */
    uint32_t *chain;
    uint32_t chain_len;      /* clusters actually linked          */
    uint32_t chain_alloc;    /* capacity of chain[]               */

    /* sequential access state (opened existing files) */
    uint32_t cur_cluster;
    uint32_t cur_cluster_idx;
    uint32_t cur_sector;
} fat_file_t;

typedef struct {
    char name8[9];   /* up to 8 chars, uppercase, NUL-terminated */
    char ext4[4];    /* up to 3 chars                            */
    uint32_t size;
} fat_scan_entry_t;

/* mount: parse BPB from first sector of SD volume. */
bool fat_mount(fat_vfs_t *fs);

/* ensure /IMG exists on the volume (FAT16 or FAT32); 8.3 name only. */
bool fat_ensure_img_dir(fat_vfs_t *fs);

/* list *.IMA / *.HFE entries of /IMG (8.3 names), sorted by (name, ext).
 * Returns count (<= maxn). */
int fat_scan_img(fat_vfs_t *fs, fat_scan_entry_t *out, int maxn);

/* open an arbitrary 8.3 file in the volume ROOT (FF.CFG, ...). */
bool fat_open_root(fat_vfs_t *fs, const char *base8, const char *ext3,
                   fat_file_t *f, uint32_t *chain, uint32_t chain_alloc);

/* open an existing image by base-name ("DISK0001") + ext ("IMA"). */
bool fat_open_img(fat_vfs_t *fs, const char *base8, const char *ext3,
                  fat_file_t *f, uint32_t *chain, uint32_t chain_alloc);

/* create a new image, pre-allocating its full cluster chain.  size_bytes is
 * the final size; the folder entry is written with size 0 and finalised in
 * fat_finalize(). */
bool fat_create_img(fat_vfs_t *fs, const char *base8, const char *ext3,
                    uint32_t size_bytes, fat_file_t *f);

/* read/write one 512B block at byte offset idx*512 of the file. */
bool fat_read_block(fat_file_t *f, uint32_t idx, uint8_t *buf);
bool fat_write_block(fat_file_t *f, uint32_t idx, const uint8_t *buf);

/* flush FAT + dir entry (call after all writes for created files). */
bool fat_finalize(fat_file_t *f);

/* raw helper for tests / future use */
uint32_t fat_get_free_clusters(fat_vfs_t *fs);