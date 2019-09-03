#include "app_error.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include "app_scheduler.h"
#include "app_simple_timer.h"

#include "boards.h"
#include "bsp.h"

#include "nordic_common.h"
#include "nrf.h"
#include "nrf_drv_clock.h"
#include "nrf_drv_gpiote.h"
#include "nrf_error.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_drv_rng.h"
#include "nrf_assert.h"
#include "SEGGER_RTT.h"
#include<time.h>
#include<stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
// Scheduler settings
#define SCHED_MAX_EVENT_DATA_SIZE       MAX(APP_TIMER_SCHED_EVENT_DATA_SIZE, 0)
#define SCHED_QUEUE_SIZE                10

#define LED_1_PIN                       BSP_LED_0     // LED 1 on the nRF51-DK or nRF52-DK
#define LED_2_PIN                       BSP_LED_1     // LED 3 on the nRF51-DK or nRF52-DK
#define LED_3_PIN                       BSP_LED_2     // LED 3 on the nRF51-DK or nRF52-DK
#define BUTTON_1_PIN                    BSP_BUTTON_0  // Button 1 on the nRF51-DK or nRF52-DK
#define BUTTON_2_PIN                    BSP_BUTTON_1  // Button 2 on the nRF51-DK or nRF52-DK
#define BUTTON_3_PIN                    BSP_BUTTON_2  // Button 1 on the nRF51-DK or nRF52-DK
#define BUTTON_4_PIN                    BSP_BUTTON_3
// General application timer settings.
#define APP_TIMER_PRESCALER             16    // Value of the RTC1 PRESCALER register.
#define APP_TIMER_OP_QUEUE_SIZE         2     // Size of timer operation queues.

// Application timer ID.
APP_TIMER_DEF(m_led_a_timer_id);

#define BUTTON_PREV_ID           0                           /**< Button used to switch the state. */
#define BUTTON_NEXT_ID           1                           /**< Button used to switch the state. */
#define RNG_ENABLED 1
static bsp_indication_t actual_state =  BSP_INDICATE_FIRST;         /**< Currently indicated state. */
static const char * indications_list[] = BSP_INDICATIONS_LIST;

#define ST7586_SPI_INSTANCE  0/**< SPI instance index. */
static const nrf_drv_spi_t st7586_spi = NRF_DRV_SPI_INSTANCE(ST7586_SPI_INSTANCE);  /**< SPI instance. */
static volatile bool st7586_spi_xfer_done = false;  /**< Flag used to indicate that SPI instance completed the transfer. */

#define ST_COMMAND   0
#define ST_DATA      1

#define RATIO_SPI0_LCD_SCK          4
#define RATIO_SPI0_LCD_A0          28
#define RATIO_SPI0_LCD_MOSI          29
#define RATIO_SPI0_LCD_BSTB          30
#define RATIO_SPI0_LCD_CS         31

#define LCD_INIT_DELAY(t) nrf_delay_ms(t)

static unsigned char rx_data;
#define RANDOM_BUFF_SIZE    16
int level = 1 ; //game level
int check_collision = 0;//충돌했을 때 다음 충돌 때까지 딜레이를 위한 변수
int life = 2;//목숨의 갯수
int hidden_stat = 0;//대각선으로 떨어지는 알파벳을 표현하기 위해
int check_wall[30];//대각선으로 떨어질때 벽에 부딪히는 걸 판단
int paus=0;//pause 상태를 나타냄
int man_x=20;//유저의 초깃값
int man_y=147;//유저의 초깃값
int xran;//랜덤변수
uint8_t p_buff[RANDOM_BUFF_SIZE];
int f[30][3];//알파벳의 갯수 30개 , 0:대각선인지 아닌지  1:x값 2:y값
int smogtrue = 0; //smog 상태인지 아닌지
int smoglevel = 0;//smog상태 일 때 display 다루기 위해
int i =0;//
int levelscore = 0;//30이 되면 초기화하고 다음 레벨로
int score1=0;//roll
int temp = 0;//roll
int bb = 0 ;//bomb effect 중인지 아닌지
int height=147;//bomb effect 높이
int bombnum = 2;//폭탄갯수
int lifealpha = 0;
void bombeffect(int height);
void makeF(int idx,int y);//함수선언
void eraseF(int idx);
void displayman(uint8_t x,uint8_t y);
void manwhite();
void man();
void collision(int life);
void first();
void gameclear();
void spi_event_handler(nrf_drv_spi_evt_t const * p_event,
		void *                    p_context)
{
	st7586_spi_xfer_done = true;
}


void st7586_write(const uint8_t category, const uint8_t data)
{
	int err_code;
	nrf_gpio_pin_write(RATIO_SPI0_LCD_A0, category);

	st7586_spi_xfer_done = false;
	err_code = nrf_drv_spi_transfer(&st7586_spi, &data, 1, &rx_data, 0);
	APP_ERROR_CHECK(err_code);
	while (!st7586_spi_xfer_done) {
		__WFE();
	}
	nrf_delay_us(10);
}

static inline void st7586_pinout_setup()
{
	// spi setup
	int err_code;
	nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
	spi_config.ss_pin   = RATIO_SPI0_LCD_CS;
	spi_config.miso_pin = NRF_DRV_SPI_PIN_NOT_USED;
	spi_config.mosi_pin = RATIO_SPI0_LCD_MOSI;
	spi_config.sck_pin  = RATIO_SPI0_LCD_SCK;
	spi_config.frequency = NRF_SPI_FREQ_1M;
	spi_config.mode = NRF_DRV_SPI_MODE_3;

	err_code = nrf_drv_spi_init(&st7586_spi, &spi_config, spi_event_handler, NULL);
	APP_ERROR_CHECK(err_code);

	nrf_gpio_pin_set(RATIO_SPI0_LCD_A0);
	nrf_gpio_cfg_output(RATIO_SPI0_LCD_A0);

	nrf_gpio_pin_clear(RATIO_SPI0_LCD_BSTB);
	nrf_gpio_cfg_output(RATIO_SPI0_LCD_BSTB);
}


bool main_context ( void )
{
	static const uint8_t ISR_NUMBER_THREAD_MODE = 0;
	uint8_t isr_number =__get_IPSR();
	if ((isr_number ) == ISR_NUMBER_THREAD_MODE)
	{
		return true;
	}
	else
	{
		return false;
	}
}
// Timeout handler for the repeated timer
static void timer_handler(void * p_context)
{
	// Toggle LED 1.
	//,,,,,,,,,,nrf_drv_gpiote_out_toggle(LED_1_PIN);

	// Light LED 3 if running in main context and turn it off if running in an interrupt context.
	// This has no practical use in a real application, but it is useful here in the tutorial.
	if (main_context())
	{
		nrf_drv_gpiote_out_clear(LED_3_PIN);
		NRF_LOG_INFO("in main context...\r\n");
		//SEGGER_RTT_printf(0,"main timer\n");
	}
	else
	{
		nrf_drv_gpiote_out_set(LED_3_PIN);
		NRF_LOG_INFO("in interrupt context...\r\n");
		//SEGGER_RTT_printf(0,"interrupt timer\n");
	}
}

void button_handler(nrf_drv_gpiote_pin_t pin)
{

	uint32_t err_code;
	//int32_t TICKS = APP_TIMER_TICKS(200);

	// Handle button presses.
	switch (pin)
	{
	case BUTTON_1_PIN:
		// updated for SDK ver. 13+
		//err_code = app_timer_start(m_led_a_timer_id, APP_TIMER_TICKS(200, APP_TIMER_PRESCALER), NULL);
		err_code = app_timer_start(m_led_a_timer_id, APP_TIMER_TICKS(50), NULL);
		//APP_TIMER_TICKS() -> 1 TICKS의 시간 조절
		APP_ERROR_CHECK(err_code);
		if(0<man_x && paus == 0)
		{
			displayman(man_x,man_y);
			manwhite();
			displayman(--man_x,man_y);
			man();
		}
		NRF_LOG_INFO("started timer\r\n");
		break;
	case BUTTON_2_PIN:
		if(man_x<38 && paus == 0)
		{
			displayman(man_x,man_y);
			manwhite();
			displayman(++man_x,man_y);
			man();
		}
		NRF_LOG_INFO("stop\r\n");
		//NRF_LOG_FLUSH();

		break;
	case BUTTON_3_PIN:
		if(paus==0)
			paus=1;
		else
			paus=0;
		NRF_LOG_INFO("stop\r\n");
		//NRF_LOG_FLUSH();

		break;
	case BUTTON_4_PIN:
		bb=1;
		break;
	default:
		break;
	}

	// Light LED 2 if running in main context and turn it off if running in an interrupt context.
	// This has no practical use in a real application, but it is useful here in the tutorial.
	if (main_context())
	{
		//SEGGER_RTT_printf(0,"main btn\n");
		nrf_drv_gpiote_out_clear(LED_2_PIN);
	}
	else
	{
		//SEGGER_RTT_printf(0,"interrupt btn\n");
		nrf_drv_gpiote_out_set(LED_2_PIN);
	}
}
// Button handler function to be called by the scheduler.
void button_scheduler_event_handler(void *p_event_data, uint16_t event_size)
{
	// In this case, p_event_data is a pointer to a nrf_drv_gpiote_pin_t that represents
	// the pin number of the button pressed. The size is constant, so it is ignored.
	button_handler(*((nrf_drv_gpiote_pin_t*)p_event_data));
}
//Button event handler.
void gpiote_event_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
	// The button_handler function could be implemented here directly, but is
	// extracted to a separate function as it makes it easier to demonstrate
	// the scheduler with less modifications to the code later in the tutorial.
	app_sched_event_put(&pin, sizeof(pin), button_scheduler_event_handler);
}


