/*
 * FS2011 Pro
 * LCD interface
 *
 * (C) 2022 Gissio
 *
 * License: MIT
 */

#include <stdio.h>
#include <string.h>

#ifndef SDL_MODE
#include "main.h"
#endif

#include "u8g2/u8g2.h"

#include "resources/font_helvR08.h"
#include "resources/font_helvR24.h"
#include "resources/font_icons.h"
#include "resources/font_tiny5.h"

#include "confidence.h"
#include "display.h"
#include "format.h"
#include "measurements.h"
#include "power.h"
#include "settings.h"

#include "resources/dotted4.xbm"

#define LCD_WIDTH 128
#define LCD_HEIGHT 64

#define LCD_CENTER_X (LCD_WIDTH / 2)
#define LCD_CENTER_Y (LCD_HEIGHT / 2)

#define TITLE_Y (LCD_CENTER_Y - 19)
#define SUBTITLE_Y (LCD_CENTER_Y + 19 + 5)

#define MEASUREMENT_VALUE_X (LCD_CENTER_X - 54)
#define MEASUREMENT_VALUE_Y (LCD_CENTER_Y + 24 / 2)
#define MEASUREMENT_VALUE_SIDE_X (LCD_CENTER_X + 29)

#define HISTORY_VIEW_X ((LCD_WIDTH - HISTORY_VIEW_WIDTH) / 2)
#define HISTORY_VIEW_Y_TOP 14
#define HISTORY_VIEW_Y_BOTTOM (HISTORY_VIEW_Y_TOP + HISTORY_VIEW_HEIGHT)

#define STATS_VIEW_X 66
#define STATS_VIEW_Y 31

#define GAME_VIEW_BOARD_X 0
#define GAME_VIEW_BOARD_Y 8
#define GAME_VIEW_BOARD_WIDTH 9 * 8
#define GAME_VIEW_TIME_X (GAME_VIEW_BOARD_WIDTH + 7)
#define GAME_VIEW_TIME_UPPER_Y 8
#define GAME_VIEW_TIME_LOWER_Y (LCD_HEIGHT - 3)
#define GAME_VIEW_MOVES_LINE_WIDTH 25
#define GAME_VIEW_MOVES_LINE_HEIGHT 6
#define GAME_VIEW_MOVES_X (GAME_VIEW_BOARD_WIDTH + 6)
#define GAME_VIEW_MOVES_Y (LCD_CENTER_Y + 4 - GAME_VIEW_MOVES_LINE_HEIGHT * GAME_MOVES_LINE_NUM / 2)
#define GAME_VIEW_BUTTON_X 101
#define GAME_VIEW_BUTTON_Y 54
#define GAME_VIEW_BUTTON_WIDTH 23
#define GAME_VIEW_BUTTON_HEIGHT 9

#define MENU_VIEW_Y_TOP 14
#define MENU_VIEW_LINE_HEIGHT 12
#define MENU_VIEW_LINE_TEXT_X 6

u8g2_t u8g2;

const char *const firmwareName = "FS2011 Pro";
const char *const firmwareVersion = "1.0.2";

#ifndef SDL_MODE
uint8_t onDisplayMessage(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_DELAY_MILLI:
        HAL_Delay(arg_int);
        break;

    case U8X8_MSG_GPIO_RESET:
        HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, arg_int);
        break;

    default:
        return 0;
    }

    return 1;
}

uint8_t onDisplayByte(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_BYTE_SET_DC:
        HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, arg_int);
        break;

    case U8X8_MSG_BYTE_SEND:
    {
        uint8_t *p = (uint8_t *)arg_ptr;
        for (int i = 0; i < arg_int; i++, p++)
        {
            uint8_t value = *p;
            GPIOA->BSRR = (0b1001111100000000 << 16) |
                          ((value << 8) & 0b1001111100000000);
            GPIOF->BSRR = (0b0000000011000000 << 16) |
                          ((value << 1) & 0b0000000011000000);

            asm("nop");
            LCD_EN_GPIO_Port->BSRR = LCD_EN_Pin;
            asm("nop");
            LCD_EN_GPIO_Port->BRR = LCD_EN_Pin;
        }

        break;
    }

    default:
        return 0;
    }

    return 1;
}

