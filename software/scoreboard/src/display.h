#pragma once
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

#define PIN_CLOCK 	GPIO_NUM_21
#define PIN_STROBE	GPIO_NUM_23 // latch
#define PIN_DATA  	GPIO_NUM_22
#define PIN_SPEAKER GPIO_NUM_17

enum DISPLAY_POSITION
{
	DP_SCORE_GUEST_RIGHT, 
	DP_SCORE_GUEST_LEFT,
	DP_SCORE_HOME_RIGHT, 
	DP_SCORE_HOME_LEFT,
	DP_TIMER_SECONDS_RIGHT,
	DP_TIMER_SECONDS_LEFT,
	DP_TIMER_MINUTES_RIGHT,
	DP_TIMER_MINUTES_LEFT,
	DP_FOULS_GUEST,
	DP_PERIOD,
	DP_FOULS_HOME,
	DP_AMOUNT
};
const uint8_t NUMBER_7S[10]
{
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};
#define NUMBER_7S_LEADING_1 0x80
#define NUMBER_7S_LEADING_COLON NUMBER_7S_LEADING_1
typedef bool TEAM;
#define TEAM_GUEST 0
#define TEAM_HOME 1

struct Scoreboard
{
    uint16_t time_s;
	bool timer_running = 0;
	uint8_t score_home = 0, score_guest = 0;
	uint8_t fouls_home = 0, fouls_guest = 0;
	uint8_t period = 0;
	uint8_t tx_buffer[10];
	bool buzzer_is_buzzing = 0;
	bool regressive_counting = 0;

	uint8_t display_buffer[DP_AMOUNT]; // first byte to be sent is buffer[0]
	Scoreboard();
	inline void write_7s(DISPLAY_POSITION pos, uint8_t number) // number is 0-9
	{
		display_buffer[pos] = NUMBER_7S[number];
	}
	void set_score(TEAM team, uint8_t value);
	void set_fouls(TEAM team, uint8_t value);
	void set_time(uint16_t seconds);
	void set_period(uint8_t value);
    void set_timer_pause(bool value);
	void set_buzzer_is_buzzing(bool value);
	void set_regressive_counting(bool value);

    static void update_display();
};
extern Scoreboard scoreboard;
void buzzer_task(void *arg);
