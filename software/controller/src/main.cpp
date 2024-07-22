#include <sys/time.h>
#include <hd44780.h>
#include <esp_idf_lib_helpers.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

#include <ESP32-USB-Soft-Host.h>

#define TXD_PIN (GPIO_NUM_33)
#define RXD_PIN (GPIO_NUM_34)
#define UART UART_NUM_1
#define LCD_COLUMNS 40
#define RX_BUF_SIZE 1024

#define KEY_ARROW_LEFT   0x50
#define KEY_ARROW_UP	 0x52
#define KEY_ARROW_DOWN	 0x51
#define KEY_ARROW_RIGHT	 0x4f
#define KEY_NUMPAD_PLUS	 0x57
#define KEY_PLUS	 0x2e
#define KEY_NUMPAD_MINUS 0x56
#define KEY_MINUS	 0x2d
#define KEY_NUMPAD_1	 0x59
#define KEY_1		 0x1e
#define KEY_NUMPAD_2	 0x5a
#define KEY_2		 0x1f
#define KEY_NUMPAD_3	 0x5b
#define KEY_3		 0x20
#define KEY_NUMPAD_4	 0x5c
#define KEY_4		 0x21
#define KEY_NUMPAD_5	 0x5d
#define KEY_5		 0x22
#define KEY_NUMPAD_6	 0x5e
#define KEY_6		 0x23
#define KEY_NUMPAD_7	 0x5f
#define KEY_7		 0x24
#define KEY_NUMPAD_8	 0x60
#define KEY_8		 0x25
#define KEY_NUMPAD_9	 0x61
#define KEY_9		 0x26
#define KEY_NUMPAD_0	 0x62
#define KEY_0		 0x27
#define KEY_NUMPAD_ENTER 0x58
#define KEY_ENTER	 0x28
#define KEY_ESCAPE	 0x29
#define KEY_F1		 0x3a
#define KEY_F2		 0x3b
#define KEY_F3		 0x3c
#define KEY_F4		 0x3d
#define KEY_F5		 0x3e
#define KEY_F6		 0x3f
#define KEY_F7		 0x40
#define KEY_F8		 0x41
#define KEY_F9		 0x42
#define KEY_F10		 0x43
#define KEY_F11		 0x44
#define KEY_F12		 0x45
#define KEY_SPACE	 0x2c
#define KEY_PAGE_UP	 0x4b
#define KEY_PAGE_DOWN	 0x4e
#define KEY_INSERT	 0x49
#define KEY_TAB		 0x2b
#define KEY_DELETE	 0x4c
#define KEY_P		 0x13

typedef int32_t int_caster_type;

enum COMMAND
{
	SET_VALUES,
	SET_TIME,
	START_SLEEP
};;

struct MenuEntry
{
	char text[32];
	void (*fptr)(void *arg);
	void *argument[32];
};

struct AlternateEntry
{
	char text[32];
	void (*fptr)(void *arg);
	void *argument[32];

	int x;
	int y;

	int left;
	int down;
	int up;
	int right;
};

struct StringM
{
	char s1[LCD_COLUMNS + 1];
	char s2[LCD_COLUMNS + 1];

	void clear()
	{
		for(int i = 0; i < LCD_COLUMNS; i++)
		{
			s1[i] = ' ';
			s2[i] = ' ';
		}
		s1[LCD_COLUMNS] = '\0';
		s2[LCD_COLUMNS] = '\0';
	}

	StringM()
	{
		clear();
	}

	void putsxy(int x, int y, char *in)
	{
		char *s;
		if(y == 0)
		{
			s = s1;
		} else if(y == 1)
		{
			s = s2;
		} else
		{
			return;
		}
		if(x < 0)
		{
			return;
		}

		for(int i = x; i < LCD_COLUMNS && in[i - x] != '\0'; i++)
		{
			s[i] = in[i - x];
		}
	}
};

