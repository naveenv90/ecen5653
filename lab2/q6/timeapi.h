#ifndef _TIMEAPI_H_
#define _TIMEAPI_H_

// Relative time definitions
#define MILLISECS_IN_SECS           1000
#define MICROSECS_IN_MILLISECS      1000
#define NANOSECS_IN_MILLISECS       1000000


// ATSC EPOCH is Jan 6, 1980 @ midnight, UNIX EPOCH is Jan 1, 1970 @ midnight
#define UNIX_TO_ATSC_EPOCH_SECONDS	315964800 
#define UNIX_TO_ATSC_EPOCH_DAYS		3657
#define UNIX_YEAR_BASE              1900
#define ATSC_EPOCH_BASE_YEAR        1980
#define UNIX_EPOCH_BASE_YEAR        1970

// Standard definitions useful for time calculations
#define SECONDS_IN_DAY			86400
#define SECONDS_IN_HOUR			3600
#define SECONDS_IN_MINUTE		60
#define DAYS_IN_NON_LEAP_YEAR   365
#define DAYS_IN_LEAP_YEAR       366
#define MONTHS_IN_YEAR          12
#define SECONDS_IN_NON_LEAP_YEAR (SECONDS_IN_DAY*DAYS_IN_NON_LEAP_YEAR)


// API functions used for DVTIDE time and date information and SI data
unsigned int secondsSinceUnixEpoch(unsigned int year, unsigned int day, unsigned int seconds);
unsigned int secondsSinceATSCEpoch(unsigned int year, unsigned int day, unsigned int seconds);
void printDateFromATSCEpoch(unsigned int epoch_seconds);
void printDateFromUnixEpoch(unsigned int epoch_seconds);
unsigned int getSecondsLocalATSC(void);
unsigned int getSecondsLocalUnix(void);
void printSystemTime(void);

#endif