// Function for configuring GPIO.
static void gpio_config()
{
	ret_code_t err_code;

	// Initialze driver.
	err_code = nrf_drv_gpiote_init();
	APP_ERROR_CHECK(err_code);

	// Configure 3 output pins for LED's.
	nrf_drv_gpiote_out_config_t out_config = GPIOTE_CONFIG_OUT_SIMPLE(false);
	err_code = nrf_drv_gpiote_out_init(LED_1_PIN, &out_config);
	APP_ERROR_CHECK(err_code);
	err_code = nrf_drv_gpiote_out_init(LED_2_PIN, &out_config);
	APP_ERROR_CHECK(err_code);
	err_code = nrf_drv_gpiote_out_init(LED_3_PIN, &out_config);
	APP_ERROR_CHECK(err_code);

	// Set output pins (this will turn off the LED's).
	nrf_drv_gpiote_out_set(LED_1_PIN);
	nrf_drv_gpiote_out_set(LED_2_PIN);
	nrf_drv_gpiote_out_set(LED_3_PIN);

	// Make a configuration for input pints. This is suitable for both pins in this example.
	nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
	in_config.pull = NRF_GPIO_PIN_PULLUP;

	// Configure input pins for buttons, with separate event handlers for each button.
	err_code = nrf_drv_gpiote_in_init(BUTTON_1_PIN, &in_config, gpiote_event_handler);
	APP_ERROR_CHECK(err_code);
	err_code = nrf_drv_gpiote_in_init(BUTTON_2_PIN, &in_config, gpiote_event_handler);
	APP_ERROR_CHECK(err_code);
	err_code = nrf_drv_gpiote_in_init(BUTTON_3_PIN, &in_config, gpiote_event_handler);
	APP_ERROR_CHECK(err_code);
	err_code = nrf_drv_gpiote_in_init(BUTTON_4_PIN, &in_config, gpiote_event_handler);
	APP_ERROR_CHECK(err_code);

	// Enable input pins for buttons.
	nrf_drv_gpiote_in_event_enable(BUTTON_1_PIN, true);
	nrf_drv_gpiote_in_event_enable(BUTTON_2_PIN, true);
	nrf_drv_gpiote_in_event_enable(BUTTON_3_PIN, true);
	nrf_drv_gpiote_in_event_enable(BUTTON_4_PIN, true);
}


// Function starting the internal LFCLK oscillator.
// This is needed by RTC1 which is used by the Application Timer
// (When SoftDevice is enabled the LFCLK is always running and this is not needed).
static void lfclk_request(void)
{
	uint32_t err_code = nrf_drv_clock_init();
	APP_ERROR_CHECK(err_code);
	nrf_drv_clock_lfclk_request(NULL);
}


//////////////////////////////////////




void white(void)
{
	if(life==0)
	{
		st7586_write(ST_COMMAND, 0x2A);//set col
		st7586_write(ST_DATA, 0x00);
		st7586_write(ST_DATA, 0);
		st7586_write(ST_DATA, 0x00);
		st7586_write(ST_DATA, 43);

		st7586_write(ST_COMMAND, 0x2B);//set row
		st7586_write(ST_DATA, 0x00);
		st7586_write(ST_DATA, 13);
		st7586_write(ST_DATA, 0x00);
		st7586_write(ST_DATA, 147);
	}
	else
	{
		st7586_write(ST_COMMAND, 0x2A);//set col
		st7586_write(ST_DATA, 0x00);
		st7586_write(ST_DATA, 0);
		st7586_write(ST_DATA, 0x00);
		st7586_write(ST_DATA, 43);

		st7586_write(ST_COMMAND, 0x2B);//set row
		st7586_write(ST_DATA, 0x00);
		st7586_write(ST_DATA, 0);
		st7586_write(ST_DATA, 0x00);
		st7586_write(ST_DATA, 160);
	}
	uint8_t i,j;
	st7586_write(ST_COMMAND, 0x2C);
	for(i=0;i<160;i++)
	{
		for(j=0;j<128;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
}
void display_address(uint8_t x,uint8_t y)
{


	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, x);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, x+2);

	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y+11);
}
void F(void)
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);

	for(i=0;i<3;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}


	for(i=0;i<3;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}


}
void Nulll()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);

	for(i=0;i<12;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
}

void smog()
{

	uint8_t i,j;
	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 43);

	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 70);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 110);
	st7586_write(ST_COMMAND,0x2C);


	for(i=0;i<13;i++)
	{
		for(j=0;j<44;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}
	for(i =0 ; i<3;i++)
	{
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}


	}
	for(i =0 ; i<3;i++)
	{
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}
	for(i =0 ; i<3;i++)
	{
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		/*
			for(j=0;j<5;j++)
			{
				st7586_write(ST_DATA,0x00);
			}*/
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}


	}//3

	for(i =0 ; i<3;i++)
	{
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}


	}//4
	for(i =0 ; i<3;i++)
	{
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}


	}//5


	for(i=0;i<13;i++)
	{
		for(j=0;j<44;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}


}
void clean()
{

	uint8_t i,j;
	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 43);

	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 70);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 110);
	st7586_write(ST_COMMAND,0x2C);

	for(i=0;i<41;i++)
	{

		for(j=0;j<44;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}


}
void A()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);

	for(i=0;i<3;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}


	for(i=0;i<3;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}
}
void B()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<2;i++)
	{
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<3;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}

	for(i=0;i<2;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<3;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}
	for(i=0;i<2;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
}
void C()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<3;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}


	for(i=0;i<6;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<3;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}


	}
}
void D()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<3;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}


	for(i=0;i<6;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}
}
void Aplus()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);


	st7586_write(ST_DATA,0x00);st7586_write(ST_DATA,0xFF);st7586_write(ST_DATA,0x00);
	st7586_write(ST_DATA,0xFF);st7586_write(ST_DATA,0xFF);st7586_write(ST_DATA,0xFF);
	st7586_write(ST_DATA,0x00);st7586_write(ST_DATA,0xFF);st7586_write(ST_DATA,0x00);
	st7586_write(ST_DATA,0x00);st7586_write(ST_DATA,0x00);st7586_write(ST_DATA,0x00);
	for(i=0;i<1;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}


	for(i=0;i<1;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}
}
void setStart(uint8_t i)

{
	st7586_write(ST_COMMAND,0x37);
	st7586_write(ST_DATA,i);
}
void zero()

{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}


	for(i=0;i<2;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}
	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}}
void one()
{uint8_t i,j;
st7586_write(ST_COMMAND,0x2C);
for(i=0;i<2;i++)
{
	for(j=0;j<2;j++)
	{
		st7586_write(ST_DATA,0x00);
	}
	for(j=0;j<1;j++)
	{
		st7586_write(ST_DATA,0xFF);
	}

}
for(i=0;i<3;i++)
{

	for(j=0;j<2;j++)
	{
		st7586_write(ST_DATA,0x00);
	}
	for(j=0;j<1;j++)
	{
		st7586_write(ST_DATA,0xFF);
	}
}


for(i=0;i<2;i++)
{

	for(j=0;j<2;j++)
	{
		st7586_write(ST_DATA,0x00);
	}
	for(j=0;j<1;j++)
	{
		st7586_write(ST_DATA,0xFF);
	}

}
for(i=0;i<3;i++)
{

	for(j=0;j<2;j++)
	{
		st7586_write(ST_DATA,0x00);
	}
	for(j=0;j<1;j++)
	{
		st7586_write(ST_DATA,0xFF);
	}
}
for(i=0;i<2;i++)
{

	for(j=0;j<2;j++)
	{
		st7586_write(ST_DATA,0x00);
	}
	for(j=0;j<1;j++)
	{
		st7586_write(ST_DATA,0xFF);
	}

}
}
void two()
{uint8_t i,j;
st7586_write(ST_COMMAND,0x2C);
for(i=0;i<2;i++)
{

	for(j=0;j<3;j++)
	{
		st7586_write(ST_DATA,0xFF);
	}

}
for(i=0;i<3;i++)
{

	for(j=0;j<2;j++)
	{
		st7586_write(ST_DATA,0x00);
	}
	for(j=0;j<1;j++)
	{
		st7586_write(ST_DATA,0xFF);
	}
}


for(i=0;i<2;i++)
{

	for(j=0;j<3;j++)
	{
		st7586_write(ST_DATA,0xFF);
	}

}
for(i=0;i<3;i++)
{

	for(j=0;j<1;j++)
	{
		st7586_write(ST_DATA,0xFF);
	}
	for(j=0;j<2;j++)
	{
		st7586_write(ST_DATA,0x00);
	}

}
for(i=0;i<2;i++)
{

	for(j=0;j<3;j++)
	{
		st7586_write(ST_DATA,0xFF);
	}

}
}
void three()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<2;i++)
	{
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}

	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}
	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
}

