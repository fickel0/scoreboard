#include "comm_wired.h"
#include <driver/uart.h>
#include "display.h"

extern void sleep();
extern void awake();
extern bool is_sleeping;

void tx_task(void *arg)
{
	for(;;)
	{
		const int len = uart_write_bytes(UART_NUM_1, scoreboard.tx_buffer, 10);
		vTaskDelay(pdMS_TO_TICKS(UART_TIMEOUT_TIME_MS));
	}
	vTaskDelete(NULL);
}

void run_command(const uint8_t * const data, int len)
{
	switch(data[0])
	{
	case SET_VALUES:
		if(len < 7)
		{
			return;
		}
		scoreboard.set_fouls(TEAM_GUEST, data[1]);
		scoreboard.set_fouls(TEAM_HOME, data[2]);
		scoreboard.set_period(data[3]);
		scoreboard.set_score(TEAM_GUEST, data[4]);
		scoreboard.set_score(TEAM_HOME, data[5]);
		scoreboard.set_timer_pause(!!(data[6] & 1));
		scoreboard.set_buzzer_is_buzzing(!!(data[6] & 2));
		scoreboard.set_regressive_counting(!!(data[6] & 4));
		break;
	case SET_TIME:
		if(len < 3)
		{
			return;
		}
		scoreboard.set_time((uint16_t)((data[1] << 8) + data[2]));
		break;
	case START_SLEEP:
		if(!is_sleeping)
		{
			sleep();
		} 
		else 
		{
			awake();
		}
		break;
	default:
		break;
	}
}

void rx_task(void *arg)
{
	uint8_t *data = (uint8_t *)malloc(UART_BUF_SIZE);
	for(;;)
	{
		const int len = uart_read_bytes(UART_NUM_1, data, UART_BUF_SIZE,
				pdMS_TO_TICKS(UART_TIMEOUT_TIME_MS));
		if(len > 0)
		{
			uint8_t *newdata = NULL;
			for(int i = 0; i < len; i++) {
				if(data[i] == 0xff && len - i >= 0) {
					newdata = &(data[i+1]);
					break;
				}
			}
			
			if(is_sleeping && newdata[0] != START_SLEEP)
			{
				awake();
			}

			if(newdata != NULL) {
				run_command(newdata, len);
			}

			fflush(stdout);
		}
	}
	free(data);
	vTaskDelete(NULL);
}
