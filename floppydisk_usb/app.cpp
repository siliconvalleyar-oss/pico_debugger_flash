#include "app.h"
#include "version.h"

#include "config.h"
#include "cfg.h"
#include "fat.h"
#include "floppy.h"
#include "hfe.h"
#include "mfm.h"
#include "sd_spi.h"
#include "ssd1306.h"

#include "hardware/gpio.h"
#include "pico/stdio.h"
#include "pico/time.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

static const char* g_version_str = FLOPPYDISK_VERSION_STR;
static uint32_t g_build_num = FLOPPYDISK_BUILD;

const char* app_get_version_str(void) {
    return g_version_str;
}

uint32_t app_get_build_num(void) {
    return g_build_num;
}

void app_show_version(void) {
    char line1[22], line2[22], line3[22];
    snprintf(line1, sizeof(line1), "FLOPPYDISK USB");
    snprintf(line2, sizeof(line2), "v%s", g_version_str);
    snprintf(line3, sizeof(line3), "build %lu", (unsigned long)g_build_num);
    ssd1306_clear();
    ssd1306_puts(1, 0, line1);
    ssd1306_puts(1, 2, line2);
    ssd1306_puts(1, 3, line3);
    ssd1306_puts(1, 5, "Iniciando...");
    ssd1306_flush();
    busy_wait_ms(1500);
}

/* ================================================================== state === */

static fat_vfs_t g_vol;
static bool g_sd_ok = false;
static floppy_cfg_t g_cfg;

static uint8_t g_flux[TRACK_BUF_SIZE];
static uint8_t g_track[FLOPPY_HD_SPT * MFM_SECTOR_SIZE];
static uint8_t g_valid[FLOPPY_HD_SPT];

/* image picker (slot navigation) */
#define IMG_LIST_MAX 64
static fat_scan_entry_t g_imglist[IMG_LIST_MAX];
static int g_imgn = 0;
static int g_imgsel = 0;

/* ----------------------------------------------------------- rtc of buttons */
static bool btn_tap(uint gp) {
    if (gpio_get(gp)) return false; /* idle = high */
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (!gpio_get(gp)) {
        /* debounce: wait until it has been low for > 15 ms */
        if (to_ms_since_boot(get_absolute_time()) - start > 15) break;
        busy_wait_ms(1);
    }
    while (!gpio_get(gp)) busy_wait_ms(5); /* wait for release */
    return true;
}

static bool g_abort = false;
static bool btn_abort(void) {
    if (g_abort) return true;
    if (gpio_get(BTN_A_PIN)) {
        return false; /* not pressed */
    }
    static uint32_t since = 0;
    if (!since) since = to_ms_since_boot(get_absolute_time());
    if (to_ms_since_boot(get_absolute_time()) - since > 80) {
        g_abort = true;
        return true;
    }
    return false;
}

/* ============================================================== tiny UI ==== */

static void draw_running(void);
static void draw_menu(void);
static void draw_message(void);

enum { R_MENU, R_RUN, R_MSG, R_SELECT };
static int g_ui = R_MENU;
static int g_sel = 0;               /* 0=LEER 1=ESCRIBIR 2=INFO */
static char g_msg[3][22];           /* message screen (up to 3 lines) */
static int g_tracks_total = 0, g_tracks_done = 0;
static char g_run_title[12];

static void ui_message(const char *l1, const char *l2, const char *l3) {
    g_ui = R_MSG;
    strncpy(g_msg[0], l1, 21);
    g_msg[0][21] = 0;
    strncpy(g_msg[1], l2, 21);
    g_msg[1][21] = 0;
    strncpy(g_msg[2], l3, 21);
    g_msg[2][21] = 0;
}

static void set_result(bool ok, const char *a, const char *b) {
    ui_message(ok ? "OK" : "ERROR", a, b);
}

/* ============================================================== read ======= */

static int detect_format(mfm_timings_t *out, int *out_hd) {
    int32_t off;
    size_t n = floppy_capture_track(g_flux, TRACK_BUF_SIZE, &off, 220, 250);
    if (!n) return -1;

    mfm_timings_t th, td;
    mfm_timings(&th, MFM_HD_BIT_TIME_US, MFM_SAMPLE_FREQ);
    mfm_timings(&td, MFM_DD_BIT_TIME_US, MFM_SAMPLE_FREQ);

    size_t hd = mfm_decode_track(g_flux, n, g_track,
                                 FLOPPY_HD_SPT, g_valid, &th, true, NULL);
    size_t dd = mfm_decode_track(g_flux, n, g_track,
                                 FLOPPY_DD_SPT, g_valid, &td, true, NULL);
    if (hd == FLOPPY_HD_SPT) {
        *out = th;
        *out_hd = 1;
        return 0;
    }
    if (dd == FLOPPY_DD_SPT) {
        *out = td;
        *out_hd = 0;
        return 0;
    }
    /* fall back to whichever decoded more sectors */
    if (hd >= dd && hd > 0) {
        *out = th;
        *out_hd = 1;
        return 0;
    }
    if (dd > 0) {
        *out = td;
        *out_hd = 0;
        return 0;
    }
    return -1;
}