void four()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<5;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}
	for(i=0;i<3;i++)
	{
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}

	for(i=0;i<4;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}


}
void five()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<2;i++)
	{
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}

	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}
	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
}
void six()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<3;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<3;i++)
	{
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}

	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}


}
void seven()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<3;i++)
	{
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<4;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}

	for(i=0;i<5;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}

}
void eight()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<2;i++)
	{
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}

	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}
	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
}
void nine()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<2;i++)
	{
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}

	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<5;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}

}
void scorenum(int score)//점수를 그리는 함수
		{
	uint8_t first,second,third;//일의 자리 십의자리 백의 자리
	third = score/100; //score를 100으로 나눈 수의 몫이 세번째자리수
	first = score%10; //10으로 나눈 수의 나머지가 첫번쨰 자리수
	second = (score - (third*100))/10;//스코어에서 세번쨰 자리수 *100 한것을 빼고 십의 자리만 남으면 그것을 10으로 나눈 것의 몫이 두번째 자리 수
	SEGGER_RTT_printf(0,"first = %d  second = %d\n",first,second);
	display_address(24,0);//1 번째 숫자
	switch(third)
	{
	case 0: {zero(); break;}
	case 1: {one(); break;}
	case 2: {two(); break;}
	case 3: {three(); break;}
	case 4: {four(); break;}
	case 5: {five(); break;}
	case 6: {six(); break;}
	case 7: {seven(); break;}
	case 8: {eight(); break;}
	case 9: {nine(); break;}

	}

	display_address(27,0);//2번째 숫자
	switch(second)
	{
	case 0: {zero(); break;}
	case 1: {one(); break;}
	case 2: {two(); break;}
	case 3: {three(); break;}
	case 4: {four(); break;}
	case 5: {five(); break;}
	case 6: {six(); break;}
	case 7: {seven(); break;}
	case 8: {eight(); break;}
	case 9: {nine(); break;}

	}


	display_address(30,0);//3번쨰 숫자
	switch(first)
	{
	case 0: {zero(); break;}
	case 1: {one(); break;}
	case 2: {two(); break;}
	case 3: {three(); break;}
	case 4: {four(); break;}
	case 5: {five(); break;}
	case 6: {six(); break;}
	case 7: {seven(); break;}
	case 8: {eight(); break;}
	case 9: {nine(); break;}

	}
	//display_address(34,0);
	//A();
	//display_address(38,0);
	//A();

		}

static void create_timers()
{
	//SEGGER_RTT_printf(0,"create timers\n");
	uint32_t err_code;

	// Create timers
	err_code = app_timer_create(&m_led_a_timer_id,
			APP_TIMER_MODE_REPEATED,
			timer_handler);
	APP_ERROR_CHECK(err_code);
}


void roll()
{
	if(check_collision > 40)//roll 한번 될 때마다 +1 되는데 40 이상되면 다시 충돌체크를 할 수 있게 됨
		check_collision = 0;

	for(int b=0;b<30;b++)//30개를 모두 비교
	{
		if(f[b][2]>0)
		{

			if(smogtrue == 1 && f[b][2]>70 && f[b][2]<111)//스모그 상태에서 알파벳의 위치가 스모그 안이면 위치값만 수정해주고 프린트는 하지않는다
			{

				if(f[b][0] == 0 && check_wall[b] == 1)//대각선 알파벳이고 왼쪽으로 이동 중이면
				{

					check_wall[b] = 1;
					f[b][1] = f[b][1] - 1;//x 값을 -1 하면서~
					if(f[b][1] == 0)//0이 되면 오른쪽으로 이동
					{
						check_wall[b] = 2;
					}

				}else if(f[b][0] == 0 && check_wall[b] == 2)//대각선 알파벳이고 오른쪽으로 이동 중이면
				{
					f[b][1] = f[b][1] + 1;//x 값을 +1 하면서~
					if(f[b][1] == 39)//오른쪽 벽에 붙으면 왼쪽으로 이동
					{
						check_wall[b] = 1;
					}
				}
				f[b][2]= f[b][2] + 1;//y = y + 1; init 13
			}else if(smogtrue == 1 && f[b][2]<71 &&f[b][2]>57)//스모그 상태에서 그 바로 위에 있으면  지워준다
			{
				if(f[b][2]==58)
				{
					eraseF(b);
				}
				if(f[b][0] == 0 && check_wall[b] == 1)
				{

					check_wall[b] = 1;
					f[b][1] = f[b][1] - 1;
					if(f[b][1] == 0)
					{
						check_wall[b] = 2;
					}

				}else if(f[b][0] == 0 && check_wall[b] == 2)
				{
					f[b][1] = f[b][1] + 1;
					if(f[b][1] == 39)
					{
						check_wall[b] = 1;
					}
				}
				f[b][2]= f[b][2] + 1;
			}
			else{
				eraseF(b);
				if(f[b][0] == 0 && check_wall[b] == 1)
				{

					check_wall[b] = 1;
					f[b][1] = f[b][1] - 1;
					if(f[b][1] == 0)
					{
						check_wall[b] = 2;
					}

				}else if(f[b][0] == 0 && check_wall[b] == 2)
				{
					f[b][1] = f[b][1] + 1;
					if(f[b][1] == 39)
					{
						check_wall[b] = 1;
					}
				}
				f[b][2]= f[b][2] + 1;//y = y + 1; init 13
				makeF(b,f[b][2]);
			}



		}
		if(f[b][2]>136 && check_collision == 0)//136이상이면 사람과 닿을 수 있는 환경이다 충돌체크가 0이면 충돌할 수 있다
		{
			for(int i =0 ; i<5 ; i ++)//5칸 충돌비교
			{
				if((f[b][1]+1) == (man_x+i) )//사람과 알파벳이 닿으면
				{
					if(f[b][0] != 29)
					{
						life--;
					}else{
						life++;
					}

					collision(life);//collision이 일어날떄 life가 급속히 줄어듬 delay가 필요
					check_collision = 1;//충돌체크가 1이되면 충돌을 판단하지않는다
					break;
				}

			}

		}


		if(f[b][2]>147)// 끝까지 도달하면 score++
		{
			switch(level)
			{
			case 1: score1 = score1 + 1;break;
			case 2:	score1 = score1 + 2;break;
			case 3: score1 = score1 + 3;break;
			case 4: score1 = score1 + 4;break;
			case 5: score1 = score1 + 5;break;
			}
			levelscore++;//30이되면 레벨을 올린다
			scorenum(score1);
			eraseF(b);//끝까지가면 지운다
			check_wall[b] = 1;//초기화과정 다음알파벳단계를 위해
			f[b][0] = -1;//
			f[b][2] = -1;//
			f[b][1]	= -1;//
			if(levelscore%30 == 0)
			{

				i=0;
				level++;
			}
		}



	}
	switch(level)//level 에 따라 떨어지는 속도 조절
	{
	case 1: nrf_delay_ms(70); break;
	case 2: nrf_delay_ms(50); break;
	case 3: nrf_delay_ms(30); break;
	case 4: nrf_delay_ms(20); break;
	case 5: nrf_delay_ms(10); break;

	}

	temp++;//그에 따라 temp++
	if(check_collision >0)
	{
		check_collision++;
	}//30X33 ==1000 ==1초
}

void bsp_evt_handler(bsp_event_t evt)//button event handler
{
	uint32_t err_code;
	switch (evt)
	{
	case BSP_EVENT_KEY_0:
		if (actual_state != BSP_INDICATE_FIRST)
			actual_state--;
		else
			actual_state = BSP_INDICATE_LAST;
		break;

	case BSP_EVENT_KEY_1:

		if (actual_state != BSP_INDICATE_LAST)
			actual_state++;
		else
			actual_state = BSP_INDICATE_FIRST;
		break;

	default:
		return; // no implementation needed
	}
	err_code = bsp_indication_set(actual_state);
	NRF_LOG_INFO("%s", (uint32_t)indications_list[actual_state]);
	APP_ERROR_CHECK(err_code);
}

void clock_initialization()
{
	NRF_CLOCK->LFCLKSRC            = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
	NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
	NRF_CLOCK->TASKS_LFCLKSTART    = 1;

	while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
	{
		// Do nothing.
	}
}

void bsp_configuration()
{
	uint32_t err_code;

	err_code = bsp_init(BSP_INIT_LED | BSP_INIT_BUTTONS, bsp_evt_handler);
	APP_ERROR_CHECK(err_code);
}