static const u8x8_display_info_t dg128064_display_info =
    {
        /* chip_enable_level = */ 0,
        /* chip_disable_level = */ 1,

        /* post_chip_enable_wait_ns = */ 150, /* */
        /* pre_chip_disable_wait_ns = */ 50,  /* */
        /* reset_pulse_width_ms = */ 1,
        /* post_reset_wait_ms = */ 1,
        /* sda_setup_time_ns = */ 50,   /* */
        /* sck_pulse_width_ns = */ 120, /* */
        /* sck_clock_hz = */ 4000000UL, /* */
        /* spi_mode = */ 0,             /* active high, rising edge */
        /* i2c_bus_clock_100kHz = */ 4,
        /* data_setup_time_ns = */ 40,   /* */
        /* write_pulse_width_ns = */ 80, /* */
        /* tile_width = */ 16,           /* width of 16*8=128 pixel */
        /* tile_hight = */ 8,
        /* default_x_offset = */ 0,
        /* flipmode_x_offset = */ 4,
        /* pixel_width = */ 128,
        /* pixel_height = */ 64,
};

static const uint8_t dg128064_init_seq[] = {

    U8X8_START_TRANSFER(), /* enable chip, delay is part of the transfer start */

    U8X8_C(0x0e2), /* soft reset */
    U8X8_C(0x0ae), /* display off */
    U8X8_C(0x040), /* set display start line to 0 */

    U8X8_C(0x0a0), /* ADC set to reverse */
    U8X8_C(0x0c8), /* common output mode */

    U8X8_C(0x0a6), /* display normal, bit val 0: LCD pixel off. */
    U8X8_C(0x0a2), /* LCD bias 1/9 */

    /* power on sequence from paxinstruments */
    U8X8_C(0x028 | 4), /* all power control circuits on */
    U8X8_DLY(50),
    U8X8_C(0x028 | 6), /* all power control circuits on */
    U8X8_DLY(50),
    U8X8_C(0x028 | 7), /* all power control circuits on */
    U8X8_DLY(50),

    U8X8_C(0x023),      /* v0 voltage resistor ratio */
    U8X8_CA(0x081, 36), /* set contrast, contrast value*/

    U8X8_C(0x0ae), /* display off */
    U8X8_C(0x0a5), /* enter powersafe: all pixel on, issue 142 */

    U8X8_END_TRANSFER(), /* disable chip */
    U8X8_END()           /* end of sequence */
};

static const uint8_t st7567_132x64_powersave0_seq[] = {
    U8X8_START_TRANSFER(), /* enable chip, delay is part of the transfer start */
    U8X8_C(0x0a4),         /* all pixel off, issue 142 */
    U8X8_C(0x0af),         /* display on */
    U8X8_END_TRANSFER(),   /* disable chip */
    U8X8_END()             /* end of sequence */
};

static const uint8_t st7567_132x64_powersave1_seq[] = {
    U8X8_START_TRANSFER(), /* enable chip, delay is part of the transfer start */
    U8X8_C(0x0ae),         /* display off */
    U8X8_C(0x0a5),         /* enter powersafe: all pixel on, issue 142 */
    U8X8_END_TRANSFER(),   /* disable chip */
    U8X8_END()             /* end of sequence */
};

