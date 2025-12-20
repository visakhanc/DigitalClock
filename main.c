/*
 * main.c
 *
 *	Digital Clock using AVR ATmega8A
 *
 *  - Uses RemoteSensor v1 board
 *	- DS3231 RTC on I2C bus
 *	- TM1637 controlled 4 Digit display
 *	- Passive piezo-buzzer for Alarm
 *	- Single button interface
 *	- Powered by 3.7V Li-ion cell, includes LTC4054 charging IC
 *	- LED for low voltage indication etc.
 *
 *  Created on: May 20, 2022
 *      Author: Visakhan
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdbool.h>
#include <util/delay.h>
#include <string.h>

#include "board.h"
#include "avr_twi.h"
#include "tm1637.h"
#include "ds3231.h"
#include "adc.h"


/***** CONFIGURATIONS ******/

#define CONFIG_24HR_FORMAT	0		// Define to 1 for 00-23 hour display */


#define LDR_VAL1		50
#define LDR_VAL2		90
#define LDR_VAL3		140
#define LDR_VAL4		200

#define DOW_SUN 		{0x6D, 0x1C, 0x54, 0}
#define DOW_MON			{0x33, 0x27, 0x5C, 0x54}
#define DOW_TUE			{0x78, 0x3E, 0x79, 0}
#define DOW_WED			{0x3C, 0x1E, 0x79, 0x5E}
#define DOW_THU			{0x78, 0x76, 0x3E, 0}
#define DOW_FRI			{0x71, 0x50, 0x10, 0}
#define DOW_SAT			{0x6D, 0x77, 0x78, 0}


typedef enum _dispStates {
	DISP_HHMM = 0,
	DISP_SS,
	DISP_DOW,
	DISP_DATE,
	DISP_MONTH,
	DISP_ALARM,
	DISP_EDIT,
	DISP_TIMER_INIT,
	DISP_TIMER_MMSS,
//	DISP_TIMER_HHMM,
	DISP_CDT_INIT,
	DISP_CDT_MMSS,
} dispState_t;


typedef enum _editStates {
	EDIT_ALARM_INIT = 0,
	EDIT_ALARM_MIN,
	EDIT_ALARM_HOUR,
	EDIT_ALARM_SET,
	EDIT_TIME_INIT,
	EDIT_TIME_MIN,
	EDIT_TIME_HOUR,
	EDIT_TIME_DATE,
	EDIT_TIME_MONTH,
	EDIT_TIME_YEAR,
	EDIT_TIME_SET,
	EDIT_CDT_SEC,
	EDIT_CDT_MIN,
	EDIT_CDT_HOUR
} editState_t;


typedef struct _timer_t {
	uint8_t sec;
	uint8_t min;
	uint8_t hour;
	bool paused;
	bool set;
	bool expired;
} timer_t;


/* GLOBAL VARIABLES */
static ds3231_time_t 		g_time, e_time;
static ds3231_alarm_t		g_alarm;
static timer_t				inc_timer = {.paused = true};
static timer_t				cd_timer = {.paused = true};
static timer_t 				bkp_timer = {.paused = true};
static bool					alarm_on;
static bool					buzzer_on;
static uint8_t 				idle;
static volatile bool 		rtc_flag;
static volatile bool 		long_press;
static volatile bool		button_flag;
static volatile bool 		no_sleep;
static volatile uint16_t 	button_samp;
static uint8_t dow_arr[][4] = { DOW_SUN, DOW_MON, DOW_TUE, DOW_WED, DOW_THU, DOW_FRI, DOW_SAT};
static uint8_t tm[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};  /* Table for day of week calculation */

/* PRIVATE FUNCTIONS */
static void avr_init(void);
static bool check_lowbattery(void);
static void display(dispState_t state);
static void edit(editState_t state);
static void buzzer(bool on);
static uint8_t increment_bcd(uint8_t bcd);
static uint8_t bcd2bin8(uint8_t bcd);
static uint8_t bin2bcd8(uint8_t bin);
static uint8_t increment_minute(uint8_t minute);
static uint8_t increment_hour(uint8_t hour);
static uint8_t increment_date(uint8_t date);
static uint8_t increment_month(uint8_t month);
static uint8_t increment_year(uint8_t year);
static void increment_timer(timer_t *tim);
static bool decrement_timer(timer_t *tim);
static uint8_t dayofweek(uint8_t date, uint8_t month, uint16_t year);

