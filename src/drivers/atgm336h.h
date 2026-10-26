#ifndef ATGM336H_H
#define ATGM336H_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Navigation constellation modes
 */
#define ATGM_MODE_GPS                   1
#define ATGM_MODE_BDS                   2
#define ATGM_MODE_GPS_BDS               3
#define ATGM_MODE_GLONASS               4
#define ATGM_MODE_GPS_GLONASS           5
#define ATGM_MODE_BDS_GLONASS           6
#define ATGM_MODE_ALL                   7

/*
 * Reset types
 */
#define RESET_HOT                       0
#define RESET_WARM                      1
#define RESET_COLD                      2

/*
 * Antenna status codes
 */
#define ATGM_ANT_UNKNOWN                0
#define ATGM_ANT_OK                     1
#define ATGM_ANT_OPEN                   2
#define ATGM_ANT_SHORT                  3

/*
 * Decoded UTC date and time structure
 */
typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint16_t millisecond;
} atgm_utc_t;

/*
 * Hardware PPS timing and clock drift metrics
 */
typedef struct {
    int64_t  last_pps_us;       // esp_timer_get_time() at latest rising edge
    int32_t  period_us;         // Measured interval between last two pulses
    int32_t  jitter_us;         // Instantaneous deviation from 1,000,000 us
    float    drift_ppm;         // Filtered local crystal frequency drift (PPM)
    uint32_t pulse_count;       // Total captured PPS edges
    bool     locked;            // True if pulse arrived within last 1.2s
} atgm_pps_t;

/*
 * Complete navigation and telemetry dataset
 */
typedef struct {
    bool       fix_valid;       // Valid position fix flag
    uint8_t    fix_quality;     // 0: Invalid, 1: GNSS, 2: DGPS
    uint8_t    fix_dimension;   // 1: None, 2: 2D, 3: 3D

    double     latitude;        // Decimal degrees (+N / -S)
    double     longitude;       // Decimal degrees (+E / -W)
    float      altitude;        // MSL altitude in meters
    float      geoid_sep;       // Geoid separation in meters

    int32_t    lat_scaled;      // Latitude * 1e6 (for LoRa payload)
    int32_t    lon_scaled;      // Longitude * 1e6 (for LoRa payload)
    int32_t    alt_cm;          // Altitude in centimeters

    float      speed_knots;     // Ground speed in knots
    float      speed_mps;       // Ground speed in m/s
    float      course_deg;      // True track angle in degrees

    float      pdop;            // Position DOP
    float      hdop;            // Horizontal DOP
    float      vdop;            // Vertical DOP
    uint16_t   hdop_q8_8;       // HDOP in fixed-point Q8.8 format

    uint8_t    sats_used;       // Satellites used in solution
    uint8_t    sats_view_gps;   // Visible GPS/SBAS satellites
    uint8_t    sats_view_bds;   // Visible BeiDou satellites
    uint8_t    sats_view_glo;   // Visible GLONASS satellites
    uint8_t    max_snr;         // Peak carrier-to-noise ratio (dB-Hz)
    uint8_t    antenna_status;  // ATGM_ANT_* status code

    atgm_utc_t utc;             // Broken-down UTC timestamp
    uint32_t   raw_time;        // Packed HHMMSS
    uint32_t   raw_date;        // Packed DDMMYY
    uint64_t   unix_epoch;      // Seconds since 1970-01-01 00:00:00 UTC
    int64_t    last_update_us;  // Local esp_timer timestamp of last valid packet

    uint32_t   rx_sentences;    // Total valid NMEA sentences parsed
    uint32_t   crc_errors;      // Total rejected sentences due to XOR mismatch
} atgm_data_t;

/*
 * Driver configuration structure
 */
typedef struct {
    int baud_rate;
    int nav_mode;               // 1: GPS, 2: BDS, 3: GPS+BDS, 4: GLONASS, 5: GPS+GLONASS, 7: ALL
    int update_rate_hz;         // 1, 2, 5, 10 Hz
    int pps_gpio;               // PPS input GPIO (-1 to disable, 0 to use config.h default)
    int pps_led_gpio;           // Optional external PPS indicator LED GPIO (-1 to disable)
} atgm_config_t;

/*
 * Core driver API
 */
int  atgm_init(atgm_config_t *config);
void atgm_send_cmd(const char *cmd);
int  atgm_receive_packet(uint8_t *buf, int size);
int  atgm_receive_line(char *buf, int max_size);
void atgm_set_mode(int nav_mode);
void atgm_set_update_rate(int rate_hz);
void atgm_set_baudrate(int baud);
void atgm_set_nmea_filter(uint8_t gga, uint8_t gll, uint8_t gsa, uint8_t gsv,
                          uint8_t rmc, uint8_t vtg, uint8_t zda, uint8_t txt);
void atgm_save_config(void);
void atgm_reset(int reset_type);
void atgm_sleep(void);
void atgm_wakeup(void);
void atgm_close(void);

/*
 * Navigation data and TDMA synchronization API
 */
bool atgm_get_data(atgm_data_t *out_data);
bool atgm_get_pps(atgm_pps_t *out_pps);
bool atgm_wait_pps(uint32_t timeout_ms, int64_t *out_edge_us);
bool atgm_get_synced_unix_us(uint64_t *out_unix_us);

#ifdef __cplusplus
}
#endif

#endif // ATGM336H_H