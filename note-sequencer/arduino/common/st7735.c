#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <avr/io.h>
#include "util.h"
#include "spi.h"
#include "st7735.h"

// GPIO pins connected to ST7735
#define PORTB_RESET_PIN 1
#define PORTB_DATA_COMMAND_PIN 0

// Note that this happens to be the same pin that the Atmega uses for it's own
// chip select when configured as a slave device (which it is not in this
// case). This is a coincidence.
#define PORTB_TFT_CHIP_SELECT_PIN 2

static void reset_low(void) {
  PORTB &= ~BIT(PORTB_RESET_PIN);
}

static void reset_high(void) {
  PORTB |= BIT(PORTB_RESET_PIN);
}

static void command_mode(void) {
  PORTB &= ~BIT(PORTB_DATA_COMMAND_PIN);
}

static void data_mode(void) {
  PORTB |= BIT(PORTB_DATA_COMMAND_PIN);
}

static void hard_reset(void) {
  reset_low();
  delay_ms(20);
  reset_high();
  delay_ms(200);
}

static void select_tft(void) {
  PORTB &= ~BIT(PORTB_TFT_CHIP_SELECT_PIN);
}

static void deselect_tft(void) {
  PORTB |= BIT(PORTB_TFT_CHIP_SELECT_PIN);
}

// Definitions and init sequences for st77xx display devices copied from:
// https://github.com/adafruit/Adafruit-ST7735-Library

#define ST7735_TFTWIDTH_128 128  // for 1.44 and mini
#define ST7735_TFTWIDTH_80 80    // for mini
#define ST7735_TFTHEIGHT_128 128 // for 1.44" display
#define ST7735_TFTHEIGHT_160 160 // for 1.8" and mini display

#define ST_CMD_DELAY 0x80 // special signifier for command lists

#define ST77XX_NOP 0x00
#define ST77XX_SWRESET 0x01
#define ST77XX_RDDID 0x04
#define ST77XX_RDDST 0x09

#define ST77XX_SLPIN 0x10
#define ST77XX_SLPOUT 0x11
#define ST77XX_PTLON 0x12
#define ST77XX_NORON 0x13

#define ST77XX_INVOFF 0x20
#define ST77XX_INVON 0x21
#define ST77XX_DISPOFF 0x28
#define ST77XX_DISPON 0x29
#define ST77XX_CASET 0x2A
#define ST77XX_RASET 0x2B
#define ST77XX_RAMWR 0x2C
#define ST77XX_RAMRD 0x2E

#define ST77XX_PTLAR 0x30
#define ST77XX_TEOFF 0x34
#define ST77XX_TEON 0x35
#define ST77XX_MADCTL 0x36
#define ST77XX_COLMOD 0x3A

#define ST77XX_MADCTL_MY 0x80
#define ST77XX_MADCTL_MX 0x40
#define ST77XX_MADCTL_MV 0x20
#define ST77XX_MADCTL_ML 0x10
#define ST77XX_MADCTL_RGB 0x00

#define ST77XX_RDID1 0xDA
#define ST77XX_RDID2 0xDB
#define ST77XX_RDID3 0xDC
#define ST77XX_RDID4 0xDD

// Some register settings
#define ST7735_MADCTL_BGR 0x08
#define ST7735_MADCTL_MH 0x04

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR 0xB4
#define ST7735_DISSET5 0xB6

#define ST7735_PWCTR1 0xC0
#define ST7735_PWCTR2 0xC1
#define ST7735_PWCTR3 0xC2
#define ST7735_PWCTR4 0xC3
#define ST7735_PWCTR5 0xC4
#define ST7735_VMCTR1 0xC5

#define ST7735_PWCTR6 0xFC

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

static void send_command(uint8_t command, uint8_t *args, uint8_t num_args) {
  select_tft();
  command_mode();
  spi_send(command);
  data_mode();
  for (uint8_t i = 0; i < num_args; i++) {
    spi_send(args[i]);
  }
  deselect_tft();
}

static void interpret_commands(uint8_t *commands) {
  uint8_t num_commands = *(commands++);
  for (int i = 0; i < num_commands; i++) {
    uint8_t command = *(commands++);
    uint8_t num_args_raw = *(commands++);
    uint8_t num_args = num_args_raw & ~ST_CMD_DELAY;
    bool has_delay = (num_args_raw & ST_CMD_DELAY) != 0;
    send_command(command, commands, num_args);
    commands += num_args;
    if (has_delay) {
      uint16_t ms = (uint16_t)(*(commands++));
      // In the command DSL the maximum value is 255. Interpret 255 as a
      // special period which actually represents 500ms.
      if (ms == 255) {
        ms = 500;
      }
      delay_ms(ms);
    }
  }
}