/*  MAIN  */
int main(void)
{
	uint8_t rtc_status;
	uint8_t elapsed = 0;
	bool low_bat = false;
	dispState_t dispState = DISP_HHMM;
	editState_t editState = EDIT_ALARM_INIT;

	avr_init();
	ds3231_read_alarm2(&g_alarm, &alarm_on);
	tm1637_set_brightness(TM1637_DISPLAY_PW_1_16);
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);

	while(1)
	{

		if(rtc_flag) {
			rtc_flag = false;
			ds3231_read_status(&rtc_status);

			ds3231_read_time(&g_time);   /* Read time from RTC */
			if(!inc_timer.paused) {  /* Increment Timer */
				increment_timer(&inc_timer);
			}
			if((!cd_timer.paused) && cd_timer.set) {
				cd_timer.expired = decrement_timer(&cd_timer);
				if(cd_timer.expired) {
					idle = 0;
					elapsed = 0;
					cd_timer.set = false;
					dispState = DISP_CDT_MMSS;
					buzzer_on = true;  // prevent sleep
					buzzer(true);
				}
			}
			if(cd_timer.expired) {
				if(++elapsed > 2) {
					cd_timer.expired = false;
					dispState = DISP_HHMM;
					buzzer_on = false;  // can sleep now
					buzzer(false);
				}
			}

			if(dispState != DISP_EDIT) {
				if(DISP_HHMM == dispState) {
					if(idle < 10) ++idle;
				}
				if(10 == idle) {
					switch(g_time.sec & 0xF) {
					case 8: dispState = DISP_DOW; break;
					case 9:	dispState = DISP_DATE; break;
					default: dispState = DISP_HHMM;
					}
				}
				display(dispState);
			}
			else {
				edit(editState);
			}

			if(alarm_on && (g_alarm.hour == g_time.hour) && (g_alarm.min == g_time.min) && ((g_alarm.sec == g_time.sec))) {  /* DS3231 Alarm2 A2F flag will not set so, check we for Alarm match here */
				if(!buzzer_on) {
					buzzer_on = true;  // prevent sleep
					idle = 0;
					elapsed = 0;
					dispState = DISP_ALARM;
					buzzer(true); /* Start buzzer tone */
				}
			}
			else {
				if(buzzer_on) {
					if(++elapsed > 30) {
						buzzer_on = false;   // can sleep now
						buzzer(false);
						dispState = DISP_HHMM;
					}
				}
			}

			if(!(g_time.sec & 0xF)) { /* Sample battery voltage every 10 sec */
				low_bat = check_lowbattery();
			}

			if(low_bat && !(g_time.sec & 0x1)) { /* Every 2 sec issue low battery indication */
				LED_ON();
				_delay_ms(40);
				LED_OFF();
			}

		}

		if(button_flag) {
			button_flag = false;
			if(!long_press) {
				no_sleep = false;
			}

			idle = 0;
			switch (dispState) {
			case DISP_HHMM:
				if(long_press) {
					dispState = DISP_EDIT;
					editState = EDIT_ALARM_INIT;
				}
				else {
					dispState = DISP_SS;
				}
				break;

			case DISP_SS:
				dispState = (long_press) ? DISP_TIMER_INIT : DISP_DOW;
				break;

			case DISP_DOW:
				dispState = DISP_DATE;
				break;

			case DISP_DATE:
				dispState = (long_press) ? DISP_MONTH : DISP_HHMM;
				break;

			case DISP_MONTH:
				if(long_press) {
					dispState = DISP_EDIT;
					editState = EDIT_TIME_INIT;
				}
				else {
					dispState = DISP_HHMM;
				}
				break;

			case DISP_TIMER_INIT:
				if(long_press) {
					dispState = DISP_CDT_INIT;
				}
				else {
					inc_timer.paused = false;
					dispState = DISP_TIMER_MMSS;
				}
				break;

			case DISP_TIMER_MMSS:
				if(long_press) {
					if(inc_timer.paused) {
						bkp_timer = inc_timer;  // Save current timer value for CDT
						inc_timer.hour = 0;
						inc_timer.min = 0;
						inc_timer.sec = 0;
						dispState = DISP_TIMER_INIT;
					}
					else {
						dispState = DISP_HHMM;
					}
				}
				else {
					inc_timer.paused = !inc_timer.paused;
				}
				break;

			case DISP_CDT_INIT:
				if(long_press) {
					dispState = DISP_HHMM;
				}
				else {
					if(!cd_timer.set) {
						cd_timer = bkp_timer;
						dispState = DISP_EDIT;
						editState = EDIT_CDT_SEC;
					}
					else {
						dispState = DISP_CDT_MMSS;
					}
				}
				break;

			case DISP_CDT_MMSS:
				if(long_press) {
					if(cd_timer.paused) {  /* Reset CDT */
						cd_timer = bkp_timer;
						dispState = DISP_EDIT;
						editState = EDIT_CDT_SEC;
					}
					else {
						dispState = DISP_HHMM;
					}
				}
				else {
					cd_timer.paused = !cd_timer.paused;
				}
				break;

			case DISP_ALARM:
				dispState = DISP_HHMM;
				buzzer_on = false;  // if false, would be set true again at the alarm match check
				buzzer(false);
				break;

			case DISP_EDIT:
				switch(editState) {
				case EDIT_ALARM_INIT:
					if(long_press) {
						if(alarm_on) {
							if(!ds3231_alarm2_onoff(ALARM_OFF)) {
								alarm_on = false;
							}
						}
						else {
							dispState = DISP_HHMM;
						}
					}
					else {
						editState = EDIT_ALARM_MIN;
						alarm_on = false; // Prevent alarm going off during set up
					}
					break;

				case EDIT_ALARM_MIN:
					if(long_press) {
						editState = EDIT_ALARM_HOUR;
					}
					else {
						g_alarm.min = increment_minute(g_alarm.min);
					}
					break;

				case EDIT_ALARM_HOUR:
					if(long_press) {
						editState = EDIT_ALARM_SET;
						g_alarm.day_date = g_time.date; // Day/Date is irrelevant for DAILY alarm type */
						ds3231_set_alarm2(&g_alarm, ALARM_DAILY);
						if(!ds3231_alarm2_onoff(ALARM_ON)) {
							alarm_on = true;
						}
					}
					else {
						g_alarm.hour = increment_hour(g_alarm.hour);
					}
					break;

				case EDIT_ALARM_SET:
				case EDIT_TIME_SET:
					dispState = DISP_HHMM;
					break;

				case EDIT_TIME_INIT:
					if(long_press) {
						dispState = DISP_HHMM;
					}
					else {
						editState = EDIT_TIME_MIN;
						e_time = g_time;
					}
					break;

				case EDIT_TIME_MIN:
					if(long_press) {
						editState = EDIT_TIME_HOUR;
					}
					else {
						e_time.min = increment_minute(e_time.min);
					}
					break;

				case EDIT_TIME_HOUR:
					if(long_press) {
						editState = EDIT_TIME_DATE;
					}
					else {
						e_time.hour = increment_hour(e_time.hour);
					}
					break;

				case EDIT_TIME_DATE:
					if(long_press) {
						editState = EDIT_TIME_MONTH;
					}
					else {
						e_time.date = increment_date(e_time.date);
					}
					break;

				case EDIT_TIME_MONTH:
					if(long_press) {
						editState = EDIT_TIME_YEAR;
					}
					else {
						e_time.month = increment_month(e_time.month);
					}
					break;

				case EDIT_TIME_YEAR:
					if(long_press) {
						editState = EDIT_TIME_SET;
						e_time.sec = 0;
						e_time.day = dayofweek(bcd2bin8(e_time.date), bcd2bin8(e_time.month), 2000+bcd2bin8(e_time.year)) + 1;  // Day of week is in the range 1-7
						ds3231_set_time(&e_time);
					}
					else {
						e_time.year = increment_year(e_time.year);
					}
					break;

				case EDIT_CDT_SEC:
					if(long_press) {
						editState = EDIT_CDT_MIN;
					}
					else {
						if(++cd_timer.sec > 59) {
							cd_timer.sec = 0;
						}
					}
					break;

				case EDIT_CDT_MIN:
					if(long_press) {
						editState = EDIT_CDT_HOUR;
					}
					else {
						if(++cd_timer.min > 59) {
							cd_timer.min = 0;
						}
					}
					break;

				case EDIT_CDT_HOUR:
					if(long_press) { // Start counting down
						cd_timer.set = true;
						cd_timer.paused = false;
						dispState = DISP_CDT_MMSS;
					}
					else {
						if(++cd_timer.hour > 99) {
							cd_timer.hour = 0;
						}
					}
					break;
				}

			}

			if(dispState != DISP_EDIT) {
				display(dispState);
			}
			else {
				edit(editState);
			}
		}

		GICR |= (1 << INT1);
		if(!no_sleep && !buzzer_on) {
			sleep_mode();
			TWI_Reset();
		}
	}

	return 0;
}