static bool name_exists(const char *base) {
    fat_scan_entry_t e[20];
    int n = fat_scan_img(&g_vol, e, 20);
    char nm[9], ex[4];
    for (int i = 0; i < n; i++) {
        strncpy(nm, e[i].name8, 8);
        nm[8] = 0;
        strncpy(ex, e[i].ext4, 3);
        ex[3] = 0;
        /* only .IMA counts: reads always create .IMA files */
        if (strcmp(ex, "IMA") != 0) continue;
        if (strcmp(nm, base) == 0) return true;
    }
    return false;
}

static void do_read(void) {
    /* drive up */
    floppy_select(true);
    if (!floppy_spin_motor(true)) {
        set_result(false, "SIN INDEX", "REVISA MOTOR");
        return;
    }
    if (!floppy_goto_track(0)) {
        floppy_spin_motor(false);
        set_result(false, "SIN TRK0", "CABLE?");

        floppy_select(false);
        return;
    }
    floppy_side(0);

    /* format detection from track 0 / head 0 */
    mfm_timings_t t;
    int hd;
    if (detect_format(&t, &hd) < 0) {
        floppy_spin_motor(false);
        floppy_select(false);
        set_result(false, "NO PUEDO LEER", "EL DISCO");
        return;
    }
    floppy_set_density(hd);
    int spt = hd ? FLOPPY_HD_SPT : FLOPPY_DD_SPT;
    size_t isize = hd ? IMAGE_HD_SIZE : IMAGE_DD_SIZE;

    /* image file name: DISK0001.. first free */
    char base[9];
    int num = 1;
    while (num <= 9999) {
        snprintf(base, sizeof(base), "DISK%04d", num);
        if (!name_exists(base)) break;
        num++;
    }
    if (num > 9999) {
        floppy_spin_motor(false);
        floppy_select(false);
        set_result(false, "SD LLENA", "(<9999 IMGS)");
        return;
    }

    /* space check */
    uint32_t need = (isize + (g_vol.bps * g_vol.spc) - 1) / (g_vol.bps * g_vol.spc);
    if (fat_get_free_clusters(&g_vol) < need) {
        floppy_spin_motor(false);
        floppy_select(false);
        set_result(false, "SD SIN", "ESPACIO");
        return;
    }

    fat_file_t img;
    if (!fat_create_img(&g_vol, base, "IMA", isize, &img)) {
        floppy_spin_motor(false);
        floppy_select(false);
        set_result(false, "SD ERROR", "CREAR");
        return;
    }

    /* read all tracks */
    g_tracks_total = FLOPPY_HD_TRACKS * FLOPPY_HEADS;
    g_tracks_done = 0;
    strcpy(g_run_title, "LEYENDO");
    g_ui = R_RUN;
    g_abort = false;

    uint32_t bad = 0;
    bool broken = false;
    for (int cyl = 0; cyl < FLOPPY_HD_TRACKS && !broken; cyl++) {
        floppy_goto_track(cyl);
        for (int h = 0; h < FLOPPY_HEADS && !broken; h++) {
            floppy_side(h);
            int got = 0;
            for (int a = 0; a < 5; a++) {
                int32_t off;
                size_t n = floppy_capture_track(g_flux, TRACK_BUF_SIZE, &off,
                                                220, 250);
                if (!n) {
                    got = 0;
                    break;
                }
                got = (int)mfm_decode_track(g_flux, n, g_track, spt, g_valid,
                                            &t, a == 0, NULL);
                if (got == spt) break;
            }
            if (got != spt) bad++;
            for (int i = 0; i < spt; i++) {
                if (g_valid[i]) {
                    fat_write_block(&img, (uint32_t)(cyl * 2 + h) * spt + i,
                                    &g_track[i * MFM_SECTOR_SIZE]);
                }
            }
            g_tracks_done++;
            draw_running();
            if (btn_abort()) broken = true;
        }
    }

    /* write down a truthful file size (all data written before finalize) */
    img.size = (uint32_t)g_tracks_done * spt * MFM_SECTOR_SIZE;
    fat_finalize(&img);

    floppy_spin_motor(false);
    floppy_select(false);

    if (broken) {
        set_result(false, "ABORTADO", base);
    } else if (bad == 0) {
        set_result(true, base, "SIN ERRORES");
    } else {
        set_result(true, base, "ERR EN ALGUNOS");
    }
}