static uint8_t rcmd1[] = {          // 7735R init, part 1 (red or green tab)
    15,                             // 15 commands in list:
    ST77XX_SWRESET,   ST_CMD_DELAY, //  1: Software reset, 0 args, w/delay
      150,                          //     150 ms delay
    ST77XX_SLPOUT,    ST_CMD_DELAY, //  2: Out of sleep mode, 0 args, w/delay
      255,                          //     500 ms delay
    ST7735_FRMCTR1, 3,              //  3: Framerate ctrl - normal mode, 3 arg:
      0x01, 0x2C, 0x2D,             //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR2, 3,              //  4: Framerate ctrl - idle mode, 3 args:
      0x01, 0x2C, 0x2D,             //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR3, 6,              //  5: Framerate - partial mode, 6 args:
      0x01, 0x2C, 0x2D,             //     Dot inversion mode
      0x01, 0x2C, 0x2D,             //     Line inversion mode
    ST7735_INVCTR,  1,              //  6: Display inversion ctrl, 1 arg:
      0x07,                         //     No inversion
    ST7735_PWCTR1,  3,              //  7: Power control, 3 args, no delay:
      0xA2,
      0x02,                         //     -4.6V
      0x84,                         //     AUTO mode
    ST7735_PWCTR2,  1,              //  8: Power control, 1 arg, no delay:
      0xC5,                         //     VGH25=2.4C VGSEL=-10 VGH=3 * AVDD
    ST7735_PWCTR3,  2,              //  9: Power control, 2 args, no delay:
      0x0A,                         //     Opamp current small
      0x00,                         //     Boost frequency
    ST7735_PWCTR4,  2,              // 10: Power control, 2 args, no delay:
      0x8A,                         //     BCLK/2,
      0x2A,                         //     opamp current small & medium low
    ST7735_PWCTR5,  2,              // 11: Power control, 2 args, no delay:
      0x8A, 0xEE,
    ST7735_VMCTR1,  1,              // 12: Power control, 1 arg, no delay:
      0x0E,
    ST77XX_INVOFF,  0,              // 13: Don't invert display, no args
    ST77XX_MADCTL,  1,              // 14: Mem access ctl (directions), 1 arg:
      0xC8,                         //     row/col addr, bottom-top refresh
    ST77XX_COLMOD,  1,              // 15: set color mode, 1 arg, no delay:
      0x05 };                       //     16-bit color

static uint8_t rcmd2_green144[] = { // 7735R init, part 2 (green 1.44 tab)
    2,                              //  2 commands in list:
    ST77XX_CASET,   4,              //  1: Column addr set, 4 args, no delay:
      0x00, 0x00,                   //     XSTART = 0
      0x00, 0x7F,                   //     XEND = 127
    ST77XX_RASET,   4,              //  2: Row addr set, 4 args, no delay:
      0x00, 0x00,                   //     XSTART = 0
      0x00, 0x7F };                 //     XEND = 127

static uint8_t rcmd3[] = {                 // 7735R init, part 3 (red or green tab)
    4,                              //  4 commands in list:
    ST7735_GMCTRP1, 16      ,       //  1: Gamma Adjustments (pos. polarity), 16 args + delay:
      0x02, 0x1c, 0x07, 0x12,       //     (Not entirely necessary, but provides
      0x37, 0x32, 0x29, 0x2d,       //      accurate colors)
      0x29, 0x25, 0x2B, 0x39,
      0x00, 0x01, 0x03, 0x10,
    ST7735_GMCTRN1, 16      ,       //  2: Gamma Adjustments (neg. polarity), 16 args + delay:
      0x03, 0x1d, 0x07, 0x06,       //     (Not entirely necessary, but provides
      0x2E, 0x2C, 0x29, 0x2D,       //      accurate colors)
      0x2E, 0x2E, 0x37, 0x3F,
      0x00, 0x00, 0x02, 0x10,
    ST77XX_NORON,     ST_CMD_DELAY, //  3: Normal display on, no args, w/delay
      10,                           //     10 ms delay
    ST77XX_DISPON,    ST_CMD_DELAY, //  4: Main screen turn on, no args w/delay
      100 };                        //     100 ms delay

static uint8_t cursor_x;
static uint8_t cursor_y;
static window_t current_window;

void st7735_init(void) {

  printf("Starting ST7735 init...\n\r");

  spi_init();

  // Configure GPIO pins.
  DDRB |= BIT(PORTB_TFT_CHIP_SELECT_PIN) | BIT(PORTB_RESET_PIN) | BIT(PORTB_DATA_COMMAND_PIN);

  deselect_tft();
  data_mode();

  hard_reset();

  interpret_commands(rcmd1);
  interpret_commands(rcmd2_green144);
  interpret_commands(rcmd3);

  uint8_t madctl =  ST77XX_MADCTL_MV | ST77XX_MADCTL_MY | ST7735_MADCTL_BGR;
  send_command(ST77XX_MADCTL, &madctl, 1);

  printf("Initialized ST7735!\n\r");
}

#define PAD_X 3
#define PAD_Y 2

void st7735_prepare_for_window(window_t window) {
  uint8_t args[4] = { 0 };

  // Actual display appears to begin at an offset.
  window.x += PAD_X;
  window.y += PAD_Y;
  current_window = window;

  args[1] = window.x;
  args[3] = window.x + window.w - 1;
  if (args[3] >= ST7735_WIDTH + PAD_X) {
    args[3] = ST7735_WIDTH - 1 + PAD_X;
  }
  send_command(ST77XX_CASET, args, 4);

  args[1] = window.y;
  args[3] = window.y + window.h - 1;
  if (args[3] >= ST7735_HEIGHT + PAD_Y) {
    args[3] = ST7735_HEIGHT - 1 + PAD_Y;
  }
  send_command(ST77XX_RASET, args, 4);

  send_command(ST77XX_RAMWR, NULL, 0);
  select_tft();

  cursor_x = window.x;
  cursor_y = window.y;
}

void st7735_send_colour(uint16_t colour) {
  if (cursor_x < ST7735_WIDTH + PAD_X && cursor_y < ST7735_HEIGHT + PAD_Y) {
    spi_send((uint8_t)(colour >> 8));
    spi_send((uint8_t)colour);
  }
  cursor_x++;
  if (cursor_x == current_window.x + current_window.w) {
    cursor_x = current_window.x;
    cursor_y++;
  }
}

void st7735_finalize(void) {
  deselect_tft();
}