static void avr_init(void)
{
	LED_INIT();
	CHRG_INIT();
	BUTTON_INIT();

	LED_ON();
	_delay_ms(500);
	LED_OFF();


	adc_init(ADC_PRESCALER_64, ADC_VREF_INTERNAL);
	adc_select_channel(BAT_ADC_CHANNEL);
	adc_samp();

	/* Buzzer */
	DDRC |= (1 << PC3);
	TCCR1A = 0;
	TCCR1B = 0;
	OCR1A = 149; /* Interrupt every 150us */
	TIMSK |= (1 << OCIE1A)|(1 << TOIE0);  /* Enable Timer1 Compare Interrupt and Timer0 overflow interrupt */


	MCUCR &= ~((1 << ISC11)|(1 << ISC10)|(1 << ISC01)|(1 << ISC00)); /* Low Level INT1 and INT0 (required for Power down mode) */
	GICR |= (1 << INT1)|(1 << INT0);

	sei();	/* For I2C driver !!*/
	tm1637_init();
	if(ds3231_init()) {
		LED_ON();
	}


}


static bool check_lowbattery(void)
{
	static uint8_t adc_count = 0;
	static uint16_t adc_sum = 0;
	bool low_bat = false;
	uint16_t samp;
	uint8_t ldr_val;

	ADC_ENABLE();
	adc_select_channel(BAT_ADC_CHANNEL);
	_delay_ms(1); /* Start-up time for Internal bandgap reference voltage */
	samp = adc_samp();
	adc_sum += samp;
	if(++adc_count == 4) {
		adc_sum >>= 2;
		if(adc_sum < 600) { /* VBAT < 3.0V */
			low_bat = true;
		}
		if(adc_sum > 680) { /* VBAT > 3.4V */
			low_bat = false;
		}
		adc_sum = 0;
		adc_count = 0;
	}

#if 0  // LDR not tested yet
	/* Sample LDR value and adjust LED brightness */
	adc_select_channel(LDR_ADC_CHANNEL);
	ADC_LEFT_ADJUST();
	ldr_val = adc_samp_8();
	ADC_RIGHT_ADJUST();
	ADC_DISABLE();
	if(ldr_val < LDR_VAL1) {
		tm1637_set_brightness(TM1637_DISPLAY_PW_1_16);
	}
	else if(ldr_val < LDR_VAL2) {
		tm1637_set_brightness(TM1637_DISPLAY_PW_2_16);
	}
	else if(ldr_val < LDR_VAL3) {
		tm1637_set_brightness(TM1637_DISPLAY_PW_4_16);
	}
	else if (ldr_val < LDR_VAL4) {
		tm1637_set_brightness(TM1637_DISPLAY_PW_10_16);
	}
#endif

	return low_bat;
}



