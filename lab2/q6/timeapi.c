#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "timeapi.h"

/**
 * secondsSinceUnixEpoch returns UTC seconds since Jan 1, 1970 for a current Gregorian date.
 *
 * I'm not sure this function is exactly right, but it is defined in POSIX:2001, so seems like
 * it should be :).
 *
 * Some other references for UTC time intervals that may be helpful are:
 *
 * 1) http://www.iers.org/
 * 2) http://tycho.usno.navy.mil/systime.html
 * 3) POSIX:2001 online at http://www.opengroup.org/onlinepubs/009695399/toc.htm
 *
 * @param year (Gregorian in range 0 to Current Year).
 * @param day (Calendar days since Jan 1 at midnight of given year).
 * @param seconds (Seconds elapsed since midnight of given day).
 * 
 * @return 32-bit seconds of difference (good for about 136
 *         years).
 */
unsigned int secondsSinceUnixEpoch(unsigned int year, unsigned int day, unsigned int seconds)
{
    unsigned int dt;

    dt = seconds +                                  // elapsed seconds
        (day*SECONDS_IN_DAY) +                      // seconds elapsed since start of year
        ((year-1970)*SECONDS_IN_NON_LEAP_YEAR) +    // seconds since base date of Jan 1, 1970 at midnight
        ((year-1969)/4)*SECONDS_IN_DAY -            // leap years are divisible by 4 and add one day
        ((year-1901)/100)*SECONDS_IN_DAY +          // century years are divisble by 4, but are not leap years 
        ((year-1900+199)/400)*SECONDS_IN_DAY;       // every 4th century year is a leap year

    return dt;
}

