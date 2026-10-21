# Контекст разработки

## Дата начала: 2026-10-21

## Описание проекта
Проект на ESP-IDF с поддержкой LoRa TDMA, управлением питанием и датчиками.

## Структура кодовой базы

### Драйверы (src/drivers/)
- `ili9341.c/h` - драйвер дисплея ILI9341 (read-only)
- `sx127x.c/h` - драйвер LoRa модуля SX127x (read-only)
- `bq27441_g1.c/h` - драйвер батарейного контроллера BQ27441

### Подсистемы (src/subsystems/)
- `lora_tdma/lora_packet.h` - структуры пакетов для LoRa TDMA (read-only)

### Ядро
- `src/list.c`, `src/list.h` - управление списком задач FreeRTOS
- `src/main.c` - точка входа приложения (read-only)
- `src/config.h` - конфигурация пинов и параметров

## Файлы в чате (доступны для редактирования)
- `src/drivers/bq27441_g1.c` - драйвер BQ27441 (батарейный контроллер)
- `src/drivers/bq27441_g1.h` - заголовочный файл для BQ27441 (пустой)
- `src/list.c` - управление списком задач FreeRTOS
- `src/config.h` - конфигурация пинов и параметров

## Подсистемы (subsys_id_t)
- SUBSYS_CORE = 0 - ядро системы
- SUBSYS_POWER - управление питанием
- SUBSYS_LORA - радиостек
- SUBSYS_SENSORS - датчики

## Конфигурация пинов (config.h)
- **Display (ILI9341)**: MOSI=11, SCLK=12, RESET=5, DC=4, CS=18, freq=60MHz
- **LoRa (SX127x)**: MOSI=5, MISO=4, SCLK=6, RESET=1, CS=7, freq=4MHz
- **Battery (BQ27441)**: SCL=22, SDA=21, I2C freq=100kHz

## Регистры BQ27441 (определены в bq27441_g1.c)
- REG_CONTROL (0x00)
- REG_TEMPERATURE (0x02)
- REG_VOLTAGE (0x04)
- REG_FLAGS (0x06)
- REG_STATE_OF_CHARGE (0x1C)
- REG_REMAINING_CAPACITY (0x0C)
- И другие...

## Заметки
- Используется FreeRTOS
- I2C для связи с BQ27441 (I2C_NUM_0, адрес: 0x55)
- В `bq27441_g1.c` есть незавершенная функция `bq_init()` - нужно реализовать работу со снапшотами
- В `list.c` используется spinlock для thread-safety при удалении задач
- `bq27441_g1.h` пустой - нужно добавить объявления функций
- Все файлы из summaries считаются read-only до явного добавления в чат
- Максимум задач в списке: TASKS_MAX = 15

## История коммитов
- 16b4033 - (no commit message provided)