static const uint8_t char_data[] =
{
	0b00000000,
	0b00011100,
	0b00000000,
	0b00000110,
	0b00000001,
	0b00000110,
	0b00000001,
	0b00000110,

	0b00000000,
	0b00011100,
	0b00000000,
	0b00000110,
	0b00000001,
	0b00000010,
	0b00000100,
	0b00000111,

	0b00000000,
	0b00011100,
	0b00000000,
	0b00000010,
	0b00000110,
	0b00000010,
	0b00000010,
	0b00000111,

	0b00001000,
	0b00011100,
	0b00001000,
	0b00000110,
	0b00000001,
	0b00000110,
	0b00000001,
	0b00000110,

	0b00001000,
	0b00011100,
	0b00001000,
	0b00000110,
	0b00000001,
	0b00000010,
	0b00000100,
	0b00000111,

	0b00001000,
	0b00011100,
	0b00001000,
	0b00000010,
	0b00000110,
	0b00000010,
	0b00000010,
	0b00000111,

	0b00011000,
	0b00011100,
	0b00011110,
	0b00011111,
	0b00011110,
	0b00011100,
	0b00011000,
	0b00000000,

	0b00011011,
	0b00011011,
	0b00011011,
	0b00011011,
	0b00011011,
	0b00011011,
	0b00011011,
	0b00000000,
};

// global variables

uint8_t score_a = 0;
uint8_t score_b = 0;
uint8_t topleft = 0;
uint8_t topright = 0;
uint8_t period = 0;
uint16_t timer_seconds = 0;
uint8_t timer_info = 0;

uint64_t last_tap_time = 0;
bool already_moved = 0;

hd44780_t lcd;
bool enter_command = 0;
int cursor_x = 0;
int cursor_y = 0;
int selector_position = 0;
int input_update = 0;
int input_char = 0;
int cursor_counter = 0;

void send_values()
{
	uint8_t tx_data[16] =
	{
		0, 0, 0, 0, 0, 0, 0, 0xff,
		(COMMAND)SET_VALUES, topleft, topright, period,
		score_a, score_b, timer_info, 0
	};

	uart_write_bytes(UART, tx_data, 16);
}

void send_time()
{
	uint8_t tx_data[12] =
	{
		0, 0, 0, 0, 0, 0, 0, 0xff,
		(COMMAND)SET_TIME,
		(uint8_t)((timer_seconds >> 8) & 0xff),
		(uint8_t)(timer_seconds & 0xff),
		0
	};

	uart_write_bytes(UART, tx_data, 12);
}

void send_start_sleep()
{
	uint8_t tx_data[9] =
	{
		0, 0, 0, 0, 0, 0, 0, 0xff,
		(COMMAND)START_SLEEP
	};
	uart_write_bytes(UART, tx_data, 9);
}

void send_toggle_pause(void *arg)
{
	timer_info ^= 1;
	timer_info = timer_info & (~2);
	send_values();
	timer_info ^= 1;
}

void send_speaker(void *arg)
{
	timer_info |= 2;
	send_values();
	timer_info = timer_info & (~2);
}

void send_togprog(void *arg)
{
	if(timer_info & 1)
	{
		timer_info ^= 4;
		send_values();
		timer_info ^= 4;
	} else
	{
		selector_position = 38;
	}
}

int clamp(int a, int lo, int hi)
{
	if(a < lo)
	{
		return lo;
	} else if (a > hi)
	{
		return hi;
	}
	return a;
}

void change_score(void *arg)
{
	int old_score_a = score_a;
	int old_score_b = score_b;
	int addend =  ((int32_t *)arg)[0];
	int team = ((int32_t *)arg)[1];

	if(input_char != '\n' && input_char != -5)
	{
		return;
	}

	if(team == 0)
	{
		score_a = clamp(score_a + addend, 0, 199);
	} else
	{
		score_b = clamp(score_b + addend, 0, 199);
	}

	timer_info = timer_info & (~2);
	send_values();
	score_a = old_score_a;
	score_b = old_score_b;
}