void score()//UI
{

	uint8_t i,j;
	display_address(0,0);
	st7586_write(ST_COMMAND,0x2C);

	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}


	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}
	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	display_address(4,0);
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}


	for(i=0;i<2;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}
	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	display_address(8,0);
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}


	for(i=0;i<2;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}
	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	display_address(12,0);
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}


	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}


	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}
	for(i=0;i<2;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}

	display_address(16,0);
	st7586_write(ST_COMMAND,0x2C);

	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}


	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}
	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}

	display_address(20,0);
	st7586_write(ST_COMMAND,0x2C);

	for(i=0;i<2;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}


	for(i=0;i<2;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}
	for(i=0;i<2;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}

}

void init(void)
{
	//srand(time(NULL));
	bsp_board_leds_init();

	APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
	NRF_LOG_DEFAULT_BACKENDS_INIT();

	st7586_pinout_setup();

	clock_initialization();

	uint32_t err_code = app_timer_init();
	APP_ERROR_CHECK(err_code);




	bsp_configuration();//btn handler

	NRF_LOG_INFO("SPI example.");
	nrf_gpio_pin_write(RATIO_SPI0_LCD_BSTB, 0);
	LCD_INIT_DELAY(10);
	nrf_gpio_pin_write(RATIO_SPI0_LCD_BSTB, 1);


	LCD_INIT_DELAY(120);


	st7586_write(ST_COMMAND, 0xD7);  // Disable Auto Read
	st7586_write(ST_DATA, 0x9F);
	st7586_write(ST_COMMAND, 0xE0);  // Enable OTP Read
	st7586_write(ST_DATA, 0x00);
	LCD_INIT_DELAY(10);
	st7586_write(ST_COMMAND, 0xE3);  // OTP Up-Load
	LCD_INIT_DELAY(20);
	st7586_write(ST_COMMAND, 0xE1);  // OTP Control Out
	st7586_write(ST_COMMAND, 0x11);  // Sleep Out
	st7586_write(ST_COMMAND, 0x28);  // Display OFF
	LCD_INIT_DELAY(50);

	st7586_write(ST_COMMAND,  0xC0);
	st7586_write(ST_DATA, 0x53);
	st7586_write(ST_DATA, 0x01);
	st7586_write(ST_COMMAND,  0xC3);
	st7586_write(ST_DATA, 0x02);
	st7586_write(ST_COMMAND,  0xC4);
	st7586_write(ST_DATA, 0x06);

	st7586_write(ST_COMMAND, 0xD0);  // Enable Analog Circuit
	st7586_write(ST_DATA, 0x1D);
	st7586_write(ST_COMMAND, 0xB5);  // N-Line = 0
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_COMMAND, 0x39);  // Monochrome Mode
	st7586_write(ST_COMMAND, 0x3A);  // Enable DDRAM Interface
	st7586_write(ST_DATA, 0x02);
	st7586_write(ST_COMMAND, 0x36);  // Scan Direction Setting
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_COMMAND, 0xB0);  // Duty Setting
	st7586_write(ST_DATA, 0x9F);

	st7586_write(ST_COMMAND, 0xB4);  // Partial Display
	st7586_write(ST_DATA, 0xA0);
	st7586_write(ST_COMMAND, 0x30);  // Partial Display Area = COM0 ~ COM119
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0xFF);
	st7586_write(ST_DATA, 0xFF);
	st7586_write(ST_COMMAND, 0x20);  // Display Inversion OFF
	st7586_write(ST_COMMAND, 0x2A);  // Column Address Setting
	st7586_write(ST_DATA, 0x00);  // SEG0 -> SEG384
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x7F);
	st7586_write(ST_COMMAND, 0x2B);  // Row Address Setting
	st7586_write(ST_DATA, 0x00);  // COM0 -> COM160
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x9F);
	//Clear_DDRAM();  // Clear whole DDRAM by “0” (384 x 160 x 2)
	/*st7586_write(ST_COMMAND, 0x2A);  // Column Address Setting
      st7586_write(ST_COMMAND, 0x00);  // SEG0 -> SEG239
      st7586_write(ST_COMMAND, 0x00);
      st7586_write(ST_DATA, 0x00);
      st7586_write(ST_DATA, 0x4F);
      st7586_write(ST_COMMAND, 0x2B);  // Row Address Setting
      st7586_write(ST_DATA, 0x00);  // COM0 -> COM120
      st7586_write(ST_DATA, 0x00);
      st7586_write(ST_DATA, 0x00);
      st7586_write(ST_DATA, 0x78);*/
	//Disp_Image();  // Fill the DDRAM Data by Panel Resolution
	st7586_write(ST_COMMAND, 0x29);  // Display ON



	LCD_INIT_DELAY(2000);
}
static uint8_t random_vector_generate(uint8_t * p_buff, uint8_t size)
{
	uint32_t err_code;
	uint8_t  available;

	nrf_drv_rng_bytes_available(&available);
	uint8_t length = MIN(size, available);

	err_code = nrf_drv_rng_rand(p_buff, length);
	APP_ERROR_CHECK(err_code);

	return length;
}
void eraseF(int idx)//idx에 해당하는 알파벳의 전 위치를 지워줌
{
	display_address(f[idx][1],f[idx][2]);//위치를 받아와서
	st7586_write(ST_COMMAND,0x2C);
	for(int i =0 ; i< 12;i++)
	{
		for(int j = 0 ; j<3; j++)
		{
			st7586_write(ST_DATA,0x00);//흰색으로 지워줌
		}
	}
}
void makeF(int idx,int y)//알파벳 생성
{

	random_vector_generate(p_buff,RANDOM_BUFF_SIZE);//랜덤변수
	xran = p_buff[0]%39 ;//알파벳의 초기 위칫값을 위한 변수 xran
	hidden_stat =  p_buff[0]%5 ; // hidden_stat = 0, 1, 2, 3, 4 히든 스텟은 0일 떄 대각선 알파벳이 된다
	lifealpha = p_buff[0]%30; // 알파벳이 목숨으로 나올 때

	if(f[idx][1] == -1)//초깃값이 -1 이기때문에 아직 화면에 생성되지 않은 배열이면 랜덤으로 x값을 설정해주고 대각선인지를 판단해준다
	{
		f[idx][1] = xran;
		if(lifealpha < 7)
		{
			lifealpha = 0;
		}
		f[idx][0] = lifealpha;//0 or 29 or other 29면 life
	}
	display_address(f[idx][1],y);//좌표값을 받아온것을 설정
	//SEGGER_RTT_printf(0,"alpha = %d\n",alpha);
	f[idx][2] = y;
	if(f[idx][0] == 29)
	{
		SEGGER_RTT_printf(0,"main plus/n");
		Aplus();
	}else{
		switch(level)//레벨에 따라 알파벳을 만든다
		{
		case 1:F();break;
		case 2:D();break;
		case 3:C();break;
		case 4:B();break;
		case 5:A();break;
		}
	}



}
void displayman(uint8_t x,uint8_t y)//사람을 그릴 때 쓴다 x y 값을 받아와 주소를 설정
{

	//SEGGER_RTT_WriteString(0, "In address\r\n");
	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, x);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, x+4);

	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y+11);
}
void man()//사람을 그린다
{

	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);

	for(i=0;i<5;i++)
	{

		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}

	for(i=0;i<1;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}


	}
	for(i=0;i<1;i++)
	{

		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}
	for(i=0;i<1;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}


	}
	for(i=0;i<1;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}


	for(i=0;i<2;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}


}
void manpause()
{

	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);

	for(i=0;i<11;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		st7586_write(ST_DATA,0x00);
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}





}
void manwhite()//남자를 지운다
{

	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);

	for(i=0;i<5;i++)
	{

		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}

	for(i=0;i<1;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}


	}
	for(i=0;i<1;i++)
	{

		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<1;i++)
	{

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}


	}
	for(i=0;i<1;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}


	for(i=0;i<2;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}


}
void glcd_white(void)
{
	//SEGGER_RTT_WriteString(0, "In white\r\n");
	uint8_t i,j;
	st7586_write(ST_COMMAND, 0x2C);

	for(i=0;i<160;i++)
	{
		for(j=0;j<128;j++)
		{
			st7586_write(ST_DATA, 0x00);
		}
	}

}


