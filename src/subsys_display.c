#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "kernel.h"
#include "subsys_display.h"

static const char *TAG_DISP = "DISPLAY_SUBSYS";

// Пины подключения (подбираются под разводку платы)
#define LCD_HOST       SPI2_HOST
#define PIN_NUM_MISO   -1
#define PIN_NUM_MOSI   11
#define PIN_NUM_CLK    12
#define PIN_NUM_CS     10
#define PIN_NUM_DC     9
#define PIN_NUM_RST    14

static spi_device_handle_t spi_dev;
static QueueHandle_t display_queue = NULL;

// Тип сообщения для очереди экрана
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t color;
    char text[16];
} display_cmd_t;

// Низкоуровневая передача команды/данных в экран
static void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd) {
    gpio_set_level(PIN_NUM_DC, 0); // Режим команд
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_polling_transmit(spi, &t);
}

static void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len) {
    if (len == 0) return;
    gpio_set_level(PIN_NUM_DC, 1); // Режим данных
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_polling_transmit(spi, &t);
}

// Инициализация SPI шины
static void spi_display_init(void) {
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);

    // Аппаратный сброс экрана
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 320 * 240 * 2 + 8
    };
    spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000, // 40 МГц
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7,
    };
    spi_bus_add_device(LCD_HOST, &devcfg, &spi_dev);

    // Здесь посылаются стартовые команды инициализации чипа дисплея
    lcd_cmd(spi_dev, 0x01); // Software Reset
    vTaskDelay(pdMS_TO_TICKS(150));
    lcd_cmd(spi_dev, 0x11); // Sleep Out
    vTaskDelay(pdMS_TO_TICKS(255));
    lcd_cmd(spi_dev, 0x29); // Display ON
}

// Задача обслуживания дисплея
void display_task(void *pvParameters) {
    (void)pvParameters;
    spi_display_init();

    display_cmd_t cmd;

    for (;;) {
        // Отстукиваем ядру, что таска не зависла
        sys_heartbeat(SUBSYS_SENSORS);

        // Ждем команду на отрисовку с таймаутом (например, 500 мс)
        if (xQueueReceive(display_queue, &cmd, pdMS_TO_TICKS(500)) == pdTRUE) {
            ESP_LOGI(TAG_DISP, "Render: %s at (%d, %d)", cmd.text, cmd.x, cmd.y);
            // Тут идет транзакция по SPI в буфер дисплея
        }
    }
}

// Точка входа подсистемы дисплея
void display_subsys_init(void) {
    // 1. Создаем очередь команд через ядро
    create_queue(10, sizeof(display_cmd_t), &display_queue);

    // 2. Спавним задачу в ядре с привязкой к подсистеме
    TaskHandle_t disp_handle = NULL;
    create_task(SUBSYS_SENSORS, "disp_task", display_task, NULL, 4096, 3, &disp_handle);
}