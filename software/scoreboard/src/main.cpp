#include <Arduino.h>
#include <driver/uart.h>
#include "display.h"
#include "comm_wired.h"

static TaskHandle_t handle_tx = NULL; 
static TaskHandle_t handle_display = NULL; 
static TaskHandle_t handle_buzzer = NULL; 
bool is_sleeping = false;

uint64_t las_time = 0;
void display_task(void *_)
{
    uint64_t counter = 0;
    uint64_t cur_time = esp_timer_get_time();
    las_time = cur_time;
	for(;;)
    {
	    scoreboard.update_display();
	    vTaskDelay(pdMS_TO_TICKS(50));

	    cur_time = esp_timer_get_time();
		if(scoreboard.timer_running)
		{
			counter += cur_time - las_time;
			if(counter >= 1000000)
			{
				counter %= 1000000;
				scoreboard.set_time(scoreboard.time_s + (scoreboard.regressive_counting ? -1 : 1));
			}
		}
		las_time = cur_time;
    }
    vTaskDelete(NULL);
}

void create_tasks_awake()
{
	xTaskCreate(tx_task,
			"uart_tx_task",
			configMINIMAL_STACK_SIZE * 4,
			NULL,
			configMAX_PRIORITIES - 1,
			&handle_tx);
	xTaskCreate(display_task,
			"display_task",
			configMINIMAL_STACK_SIZE,
			NULL,
			configMAX_PRIORITIES - 1,
			&handle_display);
	xTaskCreate(buzzer_task,
			"buzzer_task",
			configMINIMAL_STACK_SIZE,
			NULL,
			configMAX_PRIORITIES - 1,
			&handle_buzzer);
}

void setup()
{
	esp_timer_init();
	gpio_set_direction(PIN_CLOCK, GPIO_MODE_OUTPUT);
	gpio_set_level(PIN_CLOCK, 1);
	gpio_set_direction(PIN_STROBE, GPIO_MODE_OUTPUT);
	gpio_set_level(PIN_STROBE, 1);
	gpio_set_direction(PIN_DATA, GPIO_MODE_OUTPUT);
	gpio_set_level(PIN_DATA, 1);
	gpio_set_direction(GPIO_NUM_27, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_NUM_27, 0);

	const uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_APB,
	};
	uart_driver_install(UART_NUM_1, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
	uart_param_config(UART_NUM_1, &uart_config);
	uart_set_pin(UART_NUM_1, PIN_TXD, PIN_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

	xTaskCreate(rx_task,
		    "uart_rx_task",
		    configMINIMAL_STACK_SIZE * 4,
		    NULL,
		    configMAX_PRIORITIES - 1,
		    NULL);
	create_tasks_awake();
}

extern void pulse(gpio_num_t pin);
void sleep()
{
	scoreboard.set_timer_pause(true);
	vTaskDelay(pdMS_TO_TICKS(UART_TIMEOUT_TIME_MS));
	vTaskDelete(handle_tx);
	vTaskDelete(handle_display);
	vTaskDelete(handle_buzzer);
	for(int i = 0; i < DP_AMOUNT; i++) {
        for(int j = 7; j >= 0; j--) {
            gpio_set_level(PIN_DATA, 1);
	        pulse(PIN_CLOCK);
        }
    }
    pulse(PIN_STROBE);
	is_sleeping = true;
}

void awake()
{
	las_time = esp_timer_get_time();
	create_tasks_awake();
	is_sleeping = false;
}

void loop()
{
}