/* ============================================================== write ====== */

/* ------------------------------------------------------------- .IMA write -- */

static void do_write_ima(const char *base, const char *ext) {
    fat_file_t img;
    if (!fat_open_img(&g_vol, base, ext, &img, NULL, 0)) {
        set_result(false, "SD ERROR", "ABRIR");
        return;
    }

    int hd;
    int spt;
    if (img.size == IMAGE_HD_SIZE) {
        hd = 1;
        spt = FLOPPY_HD_SPT;
    } else if (img.size == IMAGE_DD_SIZE) {
        hd = 0;
        spt = FLOPPY_DD_SPT;
    } else {
        set_result(false, "TAMANO IMG", "NO VALIDO");
        return;
    }

    if (floppy_get_write_protect()) {
        set_result(false, "DISCO", "PROTEGIDO");
        return;
    }

    floppy_select(true);
    if (!floppy_spin_motor(true)) {
        set_result(false, "SIN INDEX", "REVISA MOTOR");
        return;
    }
    if (!floppy_goto_track(0)) {
        floppy_spin_motor(false);
        floppy_select(false);
        set_result(false, "SIN TRK0", "CABLE?");
        return;
    }
    floppy_side(0);
    floppy_set_density(hd);

    mfm_timings_t t;
    mfm_timings(&t, hd ? MFM_HD_BIT_TIME_US : MFM_DD_BIT_TIME_US,
                MFM_SAMPLE_FREQ);

    g_tracks_total = FLOPPY_HD_TRACKS * FLOPPY_HEADS;
    g_tracks_done = 0;
    strcpy(g_run_title, "ESCRIBIENDO");
    g_ui = R_RUN;
    g_abort = false;

    bool broken = false;
    for (int cyl = 0; cyl < FLOPPY_HD_TRACKS && !broken; cyl++) {
        floppy_goto_track(cyl);
        for (int h = 0; h < FLOPPY_HEADS && !broken; h++) {
            floppy_side(h);
            for (int i = 0; i < spt; i++) {
                if (!fat_read_block(&img, (uint32_t)(cyl * 2 + h) * spt + i,
                                    &g_track[i * MFM_SECTOR_SIZE])) {
                    broken = true;
                    break;
                }
            }
            if (broken) continue;
            size_t nflux = mfm_encode_track(g_track, spt, (uint8_t)cyl,
                                            (uint8_t)h, g_flux,
                                            TRACK_BUF_SIZE, &t);
            floppy_write_track(g_flux, nflux, true);
            g_tracks_done++;
            draw_running();
            if (btn_abort()) broken = true;
        }
    }

    floppy_spin_motor(false);
    floppy_select(false);

    if (broken) {
        set_result(false, "ABORTADO", base);
    } else {
        set_result(true, base, "GRABADO");
    }
}

/* --------------------------------------------------------------- HFE write -- */

#define HFE_CHAIN_MAX 4096u

