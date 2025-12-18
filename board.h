/**
 *	File 			: board.h
 *	Description 	: Digital Clock board common definitions
 *	Author 			: Visakhan C
 *	Date			: 2019-11-17
 */
 
#include <avr/io.h>

/* Various pins brought out in the board

	Name			Pin (ATmega8 TQFP)
	-----------------------------------

	PC1	(ADC1)			24 (Wired to BAT_ADC (Resistor Divider))
	PC2	(ADC2)			25 (Wired to CHRG of LTC4054)
	PC3 (ADC3)			26 (Wired to Base of transistor driving Buzzer)
	PC4 (SDA)(ADC4)		27 (Wired to SDA of DS3231 RTC module)
	PC5 (SCL)(ADC5)		28 (Wired to SCL of DS3231 RTC module)
	PD2 (INT0)			32 (Wired to Button)
	PD0 (RXD)			30 (NC)
	PD1 (TXD)			31 (Wired to CLK of TM1637 LED Driver module)
	PD4 				2  (Wired to DIO of TM1637 LED Driver module)
	PD6					10 (Wired to LED)
	PD7 (AIN1)			11 (NC)
	PB0 (ICP1)			12 (NC)

	PB3 (MOSI)			15 (ISP)
	PB4 (MISO)			16 (ISP)
	PB5 (SCK)			17 (ISP)
	PB2 (SS)			14 (NC)
	PB1 (OC1A)			13 (NC)
	PD3 (INT1)      	1  (Wired to INT of DS3231 RTC module)



*/

/* LED sinks current from 3.3V */
#define LED					PD6
#define LED_PORT			PORTD
#define LED_DDR				DDRD
	
/* Active low button (Not present on second version - with HC-49 crystal) */
#define BUTTON				PD2
#define BUTTON_PORT			PORTD
#define BUTTON_PIN			PIND
#define BUTTON_DDR			DDRD

/* Battery Charging indication - Active low
(NOT WORKING(H/W BUG) - DOES NOT GO ACTIVE LOW WHEN CHARGING ***/
#define CHRG				PC2
#define CHRG_PORT			PORTC
#define CHRG_PIN			PINC
#define CHRG_DDR			DDRC


#define BAT_ADC_CHANNEL		ADC_CHANNEL_1
#define LDR_ADC_CHANNEL		ADC_CHANNEL_2


/*********************** MACROS *************************/

#define LED_INIT()			(LED_DDR |= (1 << LED))
#define LED_ON()			(LED_PORT &= ~(1 << LED))
#define LED_OFF()			(LED_PORT |= (1 << LED))
#define LED_TOGGLE()		(LED_PORT ^= (1 << LED))

#define BUTTON_INIT()		(BUTTON_PORT |= (1 << BUTTON))	/* Enable internal pullup */
#define BUTTON_PRESSED()	((BUTTON_PIN & (1 << BUTTON)) == 0)

#define CHRG_INIT()			(CHRG_PORT |= (1 << CHRG))  /* Enable internal pullup */
#define BAT_CHARGING()		((CHRG_PIN & (1 << CHRG)) == 0)