void display(dispState_t state)
{
	uint8_t digit_buf[4] = {0};
	uint8_t dot_pos = 0;
	uint8_t hour = g_time.hour;

#if !CONFIG_24HR_FORMAT
	hour = bcd2bin8(hour);  // Time read from RTC is 24hr format, so convert to 12hr format
	if(hour > 12) {
		hour -= 12;
	}
	else if(!hour) {
		hour = 12;
	}
	hour = bin2bcd8(hour);
#endif

	switch(state) {
	case DISP_HHMM:
	case DISP_ALARM:
		tm1637_bcd_to_2digits(hour, &digit_buf[0], false);
		tm1637_bcd_to_2digits(g_time.min, &digit_buf[2], true);
		dot_pos = (g_time.sec & 0x1) ? 2 : 0;
		if((DISP_ALARM == state) && !(g_time.sec & 0x1)) {
			dot_pos = 3;
			do {
				digit_buf[dot_pos] = 0;
			} while(dot_pos--);
		}
		break;

	case DISP_SS:
		tm1637_bcd_to_2digits(g_time.sec, &digit_buf[2], true);
		dot_pos = 2;
		break;

	case DISP_DATE:
		digit_buf[0] = 0x5E; // 'd'
		//digit_buf[1] = 0x78; // 't'
		tm1637_bcd_to_2digits(g_time.date, &digit_buf[2], false);
		break;

	case DISP_MONTH:
		digit_buf[0] = 0x33;
		digit_buf[1] = 0x27; // 'M'
		tm1637_bcd_to_2digits(g_time.month, &digit_buf[2], false);
		break;

	case DISP_TIMER_INIT:
		digit_buf[0] = 0x78; //'t'
		digit_buf[1] = 0x10; //'i'
		tm1637_bcd_to_2digits(bin2bcd8(inc_timer.sec), &digit_buf[2], true);
		dot_pos = 2;
		break;

	case DISP_TIMER_MMSS:
		tm1637_bcd_to_2digits(bin2bcd8(inc_timer.min), &digit_buf[0], true);
		tm1637_bcd_to_2digits(bin2bcd8(inc_timer.sec), &digit_buf[2], true);
		dot_pos = 2;
		break;

	case DISP_CDT_INIT:
		digit_buf[0] = 0x39; // 'C'
		digit_buf[1] = 0x5E; // 'd'
		tm1637_bcd_to_2digits(bin2bcd8(cd_timer.sec), &digit_buf[2], true);
		dot_pos = 2;
		break;

	case DISP_CDT_MMSS:
		tm1637_bcd_to_2digits(bin2bcd8(cd_timer.min), &digit_buf[0], true);
		tm1637_bcd_to_2digits(bin2bcd8(cd_timer.sec), &digit_buf[2], true);
		dot_pos = 2;
		break;

	case DISP_DOW:
	case DISP_EDIT:
		break;
	}

	if((DISP_DOW == state) && g_time.day) {
		tm1637_send_digits(dow_arr[g_time.day-1], 4, dot_pos);
	}
	else {
		tm1637_send_digits(digit_buf, sizeof(digit_buf), dot_pos);
	}
}



