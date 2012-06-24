
#include "bg_pwr.h"

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <sd_reader_int.h>

// power switch pin
static int bg_pwr_switch_pin;
// interrupt number
static int bg_pwr_interrupt;

// function pointers for power up and down custom routines
void (*bg_pwr_on_wakeup)() = NULL;
void (*bg_pwr_on_sleep)() = NULL;

// main switch interrupt servic routine
void bg_pwr_isr()
{
  if (bg_pwr_state == BG_STATE_PWR_DOWN)
  {
    power_timer0_enable();  // for Arduino delay, millis, etc
    unsigned int start = millis();
    while ( (millis() - start < 2000) && (digitalRead(bg_pwr_switch_pin) == 1) );
    if (millis() - start >= 2000)
      bg_pwr_up();
    else
      bg_pwr_down();
  }
  else
  {
    bg_pwr_down();
  }
}

// Configure main switch
void bg_pwr_init(int pin_switch, int interrupt, void (*on_wakeup)(), void (*on_sleep)())
{
  // set pins
  bg_pwr_switch_pin = pin_switch;
  bg_pwr_interrupt = interrupt;

  // set up power up routines
  if (on_wakeup != NULL)
    bg_pwr_on_wakeup = on_wakeup;
  else
    bg_pwr_on_wakeup = NULL;

  // set up power down routine
  if (on_sleep != NULL)
    bg_pwr_on_sleep = on_sleep;
  else
    bg_pwr_on_sleep = NULL;
}

void bg_pwr_setup_switch_pin()
{
  // setup main switch interrupt
  pinMode(main_switch, INPUT);
  // setup interrupt routine
  //attachInterrupt(BG_MAIN_SWITCH_INTERRUPT, bg_pwr_isr, RISING); 
  attachInterrupt(bg_pwr_interrupt, bg_pwr_isr, RISING); 
}

// Power Up
void bg_pwr_up()
{
  //Shut off ADC, TWI, SPI, Timer0, Timer1, Timer2
  ADCSRA |= (1<<ADEN); // Enable ADC
  ACSR &= ~(1<<ACD);   // Enable the analog comparator

  // this should be set to reflect real usage of analog pins
  DIDR0 = 0x00;   // Enable digital input buffers on all ADC0-ADC5 pins
  DIDR1 &= ~(1<<AIN1D)|(1<<AIN0D); // Enable digital input buffer on AIN1/0

  power_twi_enable();
  power_spi_enable();
  power_usart0_enable();
  power_timer0_enable();
  power_timer1_enable();
  power_timer2_enable();
  power_timer3_enable();

  // update state variable
  bg_pwr_state = BG_STATE_PWR_UP;

  // execute wake up routine
  if (bg_pwr_on_wakeup != NULL)
    bg_pwr_on_wakeup();
}

// Shutdown
void bg_pwr_down()
{
  // execute sleep routine
  if (bg_pwr_on_sleep != NULL)
    bg_pwr_on_sleep();

  // setup main switch interrupt
  bg_pwr_setup_switch_pin();

  //Shut off ADC, TWI, SPI, Timer0, Timer1, Timer2

  ADCSRA &= ~(1<<ADEN); //Disable ADC
  ACSR |= (1<<ACD); //Disable the analog comparator
  DIDR0 = 0xFF; //Disable digital input buffers on all ADC0-ADC5 pins
  DIDR1 = (1<<AIN1D)|(1<<AIN0D); //Disable digital input buffer on AIN1/0

  power_twi_disable();
  power_spi_disable();
  power_usart0_disable();
  power_timer0_disable();
  power_timer1_disable();
  power_timer2_disable();
  power_timer3_disable();

  // update state variable
  bg_pwr_state = BG_STATE_PWR_DOWN;

  //Power down various bits of hardware to lower power usage  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
}


