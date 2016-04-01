// The Timezone library is modified by nikant to remove eeprom access
// commented out sections:
// file: Timezone.cpp
// line 12 #include eeprom
// line 28-31, whole of Timezone::Timezone function
// line 184-189, whole of Timezone::readRules function
// line 196-201, whole of Timezone::writeRules function
// file: Timezone.h
// lines: 42, 48, 49

// You can edit NKNTP.h in order to change to your Timezone and DST
// check https://github.com/JChristensen/Timezone/blob/master/ReadMe.md for examples.

#include <Timezone.h>    //https://github.com/JChristensen/Timezone

//Eastern European Time Zone
// http://www.epochconverter.com/timezones
extern TimeChangeRule myDST = {"EEST", Last, Sun, Mar, 3, 180};    //Daylight time = UTC + 3 hours
extern TimeChangeRule mySTD = {"EET", Last, Sun, Nov, 4, 120};     //Standard time = UTC + 2 hours
extern Timezone myTZ(myDST, mySTD);

extern boolean NTPsuccess;

void NTP2localTime();
void getTheNTPTime();