static void edit(editState_t state)
{
	uint8_t digit_buf[4] = {0};
	uint8_t dot_pos = 0;

	switch(state) {
	case EDIT_ALARM_INIT:
		digit_buf[0] = 0x77; // 'A'
		digit_buf[1] = 0x38; // 'L'
		digit_buf[2] = 0x5C; // 'o'
		digit_buf[3] = (alarm_on) ? 0x54 : 0x71; // 'n'/'f'
		dot_pos = 2;
		break;

	case EDIT_ALARM_MIN:
		tm1637_bcd_to_2digits(g_alarm.hour, &digit_buf[0], true);
		if(g_time.sec & 0x1) {
			tm1637_bcd_to_2digits(g_alarm.min, &digit_buf[2], true);
		}
		dot_pos = 2;
		break;

	case EDIT_ALARM_HOUR:
		if(g_time.sec & 0x1) {
			tm1637_bcd_to_2digits(g_alarm.hour, &digit_buf[0], true);
		}
		tm1637_bcd_to_2digits(g_alarm.min, &digit_buf[2], true);
		dot_pos = 2;
		break;

	case EDIT_ALARM_SET:
	case EDIT_TIME_SET:
		digit_buf[0] = 0x6D; // 'S'
		digit_buf[1] = 0x79; // 'E'
		digit_buf[2] = 0x78; // 't'
		break;

	case EDIT_TIME_INIT:
		digit_buf[0] = 0x79; // 'E'
		digit_buf[1] = 0x5E; // 'd'
		digit_buf[2] = 0x10; // 'i'
		digit_buf[3] = 0x78; // 't'
		break;

	case EDIT_TIME_MIN:
		tm1637_bcd_to_2digits(e_time.hour, &digit_buf[0], true);
		if(g_time.sec & 0x1) {
			tm1637_bcd_to_2digits(e_time.min, &digit_buf[2], true);
		}
		dot_pos = 2;
		break;

	case EDIT_TIME_HOUR:
		if(g_time.sec & 0x1) {
			tm1637_bcd_to_2digits(e_time.hour, &digit_buf[0], true);
		}
		tm1637_bcd_to_2digits(e_time.min, &digit_buf[2], true);
		dot_pos = 2;
		break;

	case EDIT_TIME_DATE:
		tm1637_bcd_to_2digits(e_time.month, &digit_buf[0], false);
		if(g_time.sec & 0x1) {
			tm1637_bcd_to_2digits(e_time.date, &digit_buf[2], false);
		}
		break;

	case EDIT_TIME_MONTH:
		if(g_time.sec & 0x1) {
			tm1637_bcd_to_2digits(e_time.month, &digit_buf[0], false);
		}
		tm1637_bcd_to_2digits(e_time.date, &digit_buf[2], false);
		break;

	case EDIT_TIME_YEAR:
		if(g_time.sec & 0x1) {
			tm1637_bcd_to_2digits(0x20, &digit_buf[0], true);
			tm1637_bcd_to_2digits(e_time.year, &digit_buf[2], true);
		}
		break;

	case EDIT_CDT_SEC:
		tm1637_bcd_to_2digits(bin2bcd8(cd_timer.min), &digit_buf[0], true);
		if(g_time.sec & 0x1) {
			tm1637_bcd_to_2digits(bin2bcd8(cd_timer.sec), &digit_buf[2], true);
		}
		break;

	case EDIT_CDT_MIN:
		if(g_time.sec & 0x1) {
			tm1637_bcd_to_2digits(bin2bcd8(cd_timer.min), &digit_buf[0], true);
		}
		tm1637_bcd_to_2digits(bin2bcd8(cd_timer.sec), &digit_buf[2], true);
		break;

	case EDIT_CDT_HOUR:
		if(g_time.sec & 0x1) {
			tm1637_bcd_to_2digits(bin2bcd8(cd_timer.hour), &digit_buf[0], true);
		}
		tm1637_bcd_to_2digits(bin2bcd8(cd_timer.min), &digit_buf[2], true);
		break;
	}
	tm1637_send_digits(digit_buf, sizeof(digit_buf), dot_pos);

}



