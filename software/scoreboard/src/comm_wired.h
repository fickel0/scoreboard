#pragma once
#include <stdint.h>
#include <esp_sleep.h>

#define PIN_RXD GPIO_NUM_16
#define PIN_TXD GPIO_NUM_17

#define UART_BUF_SIZE UART_FIFO_LEN * 2
#define UART_TIMEOUT_TIME_MS 100

enum COMMAND {
	SET_VALUES,
	SET_TIME,
	START_SLEEP
};

void rx_task(void *arg);
void tx_task(void *arg);
void run_command(const uint8_t * const data, int len);
