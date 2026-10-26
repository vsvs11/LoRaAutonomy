#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include "config.h"
#include <drivers/atgm336h.h>

/*
 * Default hardware pin definitions fallback
 */
#ifndef ATGM_UART_PORT
#define ATGM_UART_PORT                  UART_NUM_1
#endif
#ifndef ATGM_TX_NUM
#define ATGM_TX_NUM                     21
#endif
#ifndef ATGM_RX_NUM
#define ATGM_RX_NUM                     22
#endif
#ifndef ATGM_RESET_NUM
#define ATGM_RESET_NUM                  (-1)
#endif
#ifndef ATGM_ONOFF_NUM
#define ATGM_ONOFF_NUM                  (-1)
#endif
#ifndef ATGM_PPS_NUM
#define ATGM_PPS_NUM                    18
#endif
#ifndef ATGM_RX_BUF_SIZE
#define ATGM_RX_BUF_SIZE                2048
#endif

/*
 * CASIC (PCAS) Command definitions
 */
#define PCAS_CMD_SAVE_NVM               "$PCAS00*01\r\n"

#define PCAS_CMD_HOT_START              "$PCAS10,0*1C\r\n"
#define PCAS_CMD_WARM_START             "$PCAS10,1*1D\r\n"
#define PCAS_CMD_COLD_START             "$PCAS10,2*1E\r\n"

#define PCAS_CMD_RATE_1HZ               "$PCAS02,1000*2E\r\n"
#define PCAS_CMD_RATE_2HZ               "$PCAS02,500*1A\r\n"
#define PCAS_CMD_RATE_5HZ               "$PCAS02,200*1D\r\n"
#define PCAS_CMD_RATE_10HZ              "$PCAS02,100*1E\r\n"

#define PCAS_CMD_MODE_GPS               "$PCAS04,1*18\r\n"
#define PCAS_CMD_MODE_BDS               "$PCAS04,2*1B\r\n"
#define PCAS_CMD_MODE_GPS_BDS           "$PCAS04,3*1A\r\n"
#define PCAS_CMD_MODE_GLONASS           "$PCAS04,4*1D\r\n"
#define PCAS_CMD_MODE_GPS_GLONASS       "$PCAS04,5*1C\r\n"
#define PCAS_CMD_MODE_BDS_GLONASS       "$PCAS04,6*1F\r\n"
#define PCAS_CMD_MODE_ALL               "$PCAS04,7*1E\r\n"

/*
 * Internal driver parameters
 */
#define ATGM_UART_QUEUE_SIZE            16
#define ATGM_LINE_MAX_LEN               128
#define ATGM_MAX_TOKENS                 24
#define ATGM_PPS_LED_BLINK_US           50000ULL
#define ATGM_PPS_TIMEOUT_US             1200000LL

static int               __baud_rate      = 115200;
static int               __pps_gpio       = -1;
static int               __pps_led_gpio   = -1;
static bool              __initialized    = false;

static atgm_data_t       __nav_data       = {0};
static atgm_pps_t        __pps_state      = {0};
static uint8_t           __cycle_max_snr  = 0;

static SemaphoreHandle_t __data_mutex     = NULL;
static SemaphoreHandle_t __pps_sem        = NULL;
static QueueHandle_t     __uart_queue     = NULL;
static TaskHandle_t      __rx_task_handle = NULL;
static esp_timer_handle_t __led_timer     = NULL;
static portMUX_TYPE      __pps_spinlock   = portMUX_INITIALIZER_UNLOCKED;

/*
 * Calculate NMEA 0183 XOR checksum
 */
static uint8_t atgm_calc_checksum(const char *cmd){
    uint8_t crc = 0;
    if (*cmd == '$') cmd++;
    while (*cmd && *cmd != '*' && *cmd != '\r' && *cmd != '\n') {
        crc ^= (uint8_t)(*cmd++);
    }
    return crc;
}

/*
 * Convert hexadecimal character to integer
 */