static void do_write_hfe(const char *base) {
    static uint32_t chain[HFE_CHAIN_MAX];

    fat_file_t f;
    if (!fat_open_root(&g_vol, base, "HFE", &f, chain, HFE_CHAIN_MAX)) {
        set_result(false, "SD ERROR", "ABRIR HFE");
        return;
    }
    hfe_t hfe;
    if (!hfe_open(&hfe, &f)) {
        set_result(false, "HFE", "INVALIDO");
        return;
    }
    /* density: FF.CFG override wins, otherwise infer from the image bitrate */
    int hd;
    if (g_cfg.density == DENSITY_HD) hd = 1;
    else if (g_cfg.density == DENSITY_DD) hd = 0;
    else hd = hfe_is_hd(&hfe) ? 1 : 0;

    if (floppy_get_write_protect()) {
        set_result(false, "DISCO", "PROTEGIDO");
        return;
    }

    floppy_select(true);
    if (!floppy_spin_motor(true)) {
        set_result(false, "SIN INDEX", "REVISA MOTOR");
        return;
    }
    if (!floppy_goto_track(0)) {
        floppy_spin_motor(false);
        floppy_select(false);
        set_result(false, "SIN TRK0", "CABLE?");
        return;
    }
    floppy_side(0);
    floppy_set_density(hd);

    g_tracks_total = (int)hfe.nr_cyls * hfe.nr_sides;
    g_tracks_done = 0;
    strcpy(g_run_title, "ESCRIBIENDO");
    g_ui = R_RUN;
    g_abort = false;

    bool broken = false;
    for (uint32_t cyl = 0; cyl < hfe.nr_cyls && !broken; cyl++) {
        if (!hfe_seek_cyl(&hfe, &f, cyl)) {
            broken = true;
            break;
        }
        floppy_goto_track((int)cyl);
        for (int h = 0; h < hfe.nr_sides && !broken; h++) {
            floppy_side(h);
            size_t n = hfe_flux_for_side(&hfe, &f, h, g_flux,
                                         TRACK_BUF_SIZE);
            if (!n) {
                broken = true;
                break;
            }
            floppy_write_track(g_flux, n, true);
            g_tracks_done++;
            draw_running();
            if (btn_abort()) broken = true;
        }
    }

    floppy_spin_motor(false);
    floppy_select(false);

    if (broken) {
        set_result(false, "ABORTADO", base);
    } else {
        set_result(true, base, "GRABADO");
    }
}

/* --------------------------------------------- image picker (slot nav) ------ */

static void draw_selector(void) {
    char line[22], pos[22];
    snprintf(line, sizeof(line), "> %s.%s", g_imglist[g_imgsel].name8,
             g_imglist[g_imgsel].ext4);
    snprintf(pos, sizeof(pos), "  %d/%d", g_imgsel + 1, g_imgn);
    ssd1306_clear();
    ssd1306_puts(1, 0, "ELEGIR IMAGEN");
    ssd1306_puts(1, 3, line);
    ssd1306_puts(2, 4, pos);
    ssd1306_puts(1, 6, "A:MAS  B:OK");
    ssd1306_flush();
}

/* Enter ESCRIBIR: scan /IMG, show the slot picker. */
static void do_write(void) {
    g_imgn = fat_scan_img(&g_vol, g_imglist, IMG_LIST_MAX);
    if (g_imgn <= 0) {
        set_result(false, "NO HAY IMGS", "EN SD");
        return;
    }
    g_imgsel = 0;
    g_ui = R_SELECT;
    draw_selector();
}

static void do_write_selected(void) {
    if (strcmp(g_imglist[g_imgsel].ext4, "HFE") == 0) {
        do_write_hfe(g_imglist[g_imgsel].name8);
    } else {
        do_write_ima(g_imglist[g_imgsel].name8, g_imglist[g_imgsel].ext4);
    }
    if (g_ui != R_RUN) draw_message();
}

/* ================================================================== info ==== */

static void do_info(void) {
    uint32_t free = 0;
    uint32_t blocks = 0;
    if (g_sd_ok) {
        free = fat_get_free_clusters(&g_vol);
        blocks = g_vol.total_sectors;
    }
    /* unit is clusters * (spc * 512) bytes / 1024 Kbytes */
    uint32_t free_kb = (free * g_vol.spc * g_vol.bps) / 1024;
    char l1[16], l2[16], l3[16];
    snprintf(l1, sizeof(l1), "SD %s", g_sd_ok ? "OK" : "NO");
    snprintf(l2, sizeof(l2), "LIBRE %luKB", (unsigned long)free_kb);
    snprintf(l3, sizeof(l3), "CAP %luMB",
             (unsigned long)((blocks / 2048u))); /* 1M = 2048 blocks */
    ui_message(l1, l2, l3);
}

/* ================================================================== menu ==== */

static void draw_menu(void) {
    ssd1306_clear();
    ssd1306_puts(1, 0, "== FLOPPYCARD ==");
    ssd1306_puts(2, 2, (g_sel == 0) ? "> LEER" : "  LEER");
    ssd1306_puts(2, 3, (g_sel == 1) ? "> ESCRIBIR" : "  ESCRIBIR");
    ssd1306_puts(2, 4, (g_sel == 2) ? "> INFO" : "  INFO");
    ssd1306_puts(1, 6, "A:MOVER B:OK");
    ssd1306_puts(1, 7, "HD 1.44 / DD 720");
    ssd1306_flush();
}

static void draw_running(void) {
    static char line1[22];
    snprintf(line1, sizeof(line1), "%s %3d/%3d", g_run_title, g_tracks_done,
             g_tracks_total);
    ssd1306_clear();
    ssd1306_puts(1, 0, line1);
    uint8_t pct = (uint8_t)((uint32_t)g_tracks_done * 100 / g_tracks_total);
    ssd1306_bar(4, 12, 120, 8, pct);
    ssd1306_puts(1, 6, "A: ABORTAR");
    ssd1306_flush();
}