void send_byte(void *arg)
{
	char bytes[1];
	bytes[0] = ((int32_t *)arg)[0];
	uart_write_bytes(UART, bytes, 1);
}

void set_top(void *arg)
{
	int which  = ((int32_t *)arg)[0];
	int addend = ((int32_t *)arg)[1];

	if(input_char != '\n' && input_char != -5)
	{
		return;
	}

	int old_topleft  = topleft;
	int old_period   = period;
	int old_topright = topright;

	switch(which)
	{
	case 0 : topleft  = clamp(topleft + addend, 0, 19);  break;
	case 1 : period   = clamp(period + addend, 0, 9);    break;
	case 2 : topright = clamp(topright + addend, 0, 19); break;
	}

	timer_info = timer_info & (~2);
	send_values();
	topleft = old_topleft;
	period = old_period;
	topright = old_topright;
}

void zero_everything()
{
	topleft = 0;
	int old_topleft = topleft;
	topright = 0;
	int old_topright = topright;
	period = 0;
	int old_period = period;
	score_a = 0;
	int old_score_a = score_a;
	score_b = 0;
	int old_score_b = score_b;
	timer_info = 0;

	send_values();

	topleft  = old_topleft;
	topright = old_topright;
	period   = old_period;
	score_a  = old_score_a;
	score_b  = old_score_b;

	ets_delay_us(123456);

	int old_timer_seconds = timer_seconds;
	timer_seconds = ~0;
	send_time();
	timer_seconds = old_timer_seconds;
}

void inc_time(void *arg) { timer_seconds++; } // UNUSED
void dec_time(void *arg) { timer_seconds--; } // UNUSED

void up_digit(void *arg)
{
	int multiplier = ((int_caster_type *)arg)[0];
	int limiter = ((int_caster_type *)arg)[1];
	int which = ((int_caster_type *)arg)[2];
	int modulo = ((int_caster_type *)arg)[3];
	int num = input_char - '0';

	int      *which_int   = NULL;
	uint8_t  *which_int8  = NULL;
	uint16_t *which_int16 = NULL;
	enum { SIZE_8, SIZE_16, SIZE_INT, SIZE_NONE } which_size;

	if(which == 3 && !(timer_info & 1))
	{
		selector_position = 38;
		return;
	}

	if(!(0 <= num && num <= 9) &&
	   input_char != '+' &&
	   input_char != '-')
	{
		return;
	}

	switch(which)
	{
	case 0 : which_int8  = &topleft;       which_size = SIZE_8;  break;
	case 1 : which_int8  = &period;        which_size = SIZE_8;  break;
	case 2 : which_int8  = &topright;      which_size = SIZE_8;  break;
	case 3 : which_int16 = &timer_seconds; which_size = SIZE_16;
		if(timer_info & 1 == 0)
		{
			// if it is paused, don't allow the
			// user to change value on the timer
			return;
		}
		break;
	case 4 : which_int8  = &score_a;       which_size = SIZE_8;  break;
	case 5 : which_int8  = &score_b;       which_size = SIZE_8;  break;
	default : which_size = SIZE_NONE; break;
	}

	uint8_t old_topleft = topleft;
	uint8_t old_period = period;
	uint8_t old_topright = topright;
	uint16_t old_timer_seconds = timer_seconds;
	uint8_t old_score_a = score_a;
	uint8_t old_score_b = score_b;

	if(modulo == -1)
	{
		modulo = 1000000000;
	}
	if(which_size == SIZE_8)
	{
		if(input_char == '+')
		{
			*which_int8 += 1;
		} else if(input_char == '-')
		{
			*which_int8 -= 1;
		} else
		{
			*which_int8 -= ((*which_int8 % modulo / multiplier) % limiter) * multiplier;
			*which_int8 += num * (multiplier);
		}
	} else if(which_size == SIZE_16)
	{
		if(input_char == '+')
		{
			*which_int16 += 1;
		} else if(input_char == '-')
		{
			*which_int16 -= 1;
		} else
		{
			*which_int16 -= ((*which_int16 % modulo / multiplier) % limiter) * multiplier;
			*which_int16 += num * (multiplier);
		}
	} else if(which_size == SIZE_INT)
	{
		if(input_char == '+')
		{
			*which_int += 1;
		} else if(input_char == '-')
		{
			*which_int -= 1;
		} else
		{
			*which_int -= ((*which_int % modulo / multiplier) % limiter) * multiplier;
			*which_int += num * (multiplier);
		}
	}

	timer_info = timer_info & (~2);
	if(which == 3)
	{
		if(timer_info & 1)
		{
			send_time();
		} else
		{
			selector_position = 38;
		}
	} else
	{
		send_values();
	}

	topleft = old_topleft;
	period = old_period;
	topright = old_topright;
	timer_seconds = old_timer_seconds;
	score_a = old_score_a;
	score_b = old_score_b;

	cursor_counter = 0;

	// go to next up_digit
	if(13 <= selector_position && selector_position <= 15)
	{
		selector_position++;
	}
}