static void increment_timer(timer_t *tim)
{
	if(++tim->sec > 59) {
		tim->sec = 0;
		if(++tim->min > 59) {
			tim->min = 0;
			if(++tim->hour > 99) {
				tim->hour = 0;
			}
		}
	}
}


static bool decrement_timer(timer_t *tim)
{
	if((0 == tim->hour) && (0 == tim->min) && (0 == tim->sec)) {  /* Timer expired */
		return true;
	}
	if((0 == tim->sec--) && (tim->min)) {
		tim->sec = 59;
		if((0 == tim->min--) && (tim->hour)) {
			tim->min = 59;
			tim->hour--;
		}
	}
	return false;
}


static uint8_t dayofweek(uint8_t date, uint8_t month, uint16_t year)
{
	uint16_t temp;

	if (month < 3) {
		year--;
	}

	temp = year + year/4 - year/100 + year/400 + tm[month - 1] + date;
	return (uint8_t)(temp % 7);

}



static __inline__ uint8_t increment_bcd(uint8_t bcd)
{
	return (9 == (bcd & 0xF)) ? (bcd+7) : (bcd+1);
}


/* Converts BCD (less than 100) to binary */
static __inline__ uint8_t bcd2bin8(uint8_t bcd)
{
	return ((bcd >> 4)*10 + (bcd & 0x0F));
}