void clear(){
	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x7F);
	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x9F);
	glcd_white();
}
void lcd_init(){
	bsp_board_leds_init();

	APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
	NRF_LOG_DEFAULT_BACKENDS_INIT();

	st7586_pinout_setup();

	NRF_LOG_INFO("SPI example.");
	nrf_gpio_pin_write(RATIO_SPI0_LCD_BSTB, 0);
	LCD_INIT_DELAY(10);
	nrf_gpio_pin_write(RATIO_SPI0_LCD_BSTB, 1);


	LCD_INIT_DELAY(120);


	st7586_write(ST_COMMAND, 0xD7);  // Disable Auto Read
	st7586_write(ST_DATA, 0x9F);
	st7586_write(ST_COMMAND, 0xE0);  // Enable OTP Read
	st7586_write(ST_DATA, 0x00);
	LCD_INIT_DELAY(10);
	st7586_write(ST_COMMAND, 0xE3);  // OTP Up-Load
	LCD_INIT_DELAY(20);
	st7586_write(ST_COMMAND, 0xE1);  // OTP Control Out
	st7586_write(ST_COMMAND, 0x11);  // Sleep Out
	st7586_write(ST_COMMAND, 0x28);  // Display OFF
	LCD_INIT_DELAY(50);

	st7586_write(ST_COMMAND,  0xC0);
	st7586_write(ST_DATA, 0x53);
	st7586_write(ST_DATA, 0x01);
	st7586_write(ST_COMMAND,  0xC3);
	st7586_write(ST_DATA, 0x02);
	st7586_write(ST_COMMAND,  0xC4);
	st7586_write(ST_DATA, 0x06);

	st7586_write(ST_COMMAND, 0xD0);  // Enable Analog Circuit
	st7586_write(ST_DATA, 0x1D);
	st7586_write(ST_COMMAND, 0xB5);  // N-Line = 0
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_COMMAND, 0x39);  // Monochrome Mode
	st7586_write(ST_COMMAND, 0x3A);  // Enable DDRAM Interface
	st7586_write(ST_DATA, 0x02);
	st7586_write(ST_COMMAND, 0x36);  // Scan Direction Setting
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_COMMAND, 0xB0);  // Duty Setting
	st7586_write(ST_DATA, 0x9F);
	/*st7586_write(ST_COMMAND, 0xB4);  // Partial Display
   st7586_write(ST_DATA, 0xA0);
   st7586_write(ST_COMMAND, 0x30);  // Partial Display Area = COM0 ~ COM119
   st7586_write(ST_DATA, 0x10);
   st7586_write(ST_DATA, 0x00);
   st7586_write(ST_DATA, 0x00);
   st7586_write(ST_DATA, 0x77);*/
	st7586_write(ST_COMMAND, 0x20);  // Display Inversion OFF
	st7586_write(ST_COMMAND, 0x2A);  // Column Address Setting
	st7586_write(ST_DATA, 0x00);  // SEG0 -> SEG384
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x7F);
	st7586_write(ST_COMMAND, 0x2B);  // Row Address Setting
	st7586_write(ST_DATA, 0x00);  // COM0 -> COM160
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x9F);
	//Clear_DDRAM();  // Clear whole DDRAM by ??(384 x 160 x 2)

	//Disp_Image();  // Fill the DDRAM Data by Panel Resolution
	st7586_write(ST_COMMAND, 0x29);  // Display ON
	LCD_INIT_DELAY(100);
	//////////////////////////////////////////
	clear();
	LCD_INIT_DELAY(500);

}
void button_address(uint8_t x,uint8_t y)
{


	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, x);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, x+22);

	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y+9);
}
void arrow_address(uint8_t x,uint8_t y)
{


	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, x);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, x+9);

	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y+9);
}
void leftarrow()
{

	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<1;i++)
	{
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<1;i++)
	{
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<6;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<1;i++)
	{
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<1;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<1;i++)
	{

		for(j=0;j<10;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}


	}
	for(i=0;i<1;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<1;i++)
	{
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<1;i++)
	{
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<6;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<1;i++)
	{
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
}
void rightarrow()
{

	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<1;i++)
	{
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<1;i++)
	{
		for(j=0;j<6;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<1;i++)
	{
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}
	for(i=0;i<1;i++)
	{
		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}
	for(i=0;i<1;i++)
	{

		for(j=0;j<10;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}


	}

	for(i=0;i<1;i++)
	{
		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}
	for(i=0;i<1;i++)
	{
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}
	for(i=0;i<1;i++)
	{
		for(j=0;j<6;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}
	for(i=0;i<1;i++)
	{
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}

}

void pause()
{
	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<10;i++)
	{
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}

}

void bomb(uint8_t x,uint8_t y)
{

	uint8_t i,j;

	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, x);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, x+14);

	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y+9);

	st7586_write(ST_COMMAND,0x2C);

	for(i=0;i<1;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<11;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}//1
	for(i=0;i<1;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<11;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}//2
	for(i=0;i<1;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<11;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}//3
	for(i=0;i<1;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<11;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}//4
	for(i=0;i<1;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<11;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}//5
	for(i=0;i<1;i++)
	{
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}//6
	for(i=0;i<1;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x0F);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x0F);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}


	}//7
	for(i=0;i<1;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x0F);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x0F);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}


	}//8
	for(i=0;i<1;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x0F);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x0F);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}


	}//9
	for(i=0;i<1;i++)
	{
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}


		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x0F);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x0F);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}


	}//10
}

void button(void)
{
	uint8_t i,j;
	st7586_write(ST_COMMAND, 0x2C);
	for(i=0;i<10;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<9;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		//1



		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<9;j++)
		{
			st7586_write(ST_DATA,0x00);
		}//2



		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<9;j++)
		{
			st7586_write(ST_DATA,0x00);
		}//3



		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<9;j++)
		{
			st7586_write(ST_DATA,0x00);
		}//4


		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0x00);
		}//5



		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}//6



		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}


		//7


		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}//8


		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}//9



		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		//10

	}
}