AlternateEntry alt_entries[] =
{
  /*  0 */ { "-", set_top, {(void *)2,  (void *)-1},            8,  0, 11, 32, 32,  1
} /*  1 */,{ "0", up_digit,{(void *)10,(void *)10,(void *)2,(void*)-1},   9,  0,  0, 33, 33,  2
} /*  2 */,{ "0", up_digit,{(void *)01,(void *)10,(void *)2,(void*)-1},  10,  0,  1, 34, 34,  3
} /*  3 */,{ "+", set_top, {(void *)2,  (void *)1},            11,  0,  2, 35, 35,  4
} /*  4 */,{ "-", set_top, {(void *)1,  (void *)-1},           18,  0,  3, 14, 14,  6
} /*  5 */,{ "0", up_digit,{(void *)10,(void *)10,(void *)1,(void*)-1},  -1,  0,  4, -1, -1,  6
} /*  6 */,{ "0", up_digit,{(void *) 1,(void *)10,(void *)1,(void*)-1},  19,  0,  4, 15, 15,  7
} /*  7 */,{ "+", set_top, {(void *)1,  (void *)1},            20,  0,  6, 15, 15, 36
} /*  8 */,{ "-", set_top, {(void *)0,  (void *)-1},           27,  0, 36, 18, 18,  9
} /*  9 */,{ "0", up_digit,{(void *)10,(void *)10,(void *)0,(void*)-1},  28,  0,  8, 19, 19, 10
} /* 10 */,{ "0", up_digit,{(void *)01,(void *)10,(void *)0,(void*)-1},  29,  0,  9, 20, 20, 11
} /* 11 */,{ "+", set_top, {(void *)0,  (void *)1},            30,  0, 10, 21, 21,  0
} /* 12 */,{ "+", inc_time, {},                                -1,  1, -1, -1,  0, 13
} /* 13 */,{ "0", up_digit,{(void *)600,(void*)10,(void*)3, (void*)-1},17,  1, 38,  4,  4, 14
} /* 14 */,{ "0:",up_digit,{(void *)60,(void *)10,(void *)3,(void*)-1},18,  1, 13,  4,  4, 15
} /* 15 */,{ "0",up_digit,{(void *)10,(void *)10,(void *)3, (void*)60},20,  1, 14,  7,  7, 16
} /* 16 */,{ "0",up_digit,{(void *)1,(void *)10,(void *)3,  (void*)60}, 21,  1, 15,  7,  7, 37
} /* 17 */,{ "-", dec_time, {},                                -1,  1, 16, -1, -1, 18
} /* 18 */,{ "\x08", change_score, {(void *)-3, (void *)0},    27,  1, 37,  8,  8, 19
} /* 19 */,{ "\x09", change_score, {(void *)-2, (void *)0},    28,  1, 18,  9,  9, 20
} /* 20 */,{ "\x0A", change_score, {(void *)-1, (void *)0},    29,  1, 19, 10, 10, 21
} /* 21 */,{ "0", up_digit, {(void*)100,(void *)100,(void *)4,(void*)-1},31,  1, 20, 11, 11, 22
} /* 22 */,{ "0", up_digit, {(void *)10,(void *)10,(void *) 4,(void*)-1}, 32,  1, 21, 11, 11, 23
} /* 23 */,{ "0", up_digit, {(void *) 1,(void *)10,(void *) 4,(void*)-1}, 33,  1, 22, 11, 11, 24
} /* 24 */,{ "\x0D", change_score, {(void*) 1, (void *)0},     35,  1, 23, 11, 11, 25
} /* 25 */,{ "\x0C", change_score, {(void*) 2, (void *)0},     36,  1, 24, 11, 11, 26
} /* 26 */,{ "\x0B", change_score, {(void*) 3, (void *)0},     37,  1, 25, 11, 11, 27
} /* 27 */,{ "\x08", change_score, {(void *)-3, (void *)1},     1,  1, 26,  0,  0, 28
} /* 28 */,{ "\x09", change_score, {(void *)-2, (void *)1},     2,  1, 27,  0,  0, 29
} /* 29 */,{ "\x0A", change_score, {(void *)-1, (void *)1},     3,  1, 28,  0,  0, 30
} /* 30 */,{ "0", up_digit, {(void*)100,(void *)100,(void *)5,(void*)-1}, 5,  1, 29,  0,  0, 31
} /* 31 */,{ "0", up_digit, {(void *)10,(void *)10,(void *)5 ,(void*)-1},  6,  1, 30,  0,  0, 32
} /* 32 */,{ "0", up_digit, {(void *) 1,(void *)10,(void *)5 ,(void*)-1},  7,  1, 31,  0,  0, 33
} /* 33 */,{ "\x0D", change_score, {(void*) 1, (void *)1},      9,  1, 32,  1,  1, 34
} /* 34 */,{ "\x0C", change_score, {(void*) 2, (void *)1},     10,  1, 33,  2,  2, 35
} /* 35 */,{ "\x0B", change_score, {(void*) 3, (void *)1},     11,  1, 34,  3,  3, 38
} /* 36 */,{ "!", send_speaker, {},                            23,  0, 7,  37, 37,  8 
} /* 37 */,{ ">", send_togprog, {},                            23,  1, 16, 36, 36, 18
} /* 38 */,{ "\x0E", send_toggle_pause, {},                    15,  1, 35, 4,   4, 13
}};