/* Converts binary (less than 100) to BCD */
static __inline__ uint8_t bin2bcd8(uint8_t bin)
{
	return ((bin/10)<<4 | (bin%10));
}



static __inline__ uint8_t increment_minute(uint8_t minute)
{
	uint8_t ret = increment_bcd(minute);
	if(ret > 0x59) {
		ret = 0;
	}
	return ret;
}


static __inline__ uint8_t increment_hour(uint8_t hour)
{
	uint8_t ret = increment_bcd(hour);
	if(ret > 0x23) {
		ret = 0;
	}
	return ret;
}


static __inline__ uint8_t increment_date(uint8_t date)
{
	uint8_t ret = increment_bcd(date);
	if(ret > 0x31) {
		ret = 1;
	}
	return ret;
}


static __inline__ uint8_t increment_month(uint8_t month)
{
	uint8_t ret = increment_bcd(month);
	if(ret > 0x12) {
		ret = 1;
	}
	return ret;
}


static __inline__ uint8_t increment_year(uint8_t year)
{
	uint8_t ret = increment_bcd(year);
	if(ret > 0x99) {
		ret = 0x25;
	}
	return ret;
}


/* Start/Stop buzzer tone */
static void buzzer(bool on)
{
	if(on) {
		TCCR1B = (1 << WGM12)|0x2;
	}
	else {
		TCCR1B = 0;
	}
}



/* External Interrupt from DS3231 RTC */
ISR(INT1_vect)
{
	GICR &= ~(1 << INT1); /* Disable Level triggered INT1 interrupt */
	rtc_flag = true;
}


/* External Interrupt from Button */
ISR(INT0_vect)
{
	if(MCUCR & (1 << ISC00)) {  /* Rising Edge INT0 from button release */
		//button_flag = true;  /* Button released */
		if(button_samp < 100) {
			button_flag = true;
			long_press = false;
		}
		else {
			no_sleep = false;
		}
		TCCR0 = 0;
		MCUCR &= ~((1 << ISC01)|(1 << ISC00)); /* Back to Low Level INT0 (required for Power down mode) */
		//no_sleep = false;
	}
	else {  /* Low Level INT0 from Button press */
		//GICR &= ~(1 << INT0);  /* Disable level triggered INT0 interrupt */
		MCUCR |= (1 << ISC01)|(1 << ISC00);  /* Enable Rising Edge INT0 Interrupt */
		no_sleep = true; /* Prevent sleep while button is depressed to be able to detect button release */

		/* Start sampling button press */
		button_samp = 0;
		TCNT0 = 0;
		TCCR0 = 4; /* Start Timer0 at 8MHz/256 ~= 31kHz; Overflow occurs at 31kHz/256 = 122Hz (8ms) */
	}

}


/* Timer0 Overflow Interrupt for Button sampling */
ISR(TIMER0_OVF_vect)
{
	if(BUTTON_PRESSED()) {
		button_samp++;
		if(button_samp > 100) {
			button_flag = true;
			long_press = true;
			TCCR0 = 0;
		}
	}

}




/* Timer1 Compare A Match Interrupt for Buzzer output signal generation */
ISR(TIMER1_COMPA_vect)
{
	static bool space = false;
	static uint8_t pulse = 0;
	static uint16_t int_count = 0;

	if(!space) {
		if(!(pulse & 0x1)) {
			PORTC ^= (1 << PC3);
		}
		if(++int_count == 500) {  /* 75 ms */
			int_count = 0;
			if(++pulse == 7) {
				pulse = 0;
				space = true;
			}
		}
	}
	else {
		if(++int_count == 4700) {  /* 750 ms */
			space = false;
			int_count = 0;
		}
	}
}



