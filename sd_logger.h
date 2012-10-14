
#ifndef __SD_LOGGER_H__
#define __SD_LOGGER_H__

/* macros to turn on and off SD card */
extern int sd_log_pwr;  // pwr pin
#define sd_log_pwr_on() digitalWrite(sd_log_pwr, LOW)
#define sd_log_pwr_off() digitalWrite(sd_log_pwr, HIGH)

// check if SD card is inserted
extern int sd_log_detect;
#define sd_log_card_missing() (digitalRead(sd_log_detect))

// keep track of status
extern int sd_log_initialized;
extern int sd_log_inserted;
extern int sd_log_last_write;

/* initialize SD card */
int sd_log_init(int pin_pwr, int pin_detect, int pin_cs);

/* Run SD card diagnostic */
void sd_log_card_diagnostic();

/* write a log line to specified file */
int sd_log_writeln(char *filename, char *log_line);

#endif /* __SD_LOGGER_H__ */