void alternate_entries(StringM *stm)
{
	if(selector_position < 0) {
		selector_position = 0;
	} else if(selector_position >= sizeof alt_entries / sizeof alt_entries[0]) {
		selector_position =   (sizeof alt_entries / sizeof alt_entries[0]) - 1;
	}

	for(int i = 0; i < sizeof alt_entries / sizeof alt_entries[0]; i++) {
		AlternateEntry *entry = &alt_entries[i];
		stm->putsxy(entry->x, entry->y, entry->text);
	}

	alt_entries[21].text[0] = (score_a / 100) % 10 + '0';
	alt_entries[22].text[0] = (score_a / 10 ) % 10 + '0';
	alt_entries[23].text[0] = (score_a      ) % 10 + '0';

	alt_entries[30].text[0] = (score_b / 100) % 10 + '0';
	alt_entries[31].text[0] = (score_b / 10 ) % 10 + '0';
	alt_entries[32].text[0] = (score_b      ) % 10 + '0';

	alt_entries[1].text[0] = ((topright / 10) % 10) + '0';
	alt_entries[2].text[0] = (topright % 10) + '0';

	alt_entries[5].text[0] = ((period / 10) % 10) + '0';
	alt_entries[6].text[0] =  (period % 10) + '0';

	alt_entries[9].text[0]  = ((topleft / 10) % 10) + '0';
	alt_entries[10].text[0] =  (topleft % 10) + '0';

	alt_entries[13].text[0] = (((timer_seconds / 60) / 10) % 10) + '0';
	alt_entries[14].text[0] =  ((timer_seconds / 60) % 10) + '0';
	alt_entries[15].text[0] = (((timer_seconds % 60) / 10) % 10) + '0';
	alt_entries[16].text[0] =  ((timer_seconds % 60) % 10) + '0';

	alt_entries[37].text[0] = (timer_info & 4) ? '<' : '>';

	alt_entries[38].text[0] = (timer_info & 1) ? '\x0E' : '\x0F';

	cursor_x = alt_entries[selector_position].x;
	cursor_y = alt_entries[selector_position].y;
}