static void draw_message(void) {
    ssd1306_clear();
    ssd1306_puts(1, 2, g_msg[0]);
    ssd1306_puts(1, 3, g_msg[1]);
    ssd1306_puts(1, 4, g_msg[2]);
    ssd1306_puts(1, 6, "B: VOLVER");
    ssd1306_flush();
}

/* ================================================================== init ==== */

static void setup_gpio(void) {
    gpio_init(BTN_A_PIN);
    gpio_pull_up(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_init(BTN_B_PIN);
    gpio_pull_up(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);

    gpio_init(FLOPPY_LED_PIN);
    gpio_set_dir(FLOPPY_LED_PIN, GPIO_OUT);
    gpio_put(FLOPPY_LED_PIN, 0);

    floppy_init();
}

static void init_storage(void) {
    g_sd_ok = sd_init();
    if (g_sd_ok) {
        g_sd_ok = fat_mount(&g_vol);
        if (g_sd_ok) {
            g_sd_ok = fat_ensure_img_dir(&g_vol);
            if (g_sd_ok) cfg_load(&g_vol, &g_cfg);
        }
    }
}

static bool g_app_initialized = false;

void app_run(void) {
    if (!g_app_initialized) {
        setup_gpio();
        app_show_version();
        init_storage();
        draw_menu();
        g_app_initialized = true;
    }

    /* if SD is bad, hide the image-touching entries */
    /* Run one iteration of the UI loop */
    if (g_ui == R_MENU) {
        if (btn_tap(BTN_A_PIN)) {
            if (!g_sd_ok) {
                init_storage();
                if (g_sd_ok) draw_menu();
                else {
                    ssd1306_clear();
                    ssd1306_puts(1, 3, "SIN MICROSD");
                    ssd1306_flush();
                }
            } else {
                g_sel = (g_sel + 1) % 3;
                draw_menu();
            }
        } else if (btn_tap(BTN_B_PIN)) {
            if (!g_sd_ok) return; /* storage must work first */
            g_abort = false;
            if (g_sel == 0) do_read();
            else if (g_sel == 1) do_write();
            else do_info();
            if (g_ui == R_MSG) draw_message();
        }
    } else if (g_ui == R_RUN) {
        /* Non-blocking: update progress and check for abort */
        draw_running();
        if (btn_abort()) {
            g_ui = R_MSG;
            draw_message();
        }
        busy_wait_ms(20);
    } else if (g_ui == R_SELECT) {
        /* image slot picker: A = next, B = write selected */
        if (btn_tap(BTN_A_PIN)) {
            g_imgsel = (g_imgsel + 1) % g_imgn;
            draw_selector();
        } else if (btn_tap(BTN_B_PIN)) {
            g_abort = false;
            do_write_selected();
        }
        busy_wait_ms(30);
    } else { /* R_MSG */
        if (btn_tap(BTN_A_PIN) || btn_tap(BTN_B_PIN)) {
            g_ui = R_MENU;
            draw_menu();
        }
        busy_wait_ms(40);
    }
}

/* FAT access for shell */
bool app_fat_ok(void) {
    return g_sd_ok;
}

uint32_t app_fat_free_kb(void) {
    if (!g_sd_ok) return 0;
    uint32_t free = fat_get_free_clusters(&g_vol);
    return (free * g_vol.spc * g_vol.bps) / 1024;
}

uint32_t app_fat_total_kb(void) {
    if (!g_sd_ok) return 0;
    return (g_vol.total_sectors * g_vol.bps) / 1024;
}

int app_fat_list_images(fat_scan_entry_t *out, int maxn) {
    if (!g_sd_ok) return 0;
    return fat_scan_img(&g_vol, out, maxn);
}

bool app_fat_read_image_block(const char *base8, const char *ext3, uint32_t block_idx, uint8_t *buf) {
    if (!g_sd_ok) return false;
    fat_file_t f;
    if (!fat_open_img(&g_vol, base8, ext3, &f, NULL, 0)) return false;
    return fat_read_block(&f, block_idx, buf);
}

uint32_t app_fat_image_size(const char *base8, const char *ext3) {
    if (!g_sd_ok) return 0;
    fat_file_t f;
    if (!fat_open_img(&g_vol, base8, ext3, &f, NULL, 0)) return 0;
    return f.size;
}