void button1()
{
	button();
	display_address(24,100);
	one();
	arrow_address(29,100);
	leftarrow();
}
void button2()
{

	button();
	display_address(24,115);
	two();
	arrow_address(29,115);
	rightarrow();

}
void button3()
{

	button();
	display_address(24,130);
	three();
	arrow_address(29,130);
	pause();


}
void button4()
{

	button();
	display_address(24,145);
	four();
	bomb(28,145);


}
void gameclear()
{
	uint8_t i,j;
while(1)
{
	st7586_write(ST_COMMAND, 0x2A);//set col
						st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 0);
						st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 43);

						st7586_write(ST_COMMAND, 0x2B);//set row
						st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 0);
						st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 59);
						st7586_write(ST_COMMAND, 0x2C);
						for(i=0;i<60;i++)
						{
							for(j=0;j<44;j++)
								st7586_write(ST_DATA, 0x00);
						}

						st7586_write(ST_COMMAND, 0x2A);//set col
						st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 0);
						st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 4);

						st7586_write(ST_COMMAND, 0x2B);//set row
						st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 60);
						st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 69);
						st7586_write(ST_COMMAND, 0x2C);
						for(i=0;i<10;i++)
						{
							for(j=0;j<5;j++)
								st7586_write(ST_DATA, 0x00);
						}

						st7586_write(ST_COMMAND, 0x2A);//set col
						st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 38);
						st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 43);

						st7586_write(ST_COMMAND, 0x2B);//set row
						st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 60);
						st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 69);
						st7586_write(ST_COMMAND, 0x2C);
						for(i=0;i<10;i++)
						{
							for(j=0;j<6;j++)
								st7586_write(ST_DATA, 0x00);
						}
	st7586_write(ST_COMMAND, 0x2A);//set col
		st7586_write(ST_DATA, 0x00);
		st7586_write(ST_DATA, 5);
		st7586_write(ST_DATA, 0x00);
		st7586_write(ST_DATA, 37);

		st7586_write(ST_COMMAND, 0x2B);//set row
		st7586_write(ST_DATA, 0x00);
		st7586_write(ST_DATA, 60);
		st7586_write(ST_DATA, 0x00);
		st7586_write(ST_DATA, 69);


			st7586_write(ST_COMMAND, 0x2C);

			for(j=0;j<2;j++)
			{
				for(i=0;i<5;i++)
					st7586_write(ST_DATA, 0xFF);
				st7586_write(ST_DATA, 0x00);st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<6;i++)
					st7586_write(ST_DATA, 0x00);
				for(i=0;i<5;i++)
					st7586_write(ST_DATA, 0xFF);
				for(i=0;i<2;i++)
					st7586_write(ST_DATA, 0x00);
				for(i=0;i<5;i++)
					st7586_write(ST_DATA, 0xFF);
				for(i=0;i<2;i++)
					st7586_write(ST_DATA, 0x00);

				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<4;i++)
					st7586_write(ST_DATA, 0x00);
			}//1,2

			for(j=0;j<2;j++)
			{

				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<6;i++)
					st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<6;i++)
					st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<6;i++)
					st7586_write(ST_DATA, 0x00);

				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<3;i++)
					st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0xFF);

				for(i=0;i<2;i++)
					st7586_write(ST_DATA, 0x00);
				for(i=0;i<5;i++)
					st7586_write(ST_DATA, 0xFF);
			}//3,4

			for(j=0;j<2;j++)
			{

				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<6;i++)
					st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<6;i++)
					st7586_write(ST_DATA, 0x00);


				for(i=0;i<5;i++)
					st7586_write(ST_DATA, 0xFF);
				for(i=0;i<2;i++)
					st7586_write(ST_DATA, 0x00);
				for(i=0;i<5;i++)
					st7586_write(ST_DATA, 0xFF);
				for(i=0;i<2;i++)
					st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<4;i++)
					st7586_write(ST_DATA, 0x00);

			}//5,6

			for(j=0;j<2;j++)
			{

				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<6;i++)
					st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<6;i++)
					st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<6;i++)
					st7586_write(ST_DATA, 0x00);

				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<3;i++)
					st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<2;i++)
					st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<4;i++)
					st7586_write(ST_DATA, 0x00);
			}//7,8

			for(j=0;j<2;j++)
			{

				for(i=0;i<5;i++)
					st7586_write(ST_DATA, 0xFF);
				st7586_write(ST_DATA, 0x00);st7586_write(ST_DATA, 0x00);
				for(i=0;i<5;i++)
					st7586_write(ST_DATA, 0xFF);
				st7586_write(ST_DATA, 0x00);st7586_write(ST_DATA, 0x00);
				for(i=0;i<5;i++)
					st7586_write(ST_DATA, 0xFF);
				st7586_write(ST_DATA, 0x00);st7586_write(ST_DATA, 0x00);

				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<3;i++)
					st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<2;i++)
					st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0xFF);
				for(i=0;i<4;i++)
					st7586_write(ST_DATA, 0x00);
			}//9,10
			//clear

			st7586_write(ST_COMMAND, 0x2A);//set col
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 0);
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 43);

			st7586_write(ST_COMMAND, 0x2B);//set row
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 70);
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 79);
			st7586_write(ST_COMMAND, 0x2C);
			for(i=0;i<10;i++)
			{
				for(j=0;j<44;j++)
					st7586_write(ST_DATA, 0x00);
			}

			st7586_write(ST_COMMAND, 0x2A);//set col
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 0);
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 0);

			st7586_write(ST_COMMAND, 0x2B);//set row
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 80);
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 99);
			st7586_write(ST_COMMAND, 0x2C);
			for(i=0;i<40;i++)
			{
				for(j=0;j<1;j++)
					st7586_write(ST_DATA, 0x00);
			}

			st7586_write(ST_COMMAND, 0x2A);//set col
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 1);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 41);

					st7586_write(ST_COMMAND, 0x2B);//set row
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 80);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 99);


					st7586_write(ST_COMMAND, 0x2C);

					for(j=0;j<4;j++)
					{

						for(i=0;i<20;i++)
							st7586_write(ST_DATA, 0xFF);
						st7586_write(ST_DATA, 0x00);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0xFF);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0x00);

					}//1,2,3,4
					for(j=0;j<4;j++)
					{

						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0xFF);
						for(i=0;i<12;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0xFF);
						st7586_write(ST_DATA, 0x00);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0xFF);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0x00);

					}//5,6,7,8
					for(j=0;j<4;j++)
					{

						for(i=0;i<20;i++)
							st7586_write(ST_DATA, 0xFF);

						st7586_write(ST_DATA, 0x00);

						for(i=0;i<20;i++)
							st7586_write(ST_DATA, 0xFF);
					}//9,10,11,12
					for(j=0;j<4;j++)
					{

						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0xFF);
						for(i=0;i<12;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0xFF);
						st7586_write(ST_DATA, 0x00);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0xFF);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0x00);

					}//13,14,15,16

					for(j=0;j<4;j++)
					{

						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0xFF);
						for(i=0;i<12;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0xFF);
						st7586_write(ST_DATA, 0x00);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0xFF);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0x00);

					}//17,18,19,20
					//A+
					st7586_write(ST_COMMAND, 0x2A);//set col
										st7586_write(ST_DATA, 0x00);
										st7586_write(ST_DATA, 42);
										st7586_write(ST_DATA, 0x00);
										st7586_write(ST_DATA, 43);

										st7586_write(ST_COMMAND, 0x2B);//set row
										st7586_write(ST_DATA, 0x00);
										st7586_write(ST_DATA, 80);
										st7586_write(ST_DATA, 0x00);
										st7586_write(ST_DATA, 99);
										st7586_write(ST_COMMAND, 0x2C);
										for(i=0;i<40;i++)
										{
											for(j=0;j<2;j++)
												st7586_write(ST_DATA, 0x00);
										}

										st7586_write(ST_COMMAND, 0x2A);//set col
										st7586_write(ST_DATA, 0x00);
										st7586_write(ST_DATA, 0);
										st7586_write(ST_DATA, 0x00);
										st7586_write(ST_DATA, 43);

										st7586_write(ST_COMMAND, 0x2B);//set row
										st7586_write(ST_DATA, 0x00);
										st7586_write(ST_DATA, 100);
										st7586_write(ST_DATA, 0x00);
										st7586_write(ST_DATA, 160);
										st7586_write(ST_COMMAND, 0x2C);
										for(i=0;i<61;i++)
										{
											for(j=0;j<44;j++)
												st7586_write(ST_DATA, 0x00);
										}
					LCD_INIT_DELAY(100);
					//reverse

					st7586_write(ST_COMMAND, 0x2A);//set col
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 0);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 43);

					st7586_write(ST_COMMAND, 0x2B);//set row
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 0);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 59);
					st7586_write(ST_COMMAND, 0x2C);
					for(i=0;i<60;i++)
					{
						for(j=0;j<44;j++)
							st7586_write(ST_DATA, 0xFF);
					}

					st7586_write(ST_COMMAND, 0x2A);//set col
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 0);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 4);

					st7586_write(ST_COMMAND, 0x2B);//set row
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 60);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 69);
					st7586_write(ST_COMMAND, 0x2C);
					for(i=0;i<10;i++)
					{
						for(j=0;j<5;j++)
							st7586_write(ST_DATA, 0xFF);
					}

					st7586_write(ST_COMMAND, 0x2A);//set col
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 38);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 43);

					st7586_write(ST_COMMAND, 0x2B);//set row
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 60);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 69);
					st7586_write(ST_COMMAND, 0x2C);
					for(i=0;i<10;i++)
					{
						for(j=0;j<6;j++)
							st7586_write(ST_DATA, 0xFF);
					}

					st7586_write(ST_COMMAND, 0x2A);//set col
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 5);
							st7586_write(ST_DATA, 0x00);
							st7586_write(ST_DATA, 37);

							st7586_write(ST_COMMAND, 0x2B);//set row
							st7586_write(ST_DATA, 0x00);
							st7586_write(ST_DATA, 60);
							st7586_write(ST_DATA, 0x00);
							st7586_write(ST_DATA, 69);

							st7586_write(ST_COMMAND, 0x2c);
					for(j=0;j<2;j++)
								{
									for(i=0;i<5;i++)
										st7586_write(ST_DATA, 0x00);
									st7586_write(ST_DATA, 0x00);st7586_write(ST_DATA, 0xff);
									st7586_write(ST_DATA, 0x00);
									for(i=0;i<6;i++)
										st7586_write(ST_DATA, 0xff);
									for(i=0;i<5;i++)
										st7586_write(ST_DATA, 0x00);
									for(i=0;i<2;i++)
										st7586_write(ST_DATA, 0xff);
									for(i=0;i<5;i++)
										st7586_write(ST_DATA, 0x00);
									for(i=0;i<2;i++)
										st7586_write(ST_DATA, 0xff);

									st7586_write(ST_DATA, 0x00);
									for(i=0;i<4;i++)
										st7586_write(ST_DATA, 0xff);
								}//1,2

								for(j=0;j<2;j++)
								{

									st7586_write(ST_DATA, 0x00);
									for(i=0;i<6;i++)
										st7586_write(ST_DATA, 0xff);
									st7586_write(ST_DATA, 0x00);
									for(i=0;i<6;i++)
										st7586_write(ST_DATA, 0xff);
									st7586_write(ST_DATA, 0x00);
									for(i=0;i<6;i++)
										st7586_write(ST_DATA, 0xff);

									st7586_write(ST_DATA, 0x00);
									for(i=0;i<3;i++)
										st7586_write(ST_DATA, 0xff);
									st7586_write(ST_DATA, 0x00);

									for(i=0;i<2;i++)
										st7586_write(ST_DATA, 0xff);
									for(i=0;i<5;i++)
										st7586_write(ST_DATA, 0x00);
								}//3,4

								for(j=0;j<2;j++)
								{

									st7586_write(ST_DATA, 0x00);
									for(i=0;i<6;i++)
										st7586_write(ST_DATA, 0xff);
									st7586_write(ST_DATA, 0x00);
									for(i=0;i<6;i++)
										st7586_write(ST_DATA, 0xff);


									for(i=0;i<5;i++)
										st7586_write(ST_DATA, 0x00);
									for(i=0;i<2;i++)
										st7586_write(ST_DATA, 0xff);
									for(i=0;i<5;i++)
										st7586_write(ST_DATA, 0x00);
									for(i=0;i<2;i++)
										st7586_write(ST_DATA, 0xff);
									st7586_write(ST_DATA, 0x00);
									for(i=0;i<4;i++)
										st7586_write(ST_DATA, 0xff);

								}//5,6

								for(j=0;j<2;j++)
								{

									st7586_write(ST_DATA, 0x00);
									for(i=0;i<6;i++)
										st7586_write(ST_DATA, 0xff);
									st7586_write(ST_DATA, 0x00);
									for(i=0;i<6;i++)
										st7586_write(ST_DATA, 0xff);
									st7586_write(ST_DATA, 0x00);
									for(i=0;i<6;i++)
										st7586_write(ST_DATA, 0xff);

									st7586_write(ST_DATA, 0x00);
									for(i=0;i<3;i++)
										st7586_write(ST_DATA, 0xff);
									st7586_write(ST_DATA, 0x00);
									for(i=0;i<2;i++)
										st7586_write(ST_DATA, 0xff);
									st7586_write(ST_DATA, 0x00);
									for(i=0;i<4;i++)
										st7586_write(ST_DATA, 0xff);
								}//7,8

								for(j=0;j<2;j++)
								{

									for(i=0;i<5;i++)
										st7586_write(ST_DATA, 0x00);
									st7586_write(ST_DATA, 0xff);st7586_write(ST_DATA, 0xff);
									for(i=0;i<5;i++)
										st7586_write(ST_DATA, 0x00);
									st7586_write(ST_DATA, 0xff);st7586_write(ST_DATA, 0xff);
									for(i=0;i<5;i++)
										st7586_write(ST_DATA, 0x00);
									st7586_write(ST_DATA, 0xff);st7586_write(ST_DATA, 0xff);

									st7586_write(ST_DATA, 0x00);
									for(i=0;i<3;i++)
										st7586_write(ST_DATA, 0xff);
									st7586_write(ST_DATA, 0x00);
									for(i=0;i<2;i++)
										st7586_write(ST_DATA, 0xff);
									st7586_write(ST_DATA, 0x00);
									for(i=0;i<4;i++)
										st7586_write(ST_DATA, 0xff);
								}//9,10
								//clear reverse

								st7586_write(ST_COMMAND, 0x2A);//set col
								st7586_write(ST_DATA, 0x00);
								st7586_write(ST_DATA, 0);
								st7586_write(ST_DATA, 0x00);
								st7586_write(ST_DATA, 43);

								st7586_write(ST_COMMAND, 0x2B);//set row
								st7586_write(ST_DATA, 0x00);
								st7586_write(ST_DATA, 70);
								st7586_write(ST_DATA, 0x00);
								st7586_write(ST_DATA, 79);
								st7586_write(ST_COMMAND, 0x2C);
								for(i=0;i<10;i++)
								{
									for(j=0;j<44;j++)
										st7586_write(ST_DATA, 0xFF);
								}

								st7586_write(ST_COMMAND, 0x2A);//set col
								st7586_write(ST_DATA, 0x00);
								st7586_write(ST_DATA, 0);
								st7586_write(ST_DATA, 0x00);
								st7586_write(ST_DATA, 0);

								st7586_write(ST_COMMAND, 0x2B);//set row
								st7586_write(ST_DATA, 0x00);
								st7586_write(ST_DATA, 80);
								st7586_write(ST_DATA, 0x00);
								st7586_write(ST_DATA, 99);
								st7586_write(ST_COMMAND, 0x2C);
								for(i=0;i<40;i++)
								{
									for(j=0;j<1;j++)
										st7586_write(ST_DATA, 0xFF);
								}

					st7586_write(ST_COMMAND, 0x2A);//set col
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 1);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 41);

					st7586_write(ST_COMMAND, 0x2B);//set row
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 80);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 99);





					st7586_write(ST_COMMAND, 0x2C);

					for(j=0;j<4;j++)
					{

						for(i=0;i<20;i++)
							st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 0xff);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0xff);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0xff);

					}//1,2,3,4
					for(j=0;j<4;j++)
					{

						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<12;i++)
							st7586_write(ST_DATA, 0xff);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 0xff);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0xff);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0xff);

					}//5,6,7,8
					for(j=0;j<4;j++)
					{

						for(i=0;i<20;i++)
							st7586_write(ST_DATA, 0x00);

						st7586_write(ST_DATA, 0xff);

						for(i=0;i<20;i++)
							st7586_write(ST_DATA, 0x00);
					}//9,10,11,12
					for(j=0;j<4;j++)
					{

						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<12;i++)
							st7586_write(ST_DATA, 0xff);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 0xff);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0xff);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0xff);

					}//13,14,15,16

					for(j=0;j<4;j++)
					{

						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<12;i++)
							st7586_write(ST_DATA, 0xff);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0x00);
						st7586_write(ST_DATA, 0xff);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0xff);
						for(i=0;i<4;i++)
							st7586_write(ST_DATA, 0x00);
						for(i=0;i<8;i++)
							st7586_write(ST_DATA, 0xff);

					}//17,18,19,20
					//A+ reverse

					st7586_write(ST_COMMAND, 0x2A);//set col
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 42);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 43);

					st7586_write(ST_COMMAND, 0x2B);//set row
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 80);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 99);
					st7586_write(ST_COMMAND, 0x2C);
					for(i=0;i<40;i++)
					{
						for(j=0;j<2;j++)
							st7586_write(ST_DATA, 0xFF);
					}

					st7586_write(ST_COMMAND, 0x2A);//set col
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 0);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 43);

					st7586_write(ST_COMMAND, 0x2B);//set row
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 100);
					st7586_write(ST_DATA, 0x00);
					st7586_write(ST_DATA, 160);
					st7586_write(ST_COMMAND, 0x2C);
					for(i=0;i<61;i++)
					{
						for(j=0;j<44;j++)
							st7586_write(ST_DATA, 0xFF);
					}
					LCD_INIT_DELAY(100);
		}
}