void lcd_test(void *pvParameters)
{
	lcd.write_cb = NULL;
	lcd.font = HD44780_FONT_5X8;
	lcd.lines = 2;
	lcd.backlight = 0;
	lcd.pins.rs = GPIO_NUM_4;
	lcd.pins.e  = GPIO_NUM_16;
	lcd.pins.d4 = GPIO_NUM_21;
	lcd.pins.d5 = GPIO_NUM_17;
	lcd.pins.d6 = GPIO_NUM_19;
	lcd.pins.d7 = GPIO_NUM_18;
	lcd.pins.bl = HD44780_NOT_USED;

	ESP_ERROR_CHECK(hd44780_init(&lcd));

	hd44780_upload_character(&lcd, 0, char_data);
	hd44780_upload_character(&lcd, 1, char_data+8);
	hd44780_upload_character(&lcd, 2, char_data+8*2);
	hd44780_upload_character(&lcd, 3, char_data+8*3);
	hd44780_upload_character(&lcd, 4, char_data+8*4);
	hd44780_upload_character(&lcd, 5, char_data+8*5);
	hd44780_upload_character(&lcd, 6, char_data+8*6);
	hd44780_upload_character(&lcd, 7, char_data+8*7);

	char buf[LCD_COLUMNS];
	StringM stm = StringM();
	selector_position = 0;

	while (1)
	{
#define TAP_DELAY (600*1000)
		int newpos = -1;
		if((-4 <= input_char && input_char <= -1) &&
		   ((esp_timer_get_time() - last_tap_time > TAP_DELAY) || !already_moved)) {
			if(input_char == -1) {
				newpos = alt_entries[selector_position].left;
				cursor_counter = 0x10;
			}
			if(input_char == -2) {
				newpos =  alt_entries[selector_position].down;
				cursor_counter = 0x10;
			}
			if(input_char == -3) {
				newpos =  alt_entries[selector_position].up;
				cursor_counter = 0x10;
			}
			if(input_char == -4) {
				newpos =  alt_entries[selector_position].right;
				cursor_counter = 0x10;
			}
			already_moved = 1;
		}
		if(input_update && enter_command) {
			input_update = 0;
			enter_command = 0;
			timer_info = timer_info & (~2);
			alt_entries[selector_position].fptr(
				alt_entries[selector_position].argument);
		}
		if(newpos != -1) {
			selector_position = newpos;
		}

		stm.clear();
		alternate_entries(&stm);

		if(cursor_counter & 0x10) {
			stm.putsxy(cursor_x, cursor_y, (char *)"\xFF");
		}
		cursor_counter++;

		hd44780_gotoxy(&lcd, 0, 0);
		hd44780_puts(&lcd, stm.s1);
		hd44780_gotoxy(&lcd, 0, 1);
		hd44780_puts(&lcd, stm.s2);
		hd44780_gotoxy(&lcd, 0, 0);

		vTaskDelay(pdMS_TO_TICKS(50));
	}
}