static int hex_to_nibble(char c){
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/*
 * Validate incoming NMEA sentence frame and checksum
 */
static bool atgm_validate_sentence(const char *line){
    if (!line || line[0] != '$') return false;
    const char *star = strchr(line, '*');
    if (!star || strlen(star) < 3) return false;

    int hi = hex_to_nibble(star[1]);
    int lo = hex_to_nibble(star[2]);
    if (hi < 0 || lo < 0) return false;

    uint8_t expected = (uint8_t)((hi << 4) | lo);
    return (expected == atgm_calc_checksum(line));
}

/*
 * Convert UTC calendar structure to Unix epoch timestamp
 */
static uint64_t utc_to_epoch(const atgm_utc_t *u){
    if (u->year < 2020 || u->month < 1 || u->month > 12 || u->day < 1 || u->day > 31) {
        return 0;
    }
    static const uint16_t mdays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    uint32_t y = u->year;
    uint32_t days = (y - 1970) * 365 + ((y - 1969) / 4) - ((y - 1901) / 100) + ((y - 1601) / 400);
    days += mdays[u->month - 1];
    if ((((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0)) && u->month > 2) {
        days++;
    }
    days += (u->day - 1);
    return (uint64_t)days * 86400ULL + (uint64_t)u->hour * 3600ULL +
           (uint64_t)u->minute * 60ULL + (uint64_t)u->second;
}

/*
 * Convert NMEA DDMM.MMMMM coordinate string to decimal and scaled integer
 */
static void parse_coordinate(const char *str, char dir, double *deg_out, int32_t *scaled_out){
    if (!str || str[0] == '\0') return;
    double raw = strtod(str, NULL);
    int32_t deg = (int32_t)(raw / 100.0);
    double min = raw - ((double)deg * 100.0);
    double dec = (double)deg + (min / 60.0);
    if (dir == 'S' || dir == 'W') dec = -dec;

    *deg_out = dec;
    *scaled_out = (int32_t)llround(dec * 1000000.0);
}

/*
 * Parse NMEA HHMMSS.SSS time token
 */
static bool parse_time_token(const char *str, atgm_utc_t *utc, uint32_t *raw_out){
    if (!str || strlen(str) < 6) return false;
    double val = strtod(str, NULL);
    uint32_t hms = (uint32_t)val;
    uint16_t ms = (uint16_t)lround((val - (double)hms) * 1000.0);

    uint8_t h = (uint8_t)(hms / 10000U);
    uint8_t m = (uint8_t)((hms / 100U) % 100U);
    uint8_t s = (uint8_t)(hms % 100U);
    if (h > 23 || m > 59 || s > 60) return false;

    utc->hour = h;
    utc->minute = m;
    utc->second = s;
    utc->millisecond = (ms < 1000) ? ms : 999;
    *raw_out = hms;
    return true;
}

/*
 * Parse NMEA DDMMYY date token
 */
static bool parse_date_token(const char *str, atgm_utc_t *utc, uint32_t *raw_out){
    if (!str || strlen(str) < 6) return false;
    uint32_t dmy = (uint32_t)strtoul(str, NULL, 10);
    uint8_t d = (uint8_t)(dmy / 10000U);
    uint8_t m = (uint8_t)((dmy / 100U) % 100U);
    uint16_t y = (uint16_t)(2000U + (dmy % 100U));
    if (d < 1 || d > 31 || m < 1 || m > 12) return false;

    utc->day = d;
    utc->month = m;
    utc->year = y;
    *raw_out = dmy;
    return true;
}

/*
 * Split NMEA sentence by comma delimiter in-place
 */
static int tokenize_nmea(char *line, char **tokens, int max_tokens){
    char *star = strchr(line, '*');
    if (star) *star = '\0';

    int count = 0;
    char *ptr = line;
    while (ptr && count < max_tokens) {
        tokens[count++] = ptr;
        char *comma = strchr(ptr, ',');
        if (!comma) break;
        *comma = '\0';
        ptr = comma + 1;
    }
    return count;
}

/*
 * Parse $GNGGA / $GPGGA sentence
 */
static void parse_gga(char **tok, int count){
    if (count < 10) return;
    parse_time_token(tok[1], &__nav_data.utc, &__nav_data.raw_time);

    uint8_t fix = (uint8_t)atoi(tok[6]);
    __nav_data.fix_quality = fix;

    if (fix > 0 && tok[2][0] != '\0' && tok[4][0] != '\0') {
        __nav_data.fix_valid = true;
        parse_coordinate(tok[2], tok[3][0], &__nav_data.latitude, &__nav_data.lat_scaled);
        parse_coordinate(tok[4], tok[5][0], &__nav_data.longitude, &__nav_data.lon_scaled);
        __nav_data.sats_used = (uint8_t)atoi(tok[7]);

        if (tok[8][0] != '\0') {
            __nav_data.hdop = strtof(tok[8], NULL);
            __nav_data.hdop_q8_8 = (uint16_t)lroundf(__nav_data.hdop * 256.0f);
        }
        if (tok[9][0] != '\0') {
            __nav_data.altitude = strtof(tok[9], NULL);
            __nav_data.alt_cm = (int32_t)lroundf(__nav_data.altitude * 100.0f);
        }
        if (count >= 12 && tok[11][0] != '\0') {
            __nav_data.geoid_sep = strtof(tok[11], NULL);
        }
    } else {
        __nav_data.fix_valid = false;
        __nav_data.sats_used = 0;
    }
}

/*
 * Parse $GNRMC / $GPRMC sentence
 */
static void parse_rmc(char **tok, int count){
    if (count < 10) return;
    bool t_ok = parse_time_token(tok[1], &__nav_data.utc, &__nav_data.raw_time);
    bool d_ok = parse_date_token(tok[9], &__nav_data.utc, &__nav_data.raw_date);
    if (t_ok && d_ok) {
        __nav_data.unix_epoch = utc_to_epoch(&__nav_data.utc);
    }

    if (tok[2][0] == 'A') {
        __nav_data.fix_valid = true;
        if (tok[3][0] != '\0' && tok[5][0] != '\0') {
            parse_coordinate(tok[3], tok[4][0], &__nav_data.latitude, &__nav_data.lat_scaled);
            parse_coordinate(tok[5], tok[6][0], &__nav_data.longitude, &__nav_data.lon_scaled);
        }
        if (tok[7][0] != '\0') {
            __nav_data.speed_knots = strtof(tok[7], NULL);
            __nav_data.speed_mps = __nav_data.speed_knots * 0.514444f;
        }
        if (tok[8][0] != '\0') {
            __nav_data.course_deg = strtof(tok[8], NULL);
        }
    } else {
        __nav_data.fix_valid = false;
    }
}

/*
 * Parse $GNGSA / $GPGSA sentence
 */
static void parse_gsa(char **tok, int count){
    if (count < 18) return;
    int dim = atoi(tok[2]);
    if (dim >= 1 && dim <= 3) {
        __nav_data.fix_dimension = (uint8_t)dim;
    }
    if (tok[15][0] != '\0') __nav_data.pdop = strtof(tok[15], NULL);
    if (tok[16][0] != '\0') {
        __nav_data.hdop = strtof(tok[16], NULL);
        __nav_data.hdop_q8_8 = (uint16_t)lroundf(__nav_data.hdop * 256.0f);
    }
    if (tok[17][0] != '\0') __nav_data.vdop = strtof(tok[17], NULL);
}

/*
 * Parse $GPGSV / $BDGSV / $GLGSV sentences
 */
static void parse_gsv(const char *talker, char **tok, int count){
    if (count < 4) return;
    int total_msgs = atoi(tok[1]);
    int msg_num = atoi(tok[2]);
    uint8_t in_view = (uint8_t)atoi(tok[3]);

    if (strncmp(talker, "$GP", 3) == 0) {
        __nav_data.sats_view_gps = in_view;
        if (msg_num == 1) __cycle_max_snr = 0;
    } else if (strncmp(talker, "$BD", 3) == 0 || strncmp(talker, "$GB", 3) == 0) {
        __nav_data.sats_view_bds = in_view;
    } else if (strncmp(talker, "$GL", 3) == 0) {
        __nav_data.sats_view_glo = in_view;
    }

    for (int i = 7; i < count; i += 4) {
        if (tok[i][0] != '\0') {
            uint8_t snr = (uint8_t)atoi(tok[i]);
            if (snr > __cycle_max_snr) __cycle_max_snr = snr;
            if (snr > __nav_data.max_snr) __nav_data.max_snr = snr;
        }
    }
    if (msg_num == total_msgs && __cycle_max_snr > 0) {
        __nav_data.max_snr = __cycle_max_snr;
    }
}

/*
 * Parse $GNZDA / $GPZDA sentence
 */
static void parse_zda(char **tok, int count){
    if (count < 5) return;
    bool t_ok = parse_time_token(tok[1], &__nav_data.utc, &__nav_data.raw_time);
    uint8_t d = (uint8_t)atoi(tok[2]);
    uint8_t m = (uint8_t)atoi(tok[3]);
    uint16_t y = (uint16_t)atoi(tok[4]);

    if (t_ok && d >= 1 && d <= 31 && m >= 1 && m <= 12 && y >= 2020) {
        __nav_data.utc.day = d;
        __nav_data.utc.month = m;
        __nav_data.utc.year = y;
        __nav_data.raw_date = (uint32_t)d * 10000U + (uint32_t)m * 100U + (uint32_t)(y % 100U);
        __nav_data.unix_epoch = utc_to_epoch(&__nav_data.utc);
    }
}

/*
 * Parse $GPTXT sentence for active antenna supervisor status
 */
static void parse_txt(char **tok, int count){
    if (count < 5) return;
    if (strstr(tok[4], "ANTENNA OK")) {
        __nav_data.antenna_status = ATGM_ANT_OK;
    } else if (strstr(tok[4], "ANTENNA OPEN")) {
        __nav_data.antenna_status = ATGM_ANT_OPEN;
    } else if (strstr(tok[4], "ANTENNA SHORT")) {
        __nav_data.antenna_status = ATGM_ANT_SHORT;
    }
}

/*
 * Validate and dispatch complete NMEA sentence
 */
static void atgm_parse_line(char *line){
    if (!atgm_validate_sentence(line)) {
        if (__data_mutex) {
            xSemaphoreTake(__data_mutex, portMAX_DELAY);
            __nav_data.crc_errors++;
            xSemaphoreGive(__data_mutex);
        }
        return;
    }

    char talker[4] = {line[0], line[1], line[2], '\0'};
    char *tokens[ATGM_MAX_TOKENS];
    int count = tokenize_nmea(line, tokens, ATGM_MAX_TOKENS);
    if (count < 1 || strlen(tokens[0]) < 6) return;

    const char *type = tokens[0] + 3;

    xSemaphoreTake(__data_mutex, portMAX_DELAY);
    __nav_data.last_update_us = esp_timer_get_time();
    __nav_data.rx_sentences++;

    if (strcmp(type, "GGA") == 0)      parse_gga(tokens, count);
    else if (strcmp(type, "RMC") == 0) parse_rmc(tokens, count);
    else if (strcmp(type, "GSA") == 0) parse_gsa(tokens, count);
    else if (strcmp(type, "GSV") == 0) parse_gsv(talker, tokens, count);
    else if (strcmp(type, "ZDA") == 0) parse_zda(tokens, count);
    else if (strcmp(type, "TXT") == 0) parse_txt(tokens, count);

    xSemaphoreGive(__data_mutex);
}

/*
 * Timer callback to turn off external PPS indicator LED
 */
static void pps_led_off_callback(void *arg){
    (void)arg;
    if (__pps_led_gpio >= 0) {
        gpio_set_level(__pps_led_gpio, 0);
    }
}

/*
 * Hardware GPIO ISR for PPS rising edge capture
 */
static void IRAM_ATTR atgm_pps_isr(void *arg){
    (void)arg;
    int64_t now_us = esp_timer_get_time();

    if (__pps_led_gpio >= 0) {
        gpio_set_level(__pps_led_gpio, 1);
        if (__led_timer) {
            esp_timer_stop(__led_timer);
            esp_timer_start_once(__led_timer, ATGM_PPS_LED_BLINK_US);
        }
    }

    portENTER_CRITICAL_ISR(&__pps_spinlock);
    if (__pps_state.last_pps_us > 0) {
        int64_t delta = now_us - __pps_state.last_pps_us;
        if (delta > 950000LL && delta < 1050000LL) {
            __pps_state.period_us = (int32_t)delta;
            __pps_state.jitter_us = (int32_t)(delta - 1000000LL);
            float ppm = (float)__pps_state.jitter_us;
            __pps_state.drift_ppm = (__pps_state.pulse_count < 4)
                                  ? ppm
                                  : (__pps_state.drift_ppm * 0.8f + ppm * 0.2f);
            __pps_state.locked = true;
        }
    }
    __pps_state.last_pps_us = now_us;
    __pps_state.pulse_count++;
    portEXIT_CRITICAL_ISR(&__pps_spinlock);

    BaseType_t high_task_woken = pdFALSE;
    if (__pps_sem) {
        xSemaphoreGiveFromISR(__pps_sem, &high_task_woken);
    }
    if (high_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/*
 * Background FreeRTOS task for event-driven UART reception
 */
static void atgm_rx_task(void *pvParameters){
    (void)pvParameters;
    uart_event_t event;
    uint8_t buf[128];
    char line[ATGM_LINE_MAX_LEN];
    size_t pos = 0;

    while (1) {
        if (xQueueReceive(__uart_queue, &event, portMAX_DELAY)) {
            if (event.type == UART_DATA) {
                int rem = (int)event.size;
                while (rem > 0) {
                    int chunk = (rem > (int)sizeof(buf)) ? (int)sizeof(buf) : rem;
                    int r = uart_read_bytes(ATGM_UART_PORT, buf, chunk, pdMS_TO_TICKS(10));
                    if (r <= 0) break;
                    rem -= r;

                    for (int i = 0; i < r; i++) {
                        char ch = (char)buf[i];
                        if (ch == '$') {
                            line[0] = '$';
                            pos = 1;
                        } else if (ch == '\r' || ch == '\n') {
                            if (pos >= 6) {
                                line[pos] = '\0';
                                atgm_parse_line(line);
                            }
                            pos = 0;
                        } else if (pos > 0) {
                            if (pos < (sizeof(line) - 1)) {
                                line[pos++] = ch;
                            } else {
                                pos = 0;
                            }
                        }
                    }
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                uart_flush_input(ATGM_UART_PORT);
                xQueueReset(__uart_queue);
                pos = 0;
            }
        }
    }
}

/*
 * Write command to UART
 */
void atgm_send_cmd(const char *cmd){
    if (!cmd || !__initialized) return;
    uart_write_bytes(ATGM_UART_PORT, cmd, strlen(cmd));
}

/*
 * Set navigation constellation mode
 */
void atgm_set_mode(int nav_mode){
    switch (nav_mode) {
        case ATGM_MODE_GPS:         atgm_send_cmd(PCAS_CMD_MODE_GPS); break;
        case ATGM_MODE_BDS:         atgm_send_cmd(PCAS_CMD_MODE_BDS); break;
        case ATGM_MODE_GPS_BDS:     atgm_send_cmd(PCAS_CMD_MODE_GPS_BDS); break;
        case ATGM_MODE_GLONASS:     atgm_send_cmd(PCAS_CMD_MODE_GLONASS); break;
        case ATGM_MODE_GPS_GLONASS: atgm_send_cmd(PCAS_CMD_MODE_GPS_GLONASS); break;
        case ATGM_MODE_BDS_GLONASS: atgm_send_cmd(PCAS_CMD_MODE_BDS_GLONASS); break;
        default:                    atgm_send_cmd(PCAS_CMD_MODE_ALL); break;
    }
}

/*
 * Set position fix update rate (1, 2, 5, 10 Hz)
 */
void atgm_set_update_rate(int rate_hz){
    switch (rate_hz) {
        case 10: atgm_send_cmd(PCAS_CMD_RATE_10HZ); break;
        case 5:  atgm_send_cmd(PCAS_CMD_RATE_5HZ); break;
        case 2:  atgm_send_cmd(PCAS_CMD_RATE_2HZ); break;
        default: atgm_send_cmd(PCAS_CMD_RATE_1HZ); break;
    }
}

/*
 * Configure module and host UART baudrate
 */
void atgm_set_baudrate(int baud){
    int code = 1;
    switch (baud) {
        case 4800:   code = 0; break;
        case 9600:   code = 1; break;
        case 19200:  code = 2; break;
        case 38400:  code = 3; break;
        case 57600:  code = 4; break;
        case 115200: code = 5; break;
        default:     code = 5; baud = 115200; break;
    }

    char body[24];
    snprintf(body, sizeof(body), "PCAS01,%d", code);
    uint8_t crc = atgm_calc_checksum(body);

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "$%s*%02X\r\n", body, crc);
    atgm_send_cmd(cmd);

    uart_wait_tx_done(ATGM_UART_PORT, pdMS_TO_TICKS(100));
    uart_set_baudrate(ATGM_UART_PORT, baud);
    __baud_rate = baud;
}

/*
 * Configure NMEA sentence output intervals ($PCAS03)
 */
void atgm_set_nmea_filter(uint8_t gga, uint8_t gll, uint8_t gsa, uint8_t gsv,
                          uint8_t rmc, uint8_t vtg, uint8_t zda, uint8_t txt){
    char body[64];
    snprintf(body, sizeof(body), "PCAS03,%u,%u,%u,%u,%u,%u,%u,%u,0,0,,,0,0",
             gga, gll, gsa, gsv, rmc, vtg, zda, txt);
    uint8_t crc = atgm_calc_checksum(body);

    char cmd[80];
    snprintf(cmd, sizeof(cmd), "$%s*%02X\r\n", body, crc);
    atgm_send_cmd(cmd);
}

/*
 * Save active configuration to non-volatile memory
 */
void atgm_save_config(void){
    atgm_send_cmd(PCAS_CMD_SAVE_NVM);
}

/*
 * Perform software restart of GNSS engine
 */
void atgm_reset(int reset_type){
    if (reset_type == RESET_COLD) {
        atgm_send_cmd(PCAS_CMD_COLD_START);
    } else if (reset_type == RESET_WARM) {
        atgm_send_cmd(PCAS_CMD_WARM_START);
    } else {
        atgm_send_cmd(PCAS_CMD_HOT_START);
    }
}

/*
 * Enter low-power standby mode
 */
void atgm_sleep(void){
#if (ATGM_ONOFF_NUM >= 0)
    gpio_set_level(ATGM_ONOFF_NUM, 0);
#else
    if (__initialized) uart_flush_input(ATGM_UART_PORT);
#endif
}

/*
 * Wake up module from standby mode
 */
void atgm_wakeup(void){
#if (ATGM_ONOFF_NUM >= 0)
    gpio_set_level(ATGM_ONOFF_NUM, 1);
#else
    atgm_send_cmd("\r\n");
#endif
}

/*
 * Initialization UART, PPS interrupt, and ATGM336H chip
 */
int atgm_init(atgm_config_t *config){
    if (__initialized) return 1;

#if (ATGM_RESET_NUM >= 0)
    gpio_config_t rst_conf = {
        .pin_bit_mask = (1ULL << ATGM_RESET_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&rst_conf);
    gpio_set_level(ATGM_RESET_NUM, 0);
    esp_rom_delay_us(10000);
    gpio_set_level(ATGM_RESET_NUM, 1);
    esp_rom_delay_us(50000);
#endif

#if (ATGM_ONOFF_NUM >= 0)
    gpio_config_t pwr_conf = {
        .pin_bit_mask = (1ULL << ATGM_ONOFF_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&pwr_conf);
    gpio_set_level(ATGM_ONOFF_NUM, 1);
#endif

    if (!__data_mutex) {
        __data_mutex = xSemaphoreCreateMutex();
    }

    __baud_rate = (config && config->baud_rate > 0) ? config->baud_rate : 115200;

    uart_config_t uart_conf = {
        .baud_rate = __baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(ATGM_UART_PORT, ATGM_RX_BUF_SIZE, 0, ATGM_UART_QUEUE_SIZE, &__uart_queue, 0);
    uart_param_config(ATGM_UART_PORT, &uart_conf);
    uart_set_pin(ATGM_UART_PORT, ATGM_TX_NUM, ATGM_RX_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    __initialized = true;

    /*
     * Configure PPS input interrupt and optional external indicator LED
     */
    __pps_gpio = (config && config->pps_gpio != 0) ? config->pps_gpio : ATGM_PPS_NUM;
    __pps_led_gpio = (config && config->pps_led_gpio > 0) ? config->pps_led_gpio : -1;

    if (__pps_led_gpio >= 0) {
        gpio_config_t led_conf = {
            .pin_bit_mask = (1ULL << __pps_led_gpio),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&led_conf);
        gpio_set_level(__pps_led_gpio, 0);

        if (!__led_timer) {
            const esp_timer_create_args_t t_args = {
                .callback = &pps_led_off_callback,
                .name = "pps_led"
            };
            esp_timer_create(&t_args, &__led_timer);
        }
    }

    if (__pps_gpio >= 0) {
        if (!__pps_sem) __pps_sem = xSemaphoreCreateBinary();
        gpio_config_t pps_conf = {
            .pin_bit_mask = (1ULL << __pps_gpio),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_POSEDGE
        };
        gpio_config(&pps_conf);

        esp_err_t err = gpio_install_isr_service(0);
        if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
            gpio_isr_handler_remove(__pps_gpio);
            gpio_isr_handler_add(__pps_gpio, atgm_pps_isr, NULL);
        }
    }

    /*
     * Send initial configuration to module
     */
    if (config != NULL) {
        if (config->baud_rate > 0 && config->baud_rate != 9600) {
            uart_set_baudrate(ATGM_UART_PORT, 9600);
            atgm_set_baudrate(config->baud_rate);
            vTaskDelay(pdMS_TO_TICKS(40));
        }
        if (config->nav_mode > 0) {
            atgm_set_mode(config->nav_mode);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if (config->update_rate_hz > 0) {
            atgm_set_update_rate(config->update_rate_hz);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    uart_flush_input(ATGM_UART_PORT);
    xQueueReset(__uart_queue);

    if (!__rx_task_handle) {
        xTaskCreate(atgm_rx_task, "atgm_rx", 4096, NULL, 6, &__rx_task_handle);
    }

    return 1;
}

/*
 * Copy latest parsed navigation dataset in a thread-safe manner
 */
bool atgm_get_data(atgm_data_t *out_data){
    if (!out_data || !__data_mutex) return false;
    xSemaphoreTake(__data_mutex, portMAX_DELAY);
    memcpy(out_data, &__nav_data, sizeof(atgm_data_t));
    bool valid = __nav_data.fix_valid;
    xSemaphoreGive(__data_mutex);
    return valid;
}

/*
 * Retrieve hardware PPS timing metrics
 */
bool atgm_get_pps(atgm_pps_t *out_pps){
    if (!out_pps) return false;
    int64_t now = esp_timer_get_time();

    portENTER_CRITICAL(&__pps_spinlock);
    if ((now - __pps_state.last_pps_us) > ATGM_PPS_TIMEOUT_US) {
        __pps_state.locked = false;
    }
    memcpy(out_pps, &__pps_state, sizeof(atgm_pps_t));
    bool locked = __pps_state.locked;
    portEXIT_CRITICAL(&__pps_spinlock);
    return locked;
}

/*
 * Block calling task until next PPS rising edge arrives
 */
bool atgm_wait_pps(uint32_t timeout_ms, int64_t *out_edge_us){
    if (!__pps_sem) return false;
    xSemaphoreTake(__pps_sem, 0);
    if (xSemaphoreTake(__pps_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        if (out_edge_us) {
            portENTER_CRITICAL(&__pps_spinlock);
            *out_edge_us = __pps_state.last_pps_us;
            portEXIT_CRITICAL(&__pps_spinlock);
        }
        return true;
    }
    return false;
}

/*
 * Compute drift-compensated global Unix time in microseconds for TDMA slotting
 */
bool atgm_get_synced_unix_us(uint64_t *out_unix_us){
    if (!out_unix_us || !__data_mutex) return false;

    xSemaphoreTake(__data_mutex, portMAX_DELAY);
    uint64_t base_sec = __nav_data.unix_epoch;
    int64_t nmea_us = __nav_data.last_update_us;
    xSemaphoreGive(__data_mutex);

    if (base_sec == 0) return false;

    int64_t now_us = esp_timer_get_time();
    int64_t pps_us = 0;
    float drift_ppm = 0.0f;
    bool pps_ok = false;

    portENTER_CRITICAL(&__pps_spinlock);
    pps_us = __pps_state.last_pps_us;
    drift_ppm = __pps_state.drift_ppm;
    pps_ok = __pps_state.locked && ((now_us - pps_us) <= ATGM_PPS_TIMEOUT_US);
    portEXIT_CRITICAL(&__pps_spinlock);

    if (pps_ok) {
        if (nmea_us < pps_us && (pps_us - nmea_us) > 150000LL) {
            base_sec += 1ULL;
        }
        int64_t elapsed_us = now_us - pps_us;
        int64_t corr_us = (int64_t)((float)elapsed_us * (drift_ppm / 1000000.0f));
        *out_unix_us = (base_sec * 1000000ULL) + (uint64_t)(elapsed_us - corr_us);
    } else {
        int64_t elapsed_us = now_us - nmea_us;
        if (elapsed_us < 0) elapsed_us = 0;
        *out_unix_us = (base_sec * 1000000ULL) + (uint64_t)elapsed_us;
    }
    return true;
}

/*
 * Raw non-blocking packet read (legacy compatibility)
 */
int atgm_receive_packet(uint8_t *buf, int size){
    size_t buffered_len = 0;
    uart_get_buffered_data_len(ATGM_UART_PORT, &buffered_len);
    if (buffered_len == 0) return 0;
    if (buffered_len > (size_t)size) buffered_len = size;
    return uart_read_bytes(ATGM_UART_PORT, buf, buffered_len, 0);
}

/*
 * Raw single NMEA line read (legacy compatibility)
 */
int atgm_receive_line(char *buf, int max_size){
    size_t buffered_len = 0;
    uart_get_buffered_data_len(ATGM_UART_PORT, &buffered_len);
    if (buffered_len == 0 || max_size <= 1) return 0;

    int idx = 0;
    uint8_t ch = 0;
    bool started = false;

    while (idx < (max_size - 1)) {
        int r = uart_read_bytes(ATGM_UART_PORT, &ch, 1, pdMS_TO_TICKS(5));
        if (r <= 0) break;
        if (!started) {
            if (ch == '$') {
                started = true;
                buf[idx++] = (char)ch;
            }
            continue;
        }
        buf[idx++] = (char)ch;
        if (ch == '\n') break;
    }
    buf[idx] = '\0';
    return idx;
}

/**
 * Shutdown hardware and free OS resources.
 */
void atgm_close(void){
    if (!__initialized) return;
    if (__rx_task_handle) {
        vTaskDelete(__rx_task_handle);
        __rx_task_handle = NULL;
    }
    if (__pps_gpio >= 0) {
        gpio_isr_handler_remove(__pps_gpio);
    }
    if (__led_timer) {
        esp_timer_stop(__led_timer);
        esp_timer_delete(__led_timer);
        __led_timer = NULL;
    }
    atgm_sleep();
    uart_driver_delete(ATGM_UART_PORT);
    __uart_queue = NULL;
    __initialized = false;
}