void gameover()
{
	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 1);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 40);

	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 60);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 65);

	uint8_t i;
	st7586_write(ST_COMMAND, 0x2C);


	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<6;i++)
		st7586_write(ST_DATA, 0x00);

	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);

	for(i=0;i<4;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<4;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<6;i++)
		st7586_write(ST_DATA, 0x00);

	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);
	//1


	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);

	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);

	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);

	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);

	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);
	//2


	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);

	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);

	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);

	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);


	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);
	//3


	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<6;i++)
		st7586_write(ST_DATA, 0x00);

	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);

	for(i=0;i<4;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<4;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<4;i++)
		st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);
	//4


	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<6;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<8;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<4;i++)
		st7586_write(ST_DATA, 0x00);


	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<6;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);
	//5

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<6;i++)
		st7586_write(ST_DATA, 0x00);

	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);

	for(i=0;i<4;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<4;i++)
		st7586_write(ST_DATA, 0x00);

	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<6;i++)
		st7586_write(ST_DATA, 0x00);


	st7586_write(ST_DATA, 0xFF);

	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);
	//6

	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 19);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 26);

	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 70);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 79);


	st7586_write(ST_COMMAND, 0x2C);


	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);
	for(i=0;i<1;i++)
		st7586_write(ST_DATA, 0x00);
	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0xFF);
	//1
	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);
	for(i=0;i<1;i++)
		st7586_write(ST_DATA, 0x00);
	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0xFF);
	//2

	st7586_write(ST_DATA, 0xFF);
	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0xFF);
	for(i=0;i<1;i++)
		st7586_write(ST_DATA, 0x00);
	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0xFF);
	//3

	st7586_write(ST_DATA, 0xFF);
	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0xFF);
	for(i=0;i<1;i++)
		st7586_write(ST_DATA, 0x00);
	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0xFF);
	//4

	st7586_write(ST_DATA, 0xFF);
	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0xFF);
	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);
	//5

	st7586_write(ST_DATA, 0xFF);
	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0xFF);
	for(i=0;i<1;i++)
		st7586_write(ST_DATA, 0x00);
	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0xFF);
	//6
	st7586_write(ST_DATA, 0xFF);
	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0xFF);
	for(i=0;i<1;i++)
		st7586_write(ST_DATA, 0x00);
	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0xFF);
	//7
	st7586_write(ST_DATA, 0xFF);
	for(i=0;i<3;i++)
		st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0xFF);
	for(i=0;i<1;i++)
		st7586_write(ST_DATA, 0x00);
	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0xFF);
	//8


	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);

	for(i=0;i<1;i++)
		st7586_write(ST_DATA, 0x00);
	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0xFF);
	//9
	for(i=0;i<5;i++)
		st7586_write(ST_DATA, 0xFF);

	for(i=0;i<2;i++)
		st7586_write(ST_DATA, 0x00);

	st7586_write(ST_DATA, 0xFF);
	//10

}
void collision(int life)
{
	if(life ==0)
	{
		//SEGGER_RTT_printf(0,"Game Over\n");
		white();
		gameover();
		nrf_delay_ms(1000000);
	}
	else if(life == 1)
	{
		display_address(34,0);
		Nulll();
		display_address(38,0);
		Nulll();
	}
	else if(life == 2)
	{
		display_address(34,0);
		A();
		display_address(38,0);
		Nulll();
	}
	else if(life >2)
	{
		display_address(34,0);
		A();
		display_address(38,0);
		A();
	}
	//display_address(34,0);
	//A();
	//display_address(38,0);
	//A();
}
void first()//처음 시작화면 UI
{

	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 5);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 37);

	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 10);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 19);

	uint8_t i,j;
	st7586_write(ST_COMMAND,0x2C);

	for(i=0;i<1;i++)
	{
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}//1

	for(i=0;i<3;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}//2,3,4
	for(i=0;i<1;i++)
	{

		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<6;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
	}//5
	for(i=0;i<1;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}//6

	for(i=0;i<1;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<6;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}//7

	for(i=0;i<1;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}//8

	for(i=0;i<1;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<6;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}//9
	for(i=0;i<1;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<6;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
	}//10

	button_address(7,25);
	button();
	display_address(31,25);
	one();

	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 41);

	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 40);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 49);

	st7586_write(ST_COMMAND,0x2C);
	for(i=0;i<1;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<20;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}//1
	for(i=0;i<3;i++)
	{
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<7;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<20;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}//2,3,4
	for(i=0;i<1;i++)
	{

		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<6;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<18;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}//5
	for(i=0;i<1;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		//
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		//
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}

	}//6
	for(i=0;i<3;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<6;j++)
		{
			st7586_write(ST_DATA,0x00);
		}for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		//
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0x00);
		}for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		//
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0x00);
		}for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}//7,8,9
	for(i=0;i<1;i++)
	{

		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}for(j=0;j<3;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}for(j=0;j<5;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<4;j++)
		{
			st7586_write(ST_DATA,0x00);
		}
		//
		for(j=0;j<6;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		//
		for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0x00);
		}for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<8;j++)
		{
			st7586_write(ST_DATA,0x00);
		}for(j=0;j<1;j++)
		{
			st7586_write(ST_DATA,0xFF);
		}
		for(j=0;j<2;j++)
		{
			st7586_write(ST_DATA,0x00);
		}

	}//10


}
void bombeffect(int height)
{

	uint8_t i,j,y;
	y=height;

	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 43);

	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y+7);

	st7586_write(ST_COMMAND, 0x2C);

	for(i=0;i<4;i++)
	{
		for(j=0;j<11;j++)
		{
			st7586_write(ST_DATA, 0xFF);
			st7586_write(ST_DATA, 0xFF);
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 0x00);

		}

	}

	for(i=0;i<4;i++)
	{
		for(j=0;j<11;j++)
		{
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 0xFF);
			st7586_write(ST_DATA, 0xFF);


		}

	}


	LCD_INIT_DELAY(10);
	st7586_write(ST_COMMAND, 0x2A);//set col
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 0);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, 43);

	st7586_write(ST_COMMAND, 0x2B);//set row
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y);
	st7586_write(ST_DATA, 0x00);
	st7586_write(ST_DATA, y+7);

	st7586_write(ST_COMMAND, 0x2C);
	for(i=0;i<4;i++)
	{
		for(j=0;j<11;j++)
		{
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 0xFF);
			st7586_write(ST_DATA, 0xFF);


		}

	}

	for(i=0;i<4;i++)
	{
		for(j=0;j<11;j++)
		{
			st7586_write(ST_DATA, 0xFF);
			st7586_write(ST_DATA, 0xFF);
			st7586_write(ST_DATA, 0x00);
			st7586_write(ST_DATA, 0x00);



		}

	}

	LCD_INIT_DELAY(10);
	for(int k=0;k<30;k++)
	   {
	      if(f[k][2]<y+1&&f[k][2]>13)
	      {

	         f[k][0]=f[k][1]=f[k][2]=-1;//이펙트와 알파벳이 닿으면 초기화

	         switch(level)//레벨에 따라 점수 더하기
	         {
	         case 1:
	         {
	        	levelscore++;
	            score1++;

	            break;
	         }
	         case 2:
	         {
	        	 levelscore++;
	            score1+=2;

	                        break;
	         }
	         case 3:
	         {
	        	 levelscore++;
	            score1+=3;

	            break;
	         }
	         case 4:
	         {
	        	 levelscore++;
	            score1+=4;

	            break;
	         }
	         case 5:
	         {
	        	 levelscore++;
	            score1+=5;

	            break;
	         }
	         }
	      }

	   }
	for(i=0;i<8;i++)
	{
		for(j=0;j<44;j++)
		{
			st7586_write(ST_DATA, 0x00);
		}
	}






}
int main(void)
{

	APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
	NRF_LOG_FLUSH();

	// Request LF clock.
	lfclk_request();

	// Configure GPIO's.
	gpio_config();

	// Initialize the Application timer Library.
	//changed in SDK v 13
	app_timer_init();
	uint32_t err_code = app_timer_init();
	lcd_init();
	//init();
	err_code = NRF_LOG_INIT(NULL);
	APP_ERROR_CHECK(err_code);

	err_code = nrf_drv_rng_init(NULL);
	APP_ERROR_CHECK(err_code);
	for(int i = 0; i<30 ; i++)//변수 초기화
	{
		check_wall[i] = 1;
		f[i][0] = -1;
		f[i][1] = -1;
		f[i][2] = -1;
	}
	first();//처음시작화면 UI
	button_address(0,100);
	button1();//처음시작화면 UI
	button_address(0,115);
	button2();//처음시작화면 UI
	button_address(0,130);
	button3();//처음시작화면 UI
	button_address(0,145);
	button4();//처음시작화면 UI
	__WFI();//버튼 입력을 기다렸다 입력되면 다음으로
	if (err_code != NRF_SUCCESS) {
		//SEGGER_RTT_printf(0,"app timer initial error\n");
		NRF_LOG_INFO("*Error initing app timer*\r\n");
		NRF_LOG_FLUSH();
		//return;
	}else {
		white();
		// Create application timer instances.
		create_timers();//timer intialize 성공하면 timer create
		nrf_delay_ms(1000);
		score();//score UI 'SCORE'
		scorenum(0);//초기 score 값 0

		display_address(34,0);//처음 목숨 UI A로 설정
		A();
		display_address(38,0);//A 1개
		Nulll();



		displayman(man_x,man_y);//초기 사람의 x y 설정
		man();//사람 표시

		while(1)//본격적인 게임 시작
		{
			if(score1==450)//최고점수 450점 달성시 게임종료와 함께 종료화면 출력
						{

							gameclear();
						}

			if(paus==1)//일지 정지중일 때는 캐릭터가 일시정지모양으로 출력
			{
				displayman(man_x,man_y);
				manwhite();
				manpause();
				while(1)
				{

					if(paus==0)
					{
						displayman(man_x,man_y);
						manwhite();
						man();
						break;
					}
					app_sched_execute();//버튼 이벤트를 받기 위해(즉, 게임을 재개하기 위해)
				}
			}
			if(bb==1 && bombnum>0)//폭탄을 썼을 때
					{

				bombeffect(height);//UI : height에 따라 위로 올라가는 effect
				height-=6;
				if(height<13)//끝까지오면 다시 147로 해준다 effect는 끈다
						{
					bb=0;
					height=147;
					bombnum--;
					scorenum(score1);
					if(levelscore%30 == 0)//지웠는데 레벨 스코어가 30 이면 레벨을 올린다.
								{

									i=0;
									level++;
								}
						}
					}
			if(i<30 && (temp%20 ==0))//만드는 속도: roll에서 temp++ 을 해주는데 delay 속도 계산해서 makeF해준다
			{
				SEGGER_RTT_printf(0,"makeF\n");
				makeF(i,13);//f[i][1] = ran f[i][2] = 13 , display(ran,13)
				i++;

			}

			if(level>5)//종료조건
				break;
			if(temp%50 == 0)//temp 속도에 맞춰서 smoglevel 을 설정해주는데 0또는 1이다 1이면 발생 0이면 아니다
			{
				random_vector_generate(p_buff,RANDOM_BUFF_SIZE);
				smoglevel = p_buff[0]%2 ;
			}

			if(smoglevel == 1)//smoglevel = 1 이면 smog 발생하고 level을 2로 한다
			{
				smog();
				smoglevel = 2;
			}else if(smoglevel == 0){// 0이면 지우고 3으로한다
				clean();
				smoglevel = 3;
			}

			if(smoglevel == 1 || smoglevel == 2 || smoglevel == 4) //1이거나 2이거나 4이거나 즉 smog 상태면 smogtrue =1 이다
			{
				smogtrue = 1;
			}else{//아니면 0
				smogtrue = 0;
			}


			roll();//기본 roll 함수


			if(smoglevel == 2)//2이면 스모그 위에 부분에 알파벳 잔상을 없애기 위해 크기만큼 white로 칠해준다
			{
				uint8_t i,j;
				st7586_write(ST_COMMAND, 0x2A);//set col
				st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0);
				st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 43);

				st7586_write(ST_COMMAND, 0x2B);//set row
				st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 58);
				st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 69);
				st7586_write(ST_COMMAND,0x2C);

				for(i=0;i<12;i++)
				{

					for(j=0;j<44;j++)
					{
						st7586_write(ST_DATA,0x00);
					}

				}
				st7586_write(ST_COMMAND, 0x2A);//set col
				st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 0);
				st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 43);

				st7586_write(ST_COMMAND, 0x2B);//set row
				st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 111);
				st7586_write(ST_DATA, 0x00);
				st7586_write(ST_DATA, 122);
				st7586_write(ST_COMMAND,0x2C);

				for(i=0;i<12;i++)
				{

					for(j=0;j<44;j++)
					{
						st7586_write(ST_DATA,0x00);
					}

				}
				smoglevel =4 ;
			}

			app_sched_execute();
			//__WFI();
		}
	}

}


