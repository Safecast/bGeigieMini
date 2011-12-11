/*
 * ** Header file **
 * Read CPM from a Geiger counter
 * by FakuFaku for SafeCast <http://www.safecast.org>
 */
 
/* compute the pulse counts on last 60 seconds */
unsigned long cpm();

/* called on interrupt. increment pulse counter */
void count_pulse();

/* create serial sentence to send */
/* cpb: counts per bin, cpm: counts per minute, total: total count since start */
char createSentence(unsigned long cpb, unsigned long cpm, unsigned long total);

/* compute checksum on serial message of N bytes */
char checksum(char *s, int N);

/* convert integer to character array */
int ultoa(int n, char *a);
