#include "display.h"
#include <esp32-hal-gpio.h>

Scoreboard scoreboard;

Scoreboard::Scoreboard()
{
    set_fouls(TEAM_GUEST, 0);
    set_fouls(TEAM_HOME, 0);
    set_score(TEAM_GUEST, 0);
    set_score(TEAM_HOME, 0);
    set_period(0);
    set_time(0);
    set_timer_pause(false);
}

void Scoreboard::set_score(TEAM team, uint8_t value)
{
    value %= 200;
    DISPLAY_POSITION d_right;
    if(team == TEAM_GUEST)
    {
        score_guest = value;
        tx_buffer[3] = value;
        d_right = DP_SCORE_GUEST_RIGHT;
    }
    else
    {
        score_home = value;
        tx_buffer[4] = value;
        d_right = DP_SCORE_HOME_RIGHT;
    }
	write_7s(d_right, value % 10);
    if(value > 9)
    {
	    write_7s((DISPLAY_POSITION)(d_right + 1), (value / 10) % 10); // d_right + 1 = left display
    }
    else
    {
        display_buffer[d_right + 1] = 0;
    }
    if(value > 99)
	{
		display_buffer[d_right + 1] |= NUMBER_7S_LEADING_1;
	}
    else
    {
        display_buffer[d_right + 1] &= ~NUMBER_7S_LEADING_1;
    }
}

void Scoreboard::set_fouls(TEAM team, uint8_t value)
{
    value %= 20;
    DISPLAY_POSITION d;
    if(team == TEAM_GUEST)
    {
        fouls_guest = value;
        tx_buffer[0] = value;
        d = DP_FOULS_GUEST;
    }
    else
    {
        fouls_home = value;
        tx_buffer[1] = value;
        d = DP_FOULS_HOME;
    }
	write_7s(d, value % 10);
	if(value > 9)
	{
		display_buffer[d] |= NUMBER_7S_LEADING_1;
	}
    else
    {
        display_buffer[d] &= ~NUMBER_7S_LEADING_1;
    }
}

void Scoreboard::set_time(uint16_t value)
{
    if(value > 99 * 60 + 59) 
    {
        value = 0;
        set_timer_pause(true);
    }
    time_s = value;
    tx_buffer[6] = value & 0xff;
    tx_buffer[7] = (value>>8) & 0xff;
	write_7s(DP_TIMER_SECONDS_RIGHT, value % 10);
	write_7s(DP_TIMER_SECONDS_LEFT, (value % 60) / 10);
	write_7s(DP_TIMER_MINUTES_RIGHT, (value / 60) % 10);
	display_buffer[DP_TIMER_SECONDS_LEFT] |= NUMBER_7S_LEADING_COLON;
	write_7s(DP_TIMER_MINUTES_LEFT, (value / 60) / 10);
}

void Scoreboard::set_period(uint8_t value)
{
    value %= 10;
    period = value;
    tx_buffer[2] = value;
    write_7s(DP_PERIOD, value);
}

void Scoreboard::set_timer_pause(bool value)
{
    timer_running = !value;
    tx_buffer[5] &= ~1;
    tx_buffer[5] |= value; 
}

void Scoreboard::set_buzzer_is_buzzing(bool value)
{
    buzzer_is_buzzing = value;
    tx_buffer[5] ^= (uint8_t)value << 1;
}

void Scoreboard::set_regressive_counting(bool value)
{
    regressive_counting = value;
    tx_buffer[5] &= ~4;
    tx_buffer[5] |= (uint8_t)value << 2;
}

void pulse(gpio_num_t pin) {
	ets_delay_us(20);
	gpio_set_level(pin, 0);
	ets_delay_us(20);
	gpio_set_level(pin, 1);
	ets_delay_us(20);
}


// TODO: check if updating the display periodically is necessary
void Scoreboard::update_display()
{
    for(int i = 0; i < DP_AMOUNT; i++) {
        for(int j = 7; j >= 0; j--) {
            gpio_set_level(PIN_DATA, !(scoreboard.display_buffer[i] & (1<<j)));
	        pulse(PIN_CLOCK);
        }
    }
    pulse(PIN_STROBE);
}

void buzzer_task(void *arg)
{
	for(;;)
	{
		if(scoreboard.buzzer_is_buzzing)
		{
            scoreboard.buzzer_is_buzzing = false;
			gpio_set_level(GPIO_NUM_27, 1);
			vTaskDelay(pdMS_TO_TICKS(700));

        } 
        else
        {
            gpio_set_level(GPIO_NUM_27, 0);
            vTaskDelay(pdMS_TO_TICKS(10));
        }		
	}
	vTaskDelete(NULL);
}