void init(void)
{
	const uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_APB,
	};
	uart_driver_install(UART, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
	uart_param_config(UART, &uart_config);
	uart_set_pin(UART, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendData(const char* logName, const char* data)
{
	const int len = strlen(data);
	const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
	ESP_LOGI(logName, "Wrote %d bytes", txBytes);
	return txBytes;
}


static void rx_task(void *arg)
{
	uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
	uint16_t timer_seconds_last;
	while (1) {
		const int len = uart_read_bytes(UART, data, RX_BUF_SIZE,
						12 / portTICK_PERIOD_MS);
		if (len > 0) {
			timer_seconds_last = timer_seconds;
			data[len] = 0;
			uint8_t *new_data = data;

			topleft = data[0];
			topright = data[1];
			period = data[2];
			score_a = data[3];
			score_b = data[4];
			timer_info = data[5];
			timer_seconds = 0;
			timer_seconds |= data[6];
			timer_seconds |= data[7] << 8;
			
			if(timer_seconds == 0 && timer_seconds_last > timer_seconds && (timer_info & 4))
			{
				send_speaker(NULL);
			}
		}
	}
	free(data);
}

StaticTask_t lcd_stack_buffer;
#define LCD_STACK_SIZE 1024 * 4
StackType_t lcd_stack[LCD_STACK_SIZE];

StaticTask_t timer_stack_buffer;
#define TIMER_STACK_SIZE 1024 * 2 
StackType_t timer_stack[TIMER_STACK_SIZE];

StaticTask_t rx_stack_buffer;
#define RX_STACK_SIZE 1024 * 2 
StackType_t rx_stack[RX_STACK_SIZE];

StaticTask_t tx_stack_buffer;
#define TX_STACK_SIZE 1024 * 1 
StackType_t tx_stack[TX_STACK_SIZE];

StaticTask_t ble_stack_buffer;
#define BLE_STACK_SIZE 1024 * 6
StackType_t ble_stack[BLE_STACK_SIZE];

static void my_USB_DetectCB( uint8_t usbNum, void * dev )
{
	sDevDesc *device = (sDevDesc*)dev;
}


static void my_USB_PrintCB(uint8_t usbNum, uint8_t byte_depth, uint8_t* data, uint8_t data_len)
{
	if(data_len >= 8) {
		input_update = 1;
		int all_zeroes = 1;
		for(int i = 2; i < 8; i++) {

#define SET_HOTKEY(KEY, POS) \
			case (KEY) : { \
				selector_position = POS; \
				input_char = -5; \
				enter_command = 1; \
				break; \
			}

			last_tap_time = esp_timer_get_time();

			switch(data[i]) {
			case   KEY_ARROW_LEFT: input_char = -1; already_moved = 0; break;
			case     KEY_ARROW_UP: input_char = -3; already_moved = 0; break;
			case   KEY_ARROW_DOWN: input_char = -2; already_moved = 0; break;
			case  KEY_ARROW_RIGHT: input_char = -4; already_moved = 0; break;
			case  KEY_NUMPAD_PLUS:
			case         KEY_PLUS: input_char = '+'; enter_command = 1; break;
			case KEY_NUMPAD_MINUS:					
			case        KEY_MINUS: input_char = '-'; enter_command = 1; break;
			case     KEY_NUMPAD_1:
			case            KEY_1: input_char = '1'; enter_command = 1; break;
			case     KEY_NUMPAD_2:
			case            KEY_2: input_char = '2'; enter_command = 1; break;
			case     KEY_NUMPAD_3:
			case            KEY_3: input_char = '3'; enter_command = 1; break;
			case     KEY_NUMPAD_4:
			case            KEY_4: input_char = '4'; enter_command = 1; break;
			case     KEY_NUMPAD_5:
			case            KEY_5: input_char = '5'; enter_command = 1; break;
			case     KEY_NUMPAD_6:
			case            KEY_6: input_char = '6'; enter_command = 1; break;
			case     KEY_NUMPAD_7:
			case            KEY_7: input_char = '7'; enter_command = 1; break;
			case     KEY_NUMPAD_8:
			case            KEY_8: input_char = '8'; enter_command = 1; break;
			case     KEY_NUMPAD_9:
			case            KEY_9: input_char = '9'; enter_command = 1; break;
			case     KEY_NUMPAD_0:
			case            KEY_0: input_char = '0'; enter_command = 1; break;
			case KEY_NUMPAD_ENTER:
			case        KEY_ENTER: input_char = '\n'; enter_command = 1; break;
				SET_HOTKEY(KEY_ESCAPE   , 36);
				SET_HOTKEY(KEY_F1       , 33);
				SET_HOTKEY(KEY_F2       , 34);
				SET_HOTKEY(KEY_F3       , 35);
				SET_HOTKEY(KEY_F4       , 29);
				SET_HOTKEY(KEY_F5       , 24);
				SET_HOTKEY(KEY_F6       , 25);
				SET_HOTKEY(KEY_F7       , 26);
				SET_HOTKEY(KEY_F8       , 20);
				SET_HOTKEY(KEY_F9       , 3);
				SET_HOTKEY(KEY_F10      , 0);
				SET_HOTKEY(KEY_F11      , 11);
				SET_HOTKEY(KEY_F12      , 8);
				SET_HOTKEY(KEY_SPACE    , 38);
				SET_HOTKEY(KEY_PAGE_UP  , 7);
				SET_HOTKEY(KEY_PAGE_DOWN, 4);
				SET_HOTKEY(KEY_INSERT   , 13);
#undef SET_HOTKEY
			case KEY_TAB :
				if(data[0] & 4) { // alt pressed
					selector_position = 37;
					input_char = -5;
					enter_command = 1;
				}
				break;
			case KEY_DELETE :
				if(data[0] & 4) { // alt pressed
					zero_everything();
				}
				break;
			case KEY_P :
				if(data[0] & 4) { // alt pressed
					send_start_sleep();
				}
				break;
			}
			if(data[i] != 0) {
				all_zeroes = 0;
			}
		}
		if(all_zeroes) {
			input_char = 0;
		}
	}
}

#define DP_P0 22
#define DM_P0 23
#define DP_P1 -1
#define DM_P1 -1
#define DP_P2 -1
#define DM_P2 -1
#define DP_P3 -1
#define DM_P3 -1

usb_pins_config_t USB_Pins_Config =
{
	DP_P0, DM_P0,
	DP_P1, DM_P1,
	DP_P2, DM_P2,
	DP_P3, DM_P3
};

void setup()
{
	esp_timer_early_init();

	printf("TIMER_BASE_CLK: %d, TIMER_DIVIDER: %d, TIMER_SCALE: %d\n",
	       TIMER_BASE_CLK, TIMER_DIVIDER, TIMER_SCALE );
	USH.setOnConfigDescCB( Default_USB_ConfigDescCB );
	USH.setOnIfaceDescCb( Default_USB_IfaceDescCb );
	USH.setOnHIDDevDescCb( Default_USB_HIDDevDescCb );
	USH.setOnEPDescCb( Default_USB_EPDescCb );

	USH.init( USB_Pins_Config, my_USB_DetectCB, my_USB_PrintCB );

	xTaskCreateStatic(lcd_test, "lcd_test", LCD_STACK_SIZE, NULL, 5,
			lcd_stack, &lcd_stack_buffer);
	init();
	xTaskCreateStatic(rx_task, "uart_rx_task", RX_STACK_SIZE, NULL, 5,
			  rx_stack, &rx_stack_buffer);

}

void loop()
{
	vTaskDelete(NULL);
}
