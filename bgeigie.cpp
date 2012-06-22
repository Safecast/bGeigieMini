
#include <bgeigie.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <SD.h>
#include <sd_reader_int.h>

// main pin setup routine
void bg_pins_setup()
{
  // set up all power switches
  pinMode(sense_pwr, OUTPUT);
  bg_sensors_off();

  pinMode(gps_on_off, OUTPUT);
  bg_gps_off();

  pinMode(sd_pwr, OUTPUT);
  bg_sd_off();

  pinMode(hvps_pwr, OUTPUT);
  bg_hvps_off();

  // set up LED
  pinMode(led, OUTPUT);
  bg_led_off();

  // set USB irq
  pinMode(32u4_irq, INPUT);

  // set all CS pins to unselected (HIGH)
  pinMode(cs_radio, OUTPUT);
  digitalWrite(cs_radio, HIGH);

  pinMode(cs_32u4, OUTPUT);
  digitalWrite(cs_32u4, HIGH);

  pinMode(cs_sd, OUTPUT);
  digitalWrite(cs_sd, HIGH);

  // set up SD card detect
  pinMode(sd_detect, INPUT_PULLUP);

  // setup Geiger counts input
  pinMode(counts, INPUT);
}

// main switch interrupt servic routine
void isr_main_switch()
{
  if (state == BG_STATE_PWR_DOWN)
    bg_powerup();
  else
    bg_shutdown();
}

// Configure main switch
void bg_main_switch_config()
{
  // setup main switch interrupt
  pinMode(main_switch, INPUT); //This is the main button
  // setup interrupt routine
  atachInterrupt(BG_MAIN_SWITCH_INTERRUPT, bg_isr_main_switch, RISING); 
}

// Power Up
void bg_powerup()
{

}

// Shutdown
void bg_shutdown()
{
  //To reduce power, setup all pins as inputs with no pullups
  for(int x = 0 ; x < 31 ; x++){
    pinMode(x, INPUT);
    digitalWrite(x, LOW);
  }

  // setup main switch
  bg_main_switch_config();

  //Shut off ADC, TWI, SPI, Timer0, Timer1, Timer2

  ADCSRA &= ~(1<<ADEN); //Disable ADC
  ACSR = (1<<ACD); //Disable the analog comparator
  DIDR0 = 0x3F; //Disable digital input buffers on all ADC0-ADC5 pins
  DIDR1 = (1<<AIN1D)|(1<<AIN0D); //Disable digital input buffer on AIN1/0

  power_twi_disable();
  power_spi_disable();
  power_usart0_disable();
  power_timer0_disable();
  power_timer1_disable();
  power_timer2_disable();

  // update state variable
  state = BG_STATE_PWR_DOWN;

  //Power down various bits of hardware to lower power usage  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
}

// configures sensors
void bg_sensors_config()
{
  // set ADC reference
  analogReference(INTERNAL2V56);

  // set sensors switch pin
  pinMode(sensor_pwr, INPUT);
  sensors_off();
}

// read battery voltage in Volts
// uses a voltage divider with R1 = 100MOhm and R2 = 120MOhm
float bg_read_battery()
{
  int bat = analogRead(batt_sense);
  return bat/1023. * VREF / 120. * 220.;
}

// read voltage from HVPS
float bg_read_hvps()
{
  int hvps = analogRead(hv_sense);
  return bat/1023. * VREF;
}

// read temperature (LM61, SOT23)
float bg_read_temperature()
{
  int tmp = analogRead(temp_sense);
  return (tmp/1023. * VREF * 1000. - 424)/6.25;
}

// read humidity sensor (HIH-5030)
float bg_read_humidity()
{
  // need temperature to compensate sensor reading
  float temp = bg_read_temperature();

  // warning, 2.5 internal reference might be just not sufficient
  // to cover the whole sensor output range (up to about 95% at 25
  // degree). If full range is needed, switch to 3.3V reference.
  int hum = analogRead(hum_sense);
  float hum_stat = ((hum/1023.*VREF)/3.3 - 0.1515)/0.00636;

  // compensate for temperature
  hum_stat = hum_stat/(1.0546 - 0.00216*temp);

  return hum_stat;
}

// Initialize SD card
int bg_sd_card_init()
{
  char tmp[BG_ERR_MSG_SIZE];

  bg_sd_initialized = 0;
  bg_sd_inserted = 0;
  bg_sd_last_write = 0; // because that's where SD is turned on

  // check if Card is inserted
  if (!bg_sd_card_present())
  {
    strcpy_P(tmp, msg_card_failure);
    Serial.print(tmp);
    return 0;
  }

  // set card to inserted
  bg_sd_inserted = 1;

  // turn on SD card
  bg_sd_on();

  // see if the card can be initialized:
  if (!SD.begin(cs_sd)) {
    strcpy_P(tmp, msg_card_failure);
    Serial.print(tmp);
    return 0;
  }

  // set card as initialized
  bg_sd_initialized = 1;

  // output success
  strcpy_P(tmp, msg_card_ready);
  Serial.print(tmp);

  // return success
  return 1;
}

// write a line to a file in SD card
int bg_sd_writeln(char *filename, char *log_line)
{
  File dataFile;

  // if SD reader mode is not idle, fail
  if (sd_reader_state != SD_READER_IDLE)
  {
    bg_sd_last_write = 0;
    return 0;
  }

  // start critical bit
  uint8_t sreg_old = SREG;  // save sreg
  cli();                    // disable global interrupts

  // open file
  dataFile = SD.open(filename, FILE_WRITE);
  if (dataFile)
  {
    dataFile.print(log_line);
    dataFile.print("\n");
    dataFile.close();
    bg_sd_last_write = 1;
  }
  else
  {
    char tmp[BG_ERR_MSG_SIZE];
    strcpy_P(tmp, msg_error_log_file_cannot_open);
    Serial.print(tmp);
    bg_sd_last_write = 0;
  }   

  // re-enable interrupts
  SREG = sreg_old;

  return bg_sd_last_write;
}

