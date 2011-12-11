/*
 * Read CPM from a Geiger counter
 * by FakuFaku for SafeCast <http://www.safecast.org>
 */

#include "GeigerInterface.h"
#include <EEPROM.h>
 
#define WIN_LEN    60000       // # of milliseconds in one minute
#define NX 12                  // number of averaging bins in moving window
#define MAX_SENTENCE_SIZE 64   // max size of sentence sent through serial
#define AVAILABLE 'A'          // indicates geiger data are ready (available)
#define VOID      'V'          // indicates geiger data not ready (void)
#define BGRDD_EEPROM_ID 100
#define BGRDD_ID_LEN 3

// device id number
char dev_id[BGRDD_ID_LEN+1];

// Serial message header
char hdr[6] = "BGRDD";
char sentence[MAX_SENTENCE_SIZE];
 
// shift register
unsigned long shift_reg[NX];
unsigned long reg_index = 0;

// Total count counter
unsigned long totalCounts = 0;

// keep track of geiger readings quality
char geiger_status = VOID;
int str_count = 0;
 
unsigned long cpm()
{
  int i;
  unsigned long c_p_m = 0;
  // sum up
  for (i=0 ; i < NX ; i++)
    c_p_m += shift_reg[i];

  return c_p_m;
}

void count_pulse()
{
  shift_reg[reg_index]++;
}

void setup()
{
  int i;

  // open the serial port at 9600 bps:
  Serial.begin(9600);

  // initialize
  for (i=0 ; i < NX ; i++)
    shift_reg[i] = 0;
  reg_index = 0;
  totalCounts = 0;

  // pull bGeigie serial id
  pullDevId();
  Serial.print("Device id: ");
  Serial.println(dev_id);

  // set initial state of Geiger to void
  geiger_status = VOID;
  str_count = 0;

  // Set an interrupt on rising signals
  attachInterrupt(1, count_pulse, RISING);
}

void loop() 
{
  unsigned long c_p_m, c_p_b;
  char chk;


  // we delay by the duration of one bin
  delay(WIN_LEN/NX);

  noInterrupts();                 // XXX stop interrupts here
  c_p_m = cpm();                  // compute one minute and last bin counts
  c_p_b = shift_reg[reg_index];   // get current bin count
  reg_index = (reg_index+1) % NX; // increment register index
  shift_reg[reg_index] = 0;       // reset new register
  interrupts();                   // XXX Resume interrupts here

  // update total counter
  totalCounts += c_p_b;

  // set status of Geiger
  if (str_count < NX)
  {
    geiger_status = VOID;
    str_count++;
  } else if (c_p_m == 0) {
    geiger_status = VOID;
  } else {
    geiger_status = AVAILABLE;
  }

  // Create sentence and send it through serial link
  chk = createSentence(c_p_b, c_p_m, totalCounts);

  // Send serial data
  Serial.print(sentence);
  Serial.print('*');
  if (chk < 16)           // make sure the checksum has two digits
    Serial.print('0');
  Serial.println(chk, HEX);

}

/* create Sentence to send through serial port */
/* store the sentence in global variable. return the checksum */
char createSentence(unsigned long cpb, unsigned long cpm, unsigned long total)
{
  int i;
  int off = 0;
  int chk;

  // start with '$'  -- 1char, tot=1
  sentence[0] = '$';
  off = 1;

  // header   -- 5char, tot=6
  i = 0;
  while (hdr[i] != NULL)
    sentence[i+off] = hdr[i++];
  off += i;

  // add comma  -- 1char, tot=7
  sentence[off] = ',';
  off++;

  // add device id
  for (i=0 ; i < BGRDD_ID_LEN ; i++)
    sentence[off+i] = dev_id[i];
  off += BGRDD_ID_LEN;

  // add comma  -- 1char, tot=7
  sentence[off] = ',';
  off++;

  // add CPB (counts in last bin) -- max 10char, tot=17
  off += ultoa(cpb, sentence + off);
  sentence[off] = NULL;

  // add comma  -- 1char, tot=18
  sentence[off] = ',';
  off++;

  // add CPM (last minute)  -- max 10char, tot=28
  off += ultoa(cpm, sentence + off);
  sentence[off] = NULL;

  // add comma  -- 1char, tot=29
  sentence[off] = ',';
  off++;

  // add Total Count (counts since start of Arduino)  -- max 10char, tot=39
  off += ultoa(total, sentence + off);
  sentence[off] = NULL;

  // add comma  -- 1char, tot=40
  sentence[off] = ',';
  off++;

  // add status -- 1char, tot=41
  sentence[off] = geiger_status;
  off++;

  // terminate with NULL -- 1char, tot=42
  sentence[off] = NULL;

  // compute check sum now (not including leading '$')
  chk = checksum(sentence+1, off);

  // return number of characters
  return chk;

}

/* compute check sum of N bytes in array s */
char checksum(char *s, int N)
{
  int i = 0;
  char chk = s[0];

  for (i=1 ; i < N ; i++)
    chk ^= s[i];

  return chk;
}

/* convert an unsigned long value to a NULL terminated string */
/* the result is stored in char array a */
/* returns the length of the string created */
int ultoa(unsigned long n, char *a)
{
  int size = 0;
  int i = 0;

  // handle 0 case
  if (n == 0)
  {
    a[0] = '0';
    a[1] = NULL;
    return 1;
  }

  // reads the digits, but backwards
  while (n != 0)
  {
    a[size++] = 48 + char(n % 10); // store left-most digit
    n /= 10;            // euclidian division
  }

  // NULL terminate string
  a[size] = NULL;

  // now we have to flip the string
  for (i=0 ; i < size/2 ; i++)
  {
    // swap values
    char t = a[i];
    a[i] = a[size-1 - i];
    a[size-1 - i] = t;
  }

  // return size of string
  return size;

}

void pullDevId()
{
  for (int i=0 ; i < BGRDD_ID_LEN ; i++)
  {
    dev_id[i] = (char)EEPROM.read(i+BGRDD_EEPROM_ID);
    if (dev_id[i] < '0' || dev_id[i] > '9')
    {
      dev_id[i] = '0';
      EEPROM.write(i+BGRDD_EEPROM_ID, dev_id[i]);
    }
  }
  dev_id[BGRDD_ID_LEN] = NULL;
}










