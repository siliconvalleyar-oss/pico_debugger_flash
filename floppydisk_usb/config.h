#pragma once

/* =============================================================================
 * floppydisk — Raspberry Pi Pico 2 W (RP2350) 3.5" floppy reader/writer
 *
 * Pin map, format and MFM timing configuration.
 * ============================================================================= */

/* ------------------------------------------------------------------ CPU ---- */
/* RP2350 @ ~150 MHz.  The PIOs are clocked so that the MFM flux unit (1 T)
 * equals 24 MHz loop rate: one flux counter unit = 1/24e6 s ≈ 41.67 ns.        */

/* --------------------------------------------------------- Debug UART ------ */
/* stdio UART0, 115200 8N1                                                        */
#define UART_DEBUG_TX_PIN 0
#define UART_DEBUG_RX_PIN 1

/* ------------------------------------------------------------ Buttons ------ */
/* Active-low tactile buttons (internal pull-ups enabled)                       */
#define BTN_A_PIN 2
#define BTN_B_PIN 3

/* --------------------------------------------------------------- OLED ------ */
/* SSD1306 128x64 over I2C0                                                     */
#define OLED_I2C i2c0
#define OLED_SDA_PIN 4
#define OLED_SCL_PIN 5
#define OLED_ADDR 0x3C
#define OLED_I2C_FREQ 400000

/* -------------------------------------------------- Floppy 34-pin FDD ------ */
/* 34-pin IDC header signals (odd-numbered pins), split over the pins below.   */
/* Drive outputs (INDEX, RDATA, TRK0, WPT, DCHG) are open-collector; the 3V3   */
/* Pico is NOT 5V tolerant — see docs/hardware.md before wiring!              */
#define FLOPPY_DENSEL_PIN 6  /* pin 1  DENSEL (1=high density for PC drives)    */
#define FLOPPY_INDEX_PIN 7   /* pin 7  INDEX (input, active high pulse)         */
#define FLOPPY_MOTOR_PIN 8   /* pin 9  MOTOR (active low)                       */
#define FLOPPY_DS0_PIN 9     /* pin 11 DS0    (active low, select drive 0)      */
#define FLOPPY_STEP_PIN 14   /* pin 19 STEP   (step head)                       */
#define FLOPPY_DIR_PIN 15    /* pin 17 DIR    (1=out toward track 0)            */
#define FLOPPY_WDATA_PIN 16  /* pin 21 WDATA  (PIO write data)                  */
#define FLOPPY_WG_PIN 17     /* pin 23 WG     (write gate, active low)          */
#define FLOPPY_TRK0_PIN 18   /* pin 25 TRK0   (input, active low)               */
#define FLOPPY_WPT_PIN 19    /* pin 27 WPT    (input, active low)               */
#define FLOPPY_RDATA_PIN 20  /* pin 29 RDATA  (PIO read data / jmp pin)         */
#define FLOPPY_HS_PIN 21     /* pin 31 HS     (1=head 0, 0=head 1)              */
#define FLOPPY_DCHG_PIN 22   /* pin 33 DCHG   (input, active low)               */
#define FLOPPY_DS1_PIN 26    /* pin 13 DS1    (active low, second drive, unused */
#define FLOPPY_LED_PIN 28    /* optional user status LED                        */

/* Drive timing (3.5" drives)                                                  */
#define FLOPPY_STEP_DELAY_US 10000 /* <10 ms between steps for 3.5"            */
#define FLOPPY_SETTLE_DELAY_MS 20  /* settle after a seek                       */
#define FLOPPY_MOTOR_DELAY_MS 200  /* motor spin-up time                        */
#define FLOPPY_SELECT_DELAY_US 20
#define FLOPPY_WG_SETUP_US 50      /* WGATE settle before PIO clocks data      */

/* -------------------------------------------------------------- microSD ---- */
#define SD_SPI spi1
#define SD_SCK_PIN 10
#define SD_MOSI_PIN 11
#define SD_MISO_PIN 12
#define SD_CS_PIN 13
#define SD_INIT_BAUD 400000   /* init clock (SCK)                              */
#define SD_RUN_BAUD 12500000  /* run-time clock (12.5 MHz for better SDSC compat) */

/* ------------------------------------------------------------- Formats ----- */
#define FLOPPY_HEADS 2
#define FLOPPY_HD_TRACKS 80
#define FLOPPY_DD_TRACKS 80
#define FLOPPY_HD_SPT 18   /* sectors per track, 1.44 MB (500 kbit/s MFM)      */
#define FLOPPY_DD_SPT 9    /* sectors per track, 720 KB  (250 kbit/s MFM)      */
#define MFM_SECTOR_SIZE 512

#define IMAGE_HD_SIZE (FLOPPY_HD_TRACKS * FLOPPY_HEADS * FLOPPY_HD_SPT * MFM_SECTOR_SIZE) /* 1474560 */
#define IMAGE_DD_SIZE (FLOPPY_DD_TRACKS * FLOPPY_HEADS * FLOPPY_DD_SPT * MFM_SECTOR_SIZE) /* 737280  */

/* ---------------------------------------------------------- MFM timing ----- */
/* Sample/bit clock, in Hz.  The PIO loops are derived from clk_sys so that    */
/* one count == 1 / MFM_SAMPLE_FREQ seconds.                                   */
#define MFM_SAMPLE_FREQ 24000000u

/* Nominal time of one decoded MFM bit ("1T").  These are the values that      */
/* Adafruit's library uses (and that have been validated on real hardware):    */
/*   HD (500 kbit/s): 1.0 us  ->  T1_nom = 24 counts (cell = 2 us)             */
/*   DD (250 kbit/s): 2.0 us  ->  T1_nom = 48 counts (cell = 4 us)             */
#define MFM_HD_BIT_TIME_US 1.0f
#define MFM_DD_BIT_TIME_US 2.0f

/* Derived classifier thresholds, unit = 24 MHz counts:                        */
/*   T2_max = round(sample * bit_time_us * 2.5e-6)                             */
/*   T3_max = round(sample * bit_time_us * 3.5e-6)                             */

/* -------------------------------------------------------------- Buffers ---- */
/* One shared work buffer used for: captured flux pulses, encoded flux pulses,
 * and (reused) pre-computed write timings.  A 1.44 MB track needs ~50 k pulse
 * bytes (one revolution at 24 MHz), so 128 KiB is enough; the RP2350 keeps a
 * larger buffer for extra headroom.  The RP2040 only has 264 KiB of SRAM,
 * hence the smaller default there.                                          */
#if defined(PICO_RP2040)
#define TRACK_BUF_SIZE 131072u
#else
#define TRACK_BUF_SIZE 262144u
#endif

/* image cluster-chain table (max pre-allocated clusters any single file)      */
#define FAT_MAX_CHAIN 4096u

/* --------------------------------------------------------------- Image ----- */
#define IMG_DIR_NAME "IMG"       /* subdirectory holding *.IMA images (8.3)    */
#define IMG_FILENAME_PREFIX "DISK"