uint8_t setupU8X8(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    uint8_t x, c;
    uint8_t *ptr;

    switch (msg)
    {
    case U8X8_MSG_DISPLAY_SETUP_MEMORY:
        u8x8_d_helper_display_setup_memory(u8x8, &dg128064_display_info);
        break;

    case U8X8_MSG_DISPLAY_INIT:
        u8x8_d_helper_display_init(u8x8);
        u8x8_cad_SendSequence(u8x8, dg128064_init_seq);
        break;

    case U8X8_MSG_DISPLAY_SET_POWER_SAVE:
        if (arg_int == 0)
            u8x8_cad_SendSequence(u8x8, st7567_132x64_powersave0_seq);
        else
            u8x8_cad_SendSequence(u8x8, st7567_132x64_powersave1_seq);
        break;

    case U8X8_MSG_DISPLAY_DRAW_TILE:
        u8x8_cad_StartTransfer(u8x8);

        x = ((u8x8_tile_t *)arg_ptr)->x_pos;
        x *= 8;
        x += u8x8->x_offset;
        u8x8_cad_SendCmd(u8x8, 0x010 | (x >> 4));
        u8x8_cad_SendCmd(u8x8, 0x000 | ((x & 15)));
        u8x8_cad_SendCmd(u8x8, 0x0b0 | (((u8x8_tile_t *)arg_ptr)->y_pos));

        c = ((u8x8_tile_t *)arg_ptr)->cnt;
        c *= 8;
        ptr = ((u8x8_tile_t *)arg_ptr)->tile_ptr;
        /* The following if condition checks the hardware limits of the st7567
         * controller: It is not allowed to write beyond the display limits.
         * This is in fact an issue within flip mode.
         */
        if (c + x > 132u)
        {
            c = 132u;
            c -= x;
        }
        do
        {
            /* note: SendData can not handle more than 255 bytes */
            u8x8_cad_SendData(u8x8, c, ptr);
            arg_int--;
        } while (arg_int > 0);

        u8x8_cad_EndTransfer(u8x8);

        break;

    default:
        return 0;
    }

    return 1;
}

void setupU8G2()
{
    uint8_t tile_buf_height;
    uint8_t *buf;
    u8g2_SetupDisplay(&u8g2, setupU8X8, u8x8_cad_001, onDisplayByte, onDisplayMessage);
    buf = u8g2_m_16_8_f(&tile_buf_height);
    u8g2_SetupBuffer(&u8g2, buf, tile_buf_height, u8g2_ll_hvline_vertical_top_lsb, U8G2_R0);
}

#endif

void initDisplay()
{
#ifdef SDL_MODE
    u8g2_SetupBuffer_SDL_128x64(&u8g2, U8G2_R0);
#else
    // u8g2_Setup_st7567_enh_dg128064_f(&u8g2, U8G2_R0, onDisplayByte, onDisplayMessage);
    setupU8G2();
#endif

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
}

void setDisplay(bool value)
{
    u8g2_SetPowerSave(&u8g2, !value);
}

void clearDisplay()
{
    u8g2_ClearBuffer(&u8g2);
}

void updateDisplay()
{
    u8g2_SendBuffer(&u8g2);
}

void drawTextLeft(const char *str, int x, int y)
{
    u8g2_DrawStr(&u8g2, x, y, str);
}

void drawTextCenter(const char *str, int x, int y)
{
    u8g2_DrawStr(&u8g2, x - u8g2_GetStrWidth(&u8g2, str) / 2, y, str);
}

void drawTextRight(const char *str, int x, int y)
{
    u8g2_DrawStr(&u8g2, x - u8g2_GetStrWidth(&u8g2, str), y, str);
}

void drawSelfTestError(unsigned int value)
{
    char subtitle[32];
    strcpy(subtitle, "Self-test failed: ");
    formatHex(value, subtitle + 18);

    u8g2_SetFont(&u8g2, font_helvR08);
    drawTextCenter(firmwareName, LCD_CENTER_X, LCD_CENTER_Y - 3);

    u8g2_SetFont(&u8g2, font_tiny5);
    drawTextCenter(subtitle, LCD_CENTER_X, LCD_CENTER_Y + 11);
}

void drawWelcome()
{
    u8g2_SetFont(&u8g2, font_helvR08);
    drawTextCenter(firmwareName, LCD_CENTER_X, LCD_CENTER_Y - 3);

    u8g2_SetFont(&u8g2, font_tiny5);
    drawTextCenter(firmwareVersion, LCD_CENTER_X, LCD_CENTER_Y + 11);
}

void drawStatusBar()
{
    signed char batteryLevel = getBatteryLevel();

    if (batteryLevel < 0)
        return;

    char statusBar[8];
    char alarmIcon = (settings.rateAlarm || settings.doseAlarm) ? '<' : ' ';

    char batteryIcon = (batteryLevel == BATTERY_LEVEL_CHARGING) ? ':' : '0' + batteryLevel;
    sprintf(statusBar, "%c?%c", alarmIcon, batteryIcon);

    u8g2_SetFont(&u8g2, font_icons);

    drawTextRight(statusBar, LCD_WIDTH - 4, 8);
}

void drawTitle(const char *title)
{
    u8g2_SetFont(&u8g2, font_tiny5);
    drawTextCenter(title, LCD_CENTER_X, TITLE_Y);
}

void drawSubtitle(const char *subtitle)
{
    u8g2_SetFont(&u8g2, font_tiny5);
    drawTextCenter(subtitle, LCD_CENTER_X, SUBTITLE_Y);
}

void drawMeasurementValue(const char *mantissa, const char *characteristic)
{
    // Value
    u8g2_SetFont(&u8g2, font_helvR24);
    drawTextLeft(mantissa, MEASUREMENT_VALUE_X, MEASUREMENT_VALUE_Y);

    // Units
    u8g2_SetFont(&u8g2, font_helvR08);
    drawTextLeft(characteristic, MEASUREMENT_VALUE_SIDE_X, MEASUREMENT_VALUE_Y - 16);
}

void drawConfidenceIntervals(int sampleNum)
{
    if (sampleNum == 0)
        return;

    int lowerConfidenceInterval;
    int upperConfidenceInterval;
    getConfidenceIntervals(sampleNum, &lowerConfidenceInterval, &upperConfidenceInterval);

    u8g2_SetFont(&u8g2, font_tiny5);

    char confidenceInterval[16];

    sprintf(confidenceInterval, "+%d%%", upperConfidenceInterval);
    drawTextLeft(confidenceInterval, MEASUREMENT_VALUE_SIDE_X, MEASUREMENT_VALUE_Y - 7);

    sprintf(confidenceInterval, "-%d%%", lowerConfidenceInterval);
    drawTextLeft(confidenceInterval, MEASUREMENT_VALUE_SIDE_X, MEASUREMENT_VALUE_Y);
}

void drawHistory(const char *minLabel, const char *maxLabel,
                 int offset, int range)
{
    // Plot separators
    u8g2_DrawHLine(&u8g2, 0, HISTORY_VIEW_Y_TOP, LCD_WIDTH);
    u8g2_DrawHLine(&u8g2, 0, HISTORY_VIEW_Y_BOTTOM, LCD_WIDTH);

    // Data
    // uint8_t lastX = LCD_WIDTH;
    // uint8_t lastY = data[0];
    for (int i = 0; i < HISTORY_BUFFER_SIZE; i++)
    {
        int value = getHistoryDataPoint(i);
        if (value == 0)
            continue;

        int x = (LCD_WIDTH - 1) - i * LCD_WIDTH / HISTORY_BUFFER_SIZE;
        int y = (value + offset) * HISTORY_VALUE_DECADE / HISTORY_VIEW_HEIGHT / range;

        // Pixel
        // u8g2_DrawPixel(&u8g2, x, HISTORY_VIEW_Y_BOTTOM - y);
        // Solid
        u8g2_DrawVLine(&u8g2, x, HISTORY_VIEW_Y_BOTTOM - y, y);
        // Line
        // u8g2_DrawLine(&u8g2, x, HISTORY_VIEW_Y_BOTTOM - y,
        //               lastX, HISTORY_VIEW_Y_BOTTOM - lastY);
        // lastX = x;
        // lastY = y;
    }

    // Time divisors
    u8g2_SetDisplayRotation(&u8g2, U8G2_R1);
    for (int i = 0; i < 7; i++)
    {
        int x = (i + 1) * LCD_WIDTH / 8 - 1;
        u8g2_DrawXBM(&u8g2, HISTORY_VIEW_Y_TOP + 1, x, HISTORY_VIEW_HEIGHT - 1, 1, dotted4_bits);
    }
    u8g2_SetDisplayRotation(&u8g2, U8G2_R0);

    // Value divisors
    for (int i = 0; i < (range - 1); i++)
    {
        int y = HISTORY_VIEW_Y_TOP + (i + 1) * HISTORY_VIEW_HEIGHT / range;
        u8g2_DrawXBM(&u8g2, 0, y, LCD_WIDTH, 1, dotted4_bits);
    }

    u8g2_SetFont(&u8g2, font_tiny5);

    drawTextLeft(minLabel, 1, HISTORY_VIEW_Y_BOTTOM + 1 + 6);
    drawTextLeft(maxLabel, 1, HISTORY_VIEW_Y_TOP - 1);
}

void drawStats()
{
    char data[32];
    char line[64];

    u8g2_SetFont(&u8g2, font_tiny5);

    formatTime(settings.lifeTimer, data);
    sprintf(line, "Life timer: %s", data);
    drawTextCenter(line, LCD_CENTER_X, STATS_VIEW_Y);

    formatUnsignedLongLong(settings.lifeCounts, data);
    sprintf(line, "Life counts: %s", data);
    drawTextCenter(line, LCD_CENTER_X, STATS_VIEW_Y + 7);
}

void drawGameBoard(const char board[8][9],
                   const char time[2][6],
                   const char moveHistory[GAME_MOVES_LINE_NUM][2][6],
                   const char *buttonText, bool buttonSelected)
{
    u8g2_SetFont(&u8g2, font_icons);
    for (int y = 0; y < 8; y++)
        drawTextLeft(board[y], GAME_VIEW_BOARD_X, GAME_VIEW_BOARD_Y + 8 * y);

    u8g2_SetFont(&u8g2, font_tiny5);
    drawTextLeft(time[0], GAME_VIEW_TIME_X, GAME_VIEW_TIME_UPPER_Y);
    drawTextLeft(time[1], GAME_VIEW_TIME_X, GAME_VIEW_TIME_LOWER_Y);

    for (int y = 0; y < GAME_MOVES_LINE_NUM; y++)
    {
        for (int x = 0; x < 2; x++)
        {
            drawTextLeft(moveHistory[y][x],
                         GAME_VIEW_MOVES_X + GAME_VIEW_MOVES_LINE_WIDTH * x,
                         GAME_VIEW_MOVES_Y + GAME_VIEW_MOVES_LINE_HEIGHT * y);
        }
    }

    if (buttonText)
    {
        if (buttonSelected)
            u8g2_DrawRBox(&u8g2, GAME_VIEW_BUTTON_X, GAME_VIEW_BUTTON_Y,
                          GAME_VIEW_BUTTON_WIDTH, GAME_VIEW_BUTTON_HEIGHT, 1);
        else
            u8g2_DrawRFrame(&u8g2, GAME_VIEW_BUTTON_X, GAME_VIEW_BUTTON_Y,
                            GAME_VIEW_BUTTON_WIDTH, GAME_VIEW_BUTTON_HEIGHT, 1);

        u8g2_SetDrawColor(&u8g2, !buttonSelected);
        drawTextCenter(buttonText, GAME_VIEW_BUTTON_X + GAME_VIEW_BUTTON_WIDTH / 2, GAME_VIEW_BUTTON_Y + 7);

        u8g2_SetDrawColor(&u8g2, 1);
    }
}

void drawMenu(GetMenuOption getMenuOption, void *userdata, int startIndex, int selectedIndex)
{
    u8g2_SetFont(&u8g2, font_helvR08);

    for (int i = 0, y = MENU_VIEW_Y_TOP;
         i < MENU_VIEW_LINE_NUM;
         i++, y += MENU_VIEW_LINE_HEIGHT)
    {
        uint8_t index = startIndex + i;

        const char *name = getMenuOption(userdata, index);
        if (!name)
            break;

        uint8_t selected = (index == selectedIndex);

        u8g2_SetDrawColor(&u8g2, selected);
        u8g2_DrawBox(&u8g2, 0, y, LCD_WIDTH, MENU_VIEW_LINE_HEIGHT);

        u8g2_SetDrawColor(&u8g2, !selected);
        drawTextLeft(name, MENU_VIEW_LINE_TEXT_X, y + MENU_VIEW_LINE_HEIGHT - 2);
    }

    u8g2_SetDrawColor(&u8g2, 1);
}