// Non leap year - add one to index=1 (February) on leap years
unsigned short daysInMonth[12]={31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void printDateFromEpoch(unsigned int epoch_seconds, unsigned int base_year)
{
    unsigned int days, hours, minutes, seconds, year, month;

#if 0
    unsigned int local_epoch_seconds;
    struct tm *dateTimePtr, *localDateTimePtr;
    struct tm dateTime, localDateTime;

    // Make sure timezone and DST globals are up to date
    tzset();

    local_epoch_seconds=epoch_seconds-timezone;  // timezone is always seconds West, so subtract

    gmtime_r((time_t *)&epoch_seconds, &dateTime);
    dateTimePtr=&dateTime;

    localtime_r((time_t *)&epoch_seconds, &localDateTime);
    localDateTimePtr=&localDateTime;

    days=localDateTimePtr->tm_mday;
    hours=localDateTimePtr->tm_hour;
    minutes=localDateTimePtr->tm_min;
    seconds=localDateTimePtr->tm_sec;
    month=localDateTimePtr->tm_mon;

#else
    int i;
    unsigned int yearIdx;


    days = epoch_seconds / SECONDS_IN_DAY;
    hours = (epoch_seconds - (days*SECONDS_IN_DAY)) / SECONDS_IN_HOUR;
    minutes = (epoch_seconds - (days*SECONDS_IN_DAY) - (hours*SECONDS_IN_HOUR)) / SECONDS_IN_MINUTE;
    seconds = (epoch_seconds - (days*SECONDS_IN_DAY) - (hours*SECONDS_IN_HOUR) - (minutes*SECONDS_IN_MINUTE));

    printf("days=%d, hours=%d, minutes=%d, sec=%d\n", days, hours, minutes, seconds);

    year=base_year;

    while(days > DAYS_IN_NON_LEAP_YEAR)
    {
        // Leap year conditions - every 4th year, but not on century year unless century year is divisible by 400
        if((((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0))
        {
            yearIdx=((days / DAYS_IN_LEAP_YEAR)>0);

            if(yearIdx > 0)
            {
                year+=yearIdx;
                days=days-DAYS_IN_LEAP_YEAR;
            }
            else
                break;
        }

        // Non leap year
        else
        {
            yearIdx=((days / DAYS_IN_NON_LEAP_YEAR)>0);
            if(yearIdx > 0)
            {
                year+=yearIdx;
                days=days-DAYS_IN_NON_LEAP_YEAR;
            }
            else
                break;
        }
    }

    month=1;

    for(i=0;i<MONTHS_IN_YEAR;i++)
    {
        if(days < daysInMonth[i])
            break;
        else
        {
            month++;
            days=days-daysInMonth[i];
        }
    }
#endif

    printf("DATE=%02d-%02d-%04d, %02d:%02d:%02d (MM-DD-YYYY, HH:MM:SS)\n", month, days, year, hours, minutes, seconds);
}

void printDateFromATSCEpoch(unsigned int epoch_seconds)
{
    printDateFromEpoch((epoch_seconds+(6*SECONDS_IN_DAY)), ATSC_EPOCH_BASE_YEAR);
}

void printDateFromUnixEpoch(unsigned int epoch_seconds)
{
    printDateFromEpoch(epoch_seconds, UNIX_EPOCH_BASE_YEAR);
}


unsigned int secondsSinceATSCEpoch(unsigned int year, unsigned int day, unsigned int seconds)
{
    unsigned int dt;

    dt=secondsSinceUnixEpoch(year, day, seconds) - secondsSinceUnixEpoch(1980, 5, 0);

    return dt;
}



/**
 * printSystemTime prints Linux epoch seconds, normal Gregorian
 * date and time, and adjusted ATSC epoch seconds elapsed.
 *
 */

extern char *tzname[2];
extern long timezone;
// Don't use daylight extern, it is bogus according to the man page
//extern int daylight;

unsigned int getSecondsLocalATSC(void)
{
    unsigned int utc_seconds_since_epoch;

    // Make sure timezone and DST globals are up to date
    tzset();

    utc_seconds_since_epoch=((unsigned int)time((time_t *)0));

    // timezone is always seconds West, so subtract
    return(utc_seconds_since_epoch-timezone-UNIX_TO_ATSC_EPOCH_SECONDS);
}

unsigned int getSecondsLocalUnix(void)
{
    unsigned int utc_seconds_since_epoch;

    // Make sure timezone and DST globals are up to date
    tzset();

    utc_seconds_since_epoch=((unsigned int)time((time_t *)0));

    // timezone is always seconds West, so subtract
    return(utc_seconds_since_epoch-timezone);
}

void printSystemTime(void)
{
    unsigned int system_time;
    unsigned int seconds=0;
    time_t utc_seconds_since_epoch, local_seconds_since_epoch;
    struct tm *dateTimePtr, *localDateTimePtr;
    struct tm dateTime, localDateTime;

    // Make sure timezone and DST globals are up to date
    tzset();

    utc_seconds_since_epoch=((unsigned int)time((time_t *)0));
    local_seconds_since_epoch=utc_seconds_since_epoch-timezone;  // timezone is always seconds West, so subtract

    gmtime_r(&utc_seconds_since_epoch, &dateTime);
    dateTimePtr=&dateTime;

    localtime_r(&utc_seconds_since_epoch, &localDateTime);
    localDateTimePtr=&localDateTime;

    seconds = ((dateTimePtr->tm_hour)*SECONDS_IN_HOUR) + ((dateTimePtr->tm_min)*SECONDS_IN_MINUTE) + dateTimePtr->tm_sec;
    //system_time=secondsSinceUnixEpoch(((dateTimePtr->tm_year)+UNIX_YEAR_BASE), dateTimePtr->tm_yday, seconds);

    system_time=secondsSinceATSCEpoch(((dateTimePtr->tm_year)+ATSC_EPOCH_BASE_YEAR), dateTimePtr->tm_yday, seconds);

    printf("utc secs=%u (0x%x), local secs=%u (0x%x), UTC:%02d/%02d/%02d, %02d:%02d:%02d, LOCAL:%02d/%02d/%02d, %02d:%02d:%02d TZ=%s-%s, adj secs=%u\n", 
           (unsigned int)utc_seconds_since_epoch, 
           (unsigned int)utc_seconds_since_epoch, 
           (unsigned int)local_seconds_since_epoch,
           (unsigned int)local_seconds_since_epoch,
           dateTimePtr->tm_mon+1, dateTimePtr->tm_mday, UNIX_YEAR_BASE+dateTimePtr->tm_year,
           dateTimePtr->tm_hour, dateTimePtr->tm_min, dateTimePtr->tm_sec,
           localDateTimePtr->tm_mon+1, localDateTimePtr->tm_mday, UNIX_YEAR_BASE+localDateTimePtr->tm_year,
           localDateTimePtr->tm_hour, localDateTimePtr->tm_min, localDateTimePtr->tm_sec, tzname[0], tzname[1], system_time);

    if(localDateTimePtr->tm_isdst > 0)
        printf("Daylight savings time in affect locally\n");
    else if(localDateTimePtr->tm_isdst == 0)
        printf("Daylight savings time NOT in affect locally\n");
    else if(localDateTimePtr->tm_isdst < 0)
        printf("Daylight savings time status not known locally\n");

    // Don't use daylight extern, it is bogus according to the man page
    //if(daylight) printf("Daylight savings time used in local TZ\n");

    printf("timezone=%ld seconds west of GMT, hours=%ld, min=%ld, sec=%ld\n",
           timezone, (timezone/SECONDS_IN_HOUR), ((timezone%SECONDS_IN_HOUR)/SECONDS_IN_MINUTE),
           (timezone%SECONDS_IN_HOUR));

    // Verify offset between Unix epoch and ATSC epoch
    system_time=secondsSinceUnixEpoch(1980, 5, 0);

    printf("UNIX to ATSC EPOCH: adjusted seconds=%u, adjusted days=%u (should be 315964800 seconds, 3657 days)\n",
           system_time, (system_time/SECONDS_IN_DAY));
}
