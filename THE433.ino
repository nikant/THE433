/*
"THE433"
"control 433MHz (compatible with Arduino libs) sockets/devices through web and time rules"

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

 - thanks goes to fivosv for his patience to help me

----------------
https://github.com/nikant/THE433/blob/master/README.md

used Arduino IDE: 1.6.5-r5

Files:
- THE433.ino // main sketch
- NKTP.h & NKNTP.ino // get NTP time and convert to local time. Needs modified Timezone library, check NKNTP.h
- espWiFi2eeprom.h & espWiFi2eeprom.ino // esp8266 WiFi configuration to eeprom manager for Arduino IDE, check espWiFi2eeprom.h

Libraries needed:
- ESP/Arduino core (used working version 2.1.0)
- RCSwitch library for 433MHz RF control // https://github.com/sui77/rc-switch/
- (not a library) espWiFi2eeprom  // https://github.com/nikant/espWiFi2eeprom
- Time library // https://github.com/PaulStoffregen/Time
- TimeAlarms library modified dtNBR_ALARMS from 6 to 11 // https://github.com/PaulStoffregen/TimeAlarms
- TimeZone library modified by nikant to remove eeprom access, check NKNTP.h // https://github.com/JChristensen/Timezone

----------------

To use this you have to know how to work with RF 433MHz switches/sockets and have
the needed transmitters/receivers for your esp8266 or NodeMCU dev kit.

In order to connect to WiFi this uses the espWiFi2eeprom added files.
You can read an example of how they work here: https://github.com/nikant/espWiFi2eeprom
and see some screenshots here: https://nobugsjustfeatures.wordpress.com/2016/03/27/esp8266-yet-another-esp-wifi-config-to-eeprom/

You must also set your own time zone in the NKNTP.h file.

Remember to set the appropriate 433 MHz RCSwitch codes to the "433 functions" section add new functions if needed
and their names to the TcommandsFunctions array.
Also the equal number of names to the TcommandsFNames array. Those last strings are also used from the main form of the web interface
to command an RF switch.

--------------------------------------------------------------------------------
*/

#include "FS.h"  // https://github.com/esp8266/Arduino/blob/master/doc/filesystem.md
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <RCSwitch.h>
#include "espWiFi2eeprom.h"  // https://github.com/nikant/espWiFi2eeprom
#include "NKNTP.h"
#include <Time.h>
#include <TimeAlarms.h>  // https://www.pjrc.com/teensy/td_libs_TimeAlarms.html

#define THE433_VERSION "0.5"

// Initialize the 433MHz RF switch
RCSwitch mySwitch = RCSwitch();

// Update these with values suitable for your network.
IPAddress nkip(192, 168, 1, 101); //Node static IP
IPAddress nkgateway(192, 168, 1, 1);
IPAddress nksubnet(255, 255, 255, 0);

// Port of esp8266 webserver
ESP8266WebServer server(80);

// 433MHz RF pins for esp8266 and the onboard led
// pin 4 is GPIO4 which is D2 in the NodeMCU v1.0 board.
const int RCSwitchPin = 4;
const int ONBOARDLED = 2;

// Filename of SPIFFS savefile with TimeCommands
// ! Always with a / at the beginning of the filename !
#define TCSaveFile "/TCconfig.ini"
// Delimiter used in TimeCommands savefile: 1 character
#define TCSaveFileDelimiter ","

// Keyword to enter the Time Commands center from the start page
#define TCOMMANDWORD "ENTERTIMECOMMANDS"

// Available commands for all the 433MHz devices to be controled
// All functions available at the "433 functions" section below
// Function names
void (*TCommandsFunctions[])() = {switchon1, switchoff1, switchon2, switchoff2};
// Keywords that fire the 433MHz functions from the start page
char* TCommandsFNames[] = {"SWITCH 1 ON", "SWITCH 1 OFF", "SWITCH 2 ON", "SWITCH 2 OFF"};
// Calculate number of functions
#define numAvailFuncs (sizeof(TCommandsFNames)/sizeof(char *))


// The number of alarms can be changed in the TimeAlarms header file TimeAlarms.h set by the constant dtNBR_ALARMS
// note that the RAM used equals dtNBR_ALARMS  * 11 bytes
// right now for THE433 project it is set to #define dtNBR_ALARMS 11
// for 10 Alarms possible + 1 for the cleanup function
#define MAXTCommands dtNBR_ALARMS - 1

// Array that holds the Time Commands that are executed Once in RAM so they can be deleted from SPIFFS save file in the future
String alarmepochs[MAXTCommands][2];
int countTCommands;

// -------------------------------- START WEB and MESSAGES ---------------------------------
// HTML Strings etc.
String TCsubmitResult = "";

String SPIFFSstatus;

String SPIFFSTCommandsList;

String pinstate = "<h2>-</h2>";

const char webPageClearTCButton[] PROGMEM = "<br><form action=\"/removeTCSaveFile\" target=\"_top\"><input type=\"submit\" value=\"! Delete TimeCommands file !\"></form>\n";

const char webPageHomeButton[] PROGMEM = "<br><form action=\"/\" target=\"_top\"><input type=\"submit\" value=\"home\"></form>\n";

const char webPage433form[] PROGMEM = "\n<form action='/submit' method='POST'><input type=\"text\" name=\"pin\" value=\"\"><br><input type=\"submit\" value=\"Submit\"></form>\n";

const char webPageTC_0[] PROGMEM = "<table style=\"margin-left: auto;margin-right: auto;width:80%;border: 1px solid #fff;\"><tr>"
                                   "<td style=\"text-align:center;width:10%;\">&nbsp;</td><td style=\"text-align:left;width:40%;\"><br>";

const char webPageTC_1[] PROGMEM = "<br><br><form action='/TCsubmit' method='POST'><b>Time Command:</b>&nbsp;<input type=\"text\" name=\"tmcommand\" id=\"formtmcommand\" value=\"\" required=\"required\"><br><br><br><b>Schedule:</b>&nbsp;<br><br>"
                                   "<input type=\"radio\" name=\"AlarmType\" value=\"AlarmOnce\">&nbsp;<b>Alarm Once</b><br>(once when the time next reaches the given hour, minute)<br><br><input type=\"radio\" name=\"AlarmType\" value=\"AlarmRepeatDaily\">&nbsp;<b>Daily Alarm Repeat</b><br>(every day at the given hour, minute)<br><br>"
                                   "<input type=\"radio\" name=\"AlarmType\" value=\"AlarmOnceDOW\">&nbsp;<b>Weekly Alarm Once</b><br>(once only on the next  DayOfWeek, hour, minute)<br><br><input type=\"radio\" name=\"AlarmType\" value=\"AlarmRepeatDOW\">&nbsp;<b>Weekly Alarm Repeat</b><br>(every week on the given  DayOfWeek, hour, minute)<br><br><br><br><b>Time:</b>&nbsp;<br><br>"
                                   "Day:&nbsp;<select size=\"1\" name=\"tcdow\" required=\"required\" id=\"formtcdow\">"
                                   "<option value=\"\">&nbsp;</option><option value=\"nodow\" selected>none</option><option value=\"1\">Sunday</option><option value=\"2\">Monday</option><option value=\"3\">Tuesday</option><option value=\"4\">Wednesday</option><option value=\"5\">Thursday</option><option value=\"6\">Friday</option><option value=\"7\">Saturday</option>"
                                   "</select>&nbsp;&nbsp;Hour:&nbsp;<select size=\"1\" name=\"tchour\" required=\"required\" id=\"formtchour\">"
                                   "<option value=\"\">&nbsp;</option><option value=\"0\" selected>00</option><option value=\"1\">1</option><option value=\"2\">2</option><option value=\"3\">3</option><option value=\"4\">4</option><option value=\"5\">5</option><option value=\"6\">6</option><option value=\"7\">7</option><option value=\"8\">8</option><option value=\"9\">9</option>"
                                   "<option value=\"10\">10</option><option value=\"11\">11</option><option value=\"12\">12</option><option value=\"13\">13</option><option value=\"14\">14</option><option value=\"15\">15</option><option value=\"16\">16</option><option value=\"17\">17</option><option value=\"18\">18</option><option value=\"19\">19</option><option value=\"20\">20</option><option value=\"21\">21</option><option value=\"22\">22</option><option value=\"23\">23</option>"
                                   "</select>&nbsp;&nbsp;Minutes:&nbsp;<select size=\"1\" name=\"tcminutes\" required=\"required\" id=\"formtcminutes\">"
                                   "<option value=\"\">&nbsp;</option><option value=\"0\" selected>00</option><option value=\"1\">1</option><option value=\"2\">2</option><option value=\"3\">3</option><option value=\"4\">4</option><option value=\"5\">5</option><option value=\"6\">6</option><option value=\"7\">7</option><option value=\"8\">8</option><option value=\"9\">9</option>"
                                   "<option value=\"10\">10</option><option value=\"11\">11</option><option value=\"12\">12</option><option value=\"13\">13</option><option value=\"14\">14</option><option value=\"15\">15</option><option value=\"16\">16</option><option value=\"17\">17</option><option value=\"18\">18</option><option value=\"19\">19</option><option value=\"20\">20</option><option value=\"21\">21</option><option value=\"22\">22</option><option value=\"23\">23</option>"
                                   "<option value=\"24\">24</option><option value=\"25\">25</option><option value=\"26\">26</option><option value=\"27\">27</option><option value=\"28\">28</option><option value=\"29\">29</option><option value=\"30\">30</option><option value=\"31\">31</option><option value=\"32\">32</option><option value=\"33\">33</option><option value=\"34\">34</option><option value=\"35\">35</option><option value=\"36\">36</option><option value=\"37\">37</option>"
                                   "<option value=\"38\">38</option><option value=\"39\">39</option><option value=\"40\">40</option><option value=\"41\">41</option><option value=\"42\">42</option><option value=\"43\">43</option><option value=\"44\">44</option><option value=\"45\">45</option><option value=\"46\">46</option><option value=\"47\">47</option>"
                                   "<option value=\"48\">48</option><option value=\"49\">49</option><option value=\"50\">50</option><option value=\"51\">51</option><option value=\"52\">52</option><option value=\"53\">53</option><option value=\"54\">54</option><option value=\"55\">55</option><option value=\"56\">56</option><option value=\"57\">57</option><option value=\"58\">58</option><option value=\"59\">59</option>"
                                   "</select>&nbsp;&nbsp;<br><br><input type=\"submit\" value=\"Submit\"></form><br>"
                                   "</td><td style=\"text-align:center;width:10%;\">&nbsp;</td><td style=\"text-align:left;width:40%;\"><br><br>";

const char webPageTC_2[] PROGMEM =  "<br><br></td></tr></table>\n";

const char NTPfail[] PROGMEM = "<br><br><p style=\"font-size:medium\"><b>NTP Time failure. TimeCommands are not available!</b></p>\n";
const char SPIFFSTCommandsList_1[] PROGMEM = "<hr><br><b>---&nbsp;TimeCommands in SPIFFS&nbsp;---</b>";
const char SPIFFSTCommandsList_2[] PROGMEM = "<br><b>---&nbsp;-&nbsp;---</b><br><br>";
const char SPIFFSTCommandsList_3[] PROGMEM = "<form action=\"/TClistdelline\" target=\"_top\"><ul style=\"list-style-type:none\">";
const char SPIFFSTCommandsList_4[] PROGMEM = "<li><input type=\"radio\" name=\"TC2Remove\" value=\"";
const char SPIFFSTCommandsList_5[] PROGMEM = "</ul><input type=\"submit\" value=\"delete selected TimeCommand\"></form>\n";
const char esprestartmsg[] PROGMEM = "<br>Restart esp in order to activate TimeCommands!";
const char webPageRestartButton[] PROGMEM = "<br><form action=\"/esp433restart\" target=\"_top\"><input type=\"submit\" value=\"! RESTART ESP !\"></form>\n";

String webPageFooter =  "\n<br>- version: " THE433_VERSION " -\n<br>ESP Free Heap: " + String(ESP.getFreeHeap()) + "\n</body></html>";

String webPageHeader(String webtitle, String webpagetitle, String backcolor, String fontcolor) {
  return  "<!DOCTYPE HTML>\n"
          "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"><title>" + webtitle + "</title>\n"
          "<style type=\"text/css\">body {text-align:center;font-family: sans-serif;background-color: " + backcolor + ";color: " + fontcolor + ";} a:link, a:visited, a:hover, a:active {color:" + fontcolor + ";}</style>\n"
          "</head>\n"
          "<body>\n"
          "<h1>" + webpagetitle + "</h1>\n";
}

// Lists the available Time Commands for the web interface
String TCommandsCall() {
  String TCwebstring = "<ul style=\"list-style-type:none\">";
  for (int i = 0; i < numAvailFuncs; ++i) {
    String thetcommand = TCommandsFNames[i];
    TCwebstring += "<li>";
    TCwebstring += i + 1;
    TCwebstring += ":&nbsp;&nbsp;<b>";
    TCwebstring += "<a href=\"#\" target=\"_top\" onClick=\"document.getElementById(\'formtmcommand\').value=\'" + thetcommand + "\'\">";
    TCwebstring += thetcommand;
    TCwebstring += "</a>";
    TCwebstring += "</b>&nbsp;&nbsp;<br><br>";
    TCwebstring += "</li>";
  }
  TCwebstring += "</ul>";
  return TCwebstring;
}

// -------------------------------- END WEB ---------------------------------

// -------------------------------- START 433 functions ---------------------------------
// Main 433MHz RF functions. Can be called from main page of the web interface with the keyowrds at the top.

void switchon1() {
  digitalWrite(ONBOARDLED, LOW);
  for (int i = 0; i <= 19; i++) {
    mySwitch.setPulseLength(260); // this should be set according to your reading from RF receiver
    mySwitch.send("010101010101010101010101"); // sample raw data for 433MHz RF transmitter
    Serial.print("1 ON ");
    Serial.println(i);
    delay(100);
  }
  pinstate = F("<h2>1</h2>");
  digitalWrite(ONBOARDLED, HIGH);
}

void switchoff1() {
  digitalWrite(ONBOARDLED, LOW);
  for (int i = 0; i <= 19; i++) {
    mySwitch.setPulseLength(260); // this should be set according to your reading from RF receiver
    mySwitch.send("010101010101010101010101"); // sample raw data for 433MHz RF transmitter
    Serial.print("1 OFF ");
    Serial.println(i);
    delay(100);
  }
  pinstate = F("<h2>0</h2>");
  digitalWrite(ONBOARDLED, HIGH);
}

void switchon2() {
  digitalWrite(ONBOARDLED, LOW);
  for (int i = 0; i <= 19; i++) {
    mySwitch.setPulseLength(260); // this should be set according to your reading from RF receiver
    mySwitch.send("010101010101010101010101"); // sample raw data for 433MHz RF transmitter
    Serial.print("2 ON ");
    Serial.println(i);
    delay(100);
  }
  pinstate = F("<h2>1</h2>");
  digitalWrite(ONBOARDLED, HIGH);
}

void switchoff2() {
  digitalWrite(ONBOARDLED, LOW);
  for (int i = 0; i <= 19; i++) {
    mySwitch.setPulseLength(260); // this should be set according to your reading from RF receiver
    mySwitch.send("010101010101010101010101"); // sample raw data for 433MHz RF transmitter
    Serial.print("2 OFF ");
    Serial.println(i);
    delay(100);
  }
  pinstate = F("<h2>0</h2>");
  digitalWrite(ONBOARDLED, HIGH);
}
// -------------------------------- END 433 functions ---------------------------------

// -------------------------------- START general functions ---------------------------------

// Pad time digits with zeros for HTML
String calcDigits(int digits) {
  // utility function for digital clock display: prints preceding colon and leading 0
  String stringDigi = "";
  if (digits < 10) stringDigi = "0" + String(digits);
  else stringDigi = String(digits);
  return stringDigi;
}

// Time.h (library) function to convert given time to seconds
time_t tmConvert_t(int YYYY, byte MM, byte DD, byte hh, byte mm, byte ss)
{
  tmElements_t tmSet;
  tmSet.Year = YYYY - 1970;
  tmSet.Month = MM;
  tmSet.Day = DD;
  tmSet.Hour = hh;
  tmSet.Minute = mm;
  tmSet.Second = ss;
  return makeTime(tmSet);         //convert to time_t
}

// -------------------------------- END general functions ---------------------------------

// -------------------------------- START ALARM functions ---------------------------------

// Empty array of Time Commands at boot
void nullFillalarmepochs() {
  for (int i = 0; i < MAXTCommands; i++)
    for (int j = 0; j < 2; j++) {
      alarmepochs[i][j] = "";
    }
}

// Function to delete Time Commands that have been executed and where set to execute Once only from SPIFFS save file
void CleanupTC() {
  //debug
  //Serial.print("tic "); Serial.println(second());
  for (int i = 0; i < MAXTCommands; i++) {
    if ((alarmepochs[i][0] != "") && (alarmepochs[i][1] != "")) {
      unsigned long chcktm = alarmepochs[i][0].toInt();
      if (chcktm > 0) {
        // No need to set it here. It is set at main setup function at the end.
        //chcktm = chcktm + (2 * 60); // (minutes * seconds) that must have passed in order to clean the alarm event
        if (now() > chcktm) {
          RemoveLinesSpiffsTC(alarmepochs[i][1]);
          delay(100);
          for (int j = 0; j < 2; j++)  {
            alarmepochs[i][j] = "";
          }
          Serial.println(F("periodic cleanup: TimeCommand removed!"));
        }
      }
    }
  }
}

// Add Time Commands that are set to execute Once only to an array to be cleaned from SPIFFS save file
void SetupCleanTimer(String inTC, String OnceTCepoch, int inLine) {
  inTC.replace("\n", "");
  inTC.trim();
  alarmepochs[inLine - 1][0] = OnceTCepoch;
  alarmepochs[inLine - 1][1] = inTC;
}

// Set Time Commands to Alarms from TimeAlarms library at boot
void SetupAlarms(int tmpTCommand, String tmpAlarmType, String tmpDOW, int tmpTChour, int tmpTCminute) {
  if (tmpAlarmType == "AlarmRepeatDaily") {
    Alarm.alarmRepeat(tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
    Serial.println(F("Alarm set!"));
  } else if (tmpAlarmType == "AlarmOnce") {
    Alarm.alarmOnce(tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
    Serial.println(F("Alarm set!"));
  } else {
    //dowSunday, dowMonday, dowTuesday, dowWednesday, dowThursday, dowFriday, or dowSaturday
    if (tmpAlarmType == "AlarmRepeatDOW") {
      if (tmpDOW == "1") {
        Alarm.alarmRepeat(dowSunday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      } else if (tmpDOW == "2") {
        Alarm.alarmRepeat(dowMonday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      } else if (tmpDOW == "3") {
        Alarm.alarmRepeat(dowTuesday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      } else if (tmpDOW == "4") {
        Alarm.alarmRepeat(dowWednesday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      } else if (tmpDOW == "5") {
        Alarm.alarmRepeat(dowThursday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      } else if (tmpDOW == "6") {
        Alarm.alarmRepeat(dowFriday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      } else if (tmpDOW == "7") {
        Alarm.alarmRepeat(dowSaturday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      }
      Serial.println(F("Alarm set!"));
    } else if (tmpAlarmType == "AlarmOnceDOW") {
      if (tmpDOW == "1") {
        Alarm.alarmOnce(dowSunday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      } else if (tmpDOW == "2") {
        Alarm.alarmOnce(dowMonday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      } else if (tmpDOW == "3") {
        Alarm.alarmOnce(dowTuesday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      } else if (tmpDOW == "4") {
        Alarm.alarmOnce(dowWednesday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      } else if (tmpDOW == "5") {
        Alarm.alarmOnce(dowThursday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      } else if (tmpDOW == "6") {
        Alarm.alarmOnce(dowFriday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      } else if (tmpDOW == "7") {
        Alarm.alarmOnce(dowSaturday, tmpTChour, tmpTCminute, 0, TCommandsFunctions[tmpTCommand]);
      }
      Serial.println(F("Alarm set!"));
    }
  }
  // Alarm.delay to feed the TimeAlarms library
  Alarm.delay(100);
}

// Parse lines from the SPIFFS save file into SetupAlarms function above
// and also to the commands to be cleaned array
void parseTC(String inTC, int inLine) {
  int tmpTCommand, tmpTChour, tmpTCminute, ind1, ind2, ind3, ind4, ind5, ind6;
  String tmpAlarmType, TCcc, TChh, TCmm, tmpDOW, tmpOnceTCepoch, tmpisOnce;

  ind1 = inTC.indexOf(TCSaveFileDelimiter);
  ind2 = inTC.indexOf(TCSaveFileDelimiter, ind1 + 1);
  ind3 = inTC.indexOf(TCSaveFileDelimiter, ind2 + 1);
  ind4 = inTC.indexOf(TCSaveFileDelimiter, ind3 + 1);
  ind5 = inTC.indexOf(TCSaveFileDelimiter, ind4 + 1);
  ind6 = inTC.indexOf(TCSaveFileDelimiter, ind5 + 1);

  TCcc = inTC.substring(0, ind1);
  tmpTCommand = TCcc.toInt();
  tmpAlarmType = inTC.substring(ind1 + 1, ind2);
  tmpDOW = inTC.substring(ind2 + 1, ind3);
  TChh = inTC.substring(ind3 + 1, ind4);
  tmpTChour = TChh.toInt();
  TCmm = inTC.substring(ind4 + 1, ind5);
  tmpTCminute = TCmm.toInt();
  tmpOnceTCepoch = inTC.substring(ind5 + 1, ind6);
  tmpisOnce = inTC.substring(ind6 + 1);
  SetupAlarms(tmpTCommand, tmpAlarmType, tmpDOW, tmpTChour, tmpTCminute);
  // set commands that are Once only to an array to be cleaned from SPIFFS save file
  if (tmpisOnce == "1") SetupCleanTimer(inTC, tmpOnceTCepoch, inLine);
  //Serial.println(inLine); Serial.println(tmpOnceTCepoch);
}

// Parse lines from the SPIFFS save file for HTML show
String parseTC2HTML(String inTC, int inLine) {
  String tmpAlarmType, TCcc, tmpTChour, tmpTCminute, TCdd, parsed2html;
  int ind1, ind2, ind3, ind4, ind5, ind6, tmpTCommand, tmpDOW;

  ind1 = inTC.indexOf(TCSaveFileDelimiter);
  ind2 = inTC.indexOf(TCSaveFileDelimiter, ind1 + 1);
  ind3 = inTC.indexOf(TCSaveFileDelimiter, ind2 + 1);
  ind4 = inTC.indexOf(TCSaveFileDelimiter, ind3 + 1);
  ind5 = inTC.indexOf(TCSaveFileDelimiter, ind4 + 1);
  ind6 = inTC.indexOf(TCSaveFileDelimiter, ind5 + 1);

  TCcc = inTC.substring(0, ind1);
  tmpTCommand = TCcc.toInt();
  tmpAlarmType = inTC.substring(ind1 + 1, ind2);
  TCdd = inTC.substring(ind2 + 1, ind3);
  tmpDOW = TCdd.toInt();
  tmpTChour = inTC.substring(ind3 + 1, ind4);
  tmpTCminute = inTC.substring(ind4 + 1, ind5);
  //tmpOnceTCepoch = inTC.substring(ind5 + 1, ind6);
  //tmpisOnce = inTC.substring(ind6 + 1);
  parsed2html = "<b>" + String(inLine) + ".&nbsp;type:</b>" + tmpAlarmType + "&nbsp;&nbsp;<b>command:</b>";
  parsed2html += String(TCommandsFNames[tmpTCommand]);
  if (tmpDOW > 0) parsed2html += "&nbsp;&nbsp;<b>day:</b>" + String(dayStr(tmpDOW));
  parsed2html += "&nbsp;&nbsp;<b>hour:</b>" + String(tmpTChour) + "&nbsp;&nbsp;<b>minute:</b>" + tmpTCminute + "<br><br>";
  return parsed2html;
}

// -------------------------------- END ALARM functions ---------------------------------

// -------------------------------- START server functions ---------------------------------
// Timestamp function for the web interface
String timestamp() {
  if (NTPsuccess) {
    String theday = dayStr(weekday());
    String themonth = monthStr(month());
    return "<br><br><p>local time timestamp: <b>" + theday + ", " + String(day()) + " " + themonth + " " + String(year()) + "</b> -- <b>" + calcDigits(hour()) + ":" + calcDigits(minute()) + ":" + calcDigits(second()) + "</b></p>\n";
  } else return FPSTR(NTPfail);
}

// When the Restart button is pressed at the web interface
void handle_restart() {
  String tmppinstate;
  tmppinstate = pinstate;
  pinstate = F("<b>Please wait a few seconds.<br>! Device will reboot !</b><br><br>");
  pinstate += tmppinstate;
  delay(100);
  handle_root();
  delay(500);
  handle_APrestart();
}

// Main Time Commands web interface
void handle_timecommands() {
  String SServerSend;
  SServerSend = webPageHeader("THE433 TimeCommands control", "THE433 TimeCommands control", "#333", "#fff");
  if (NTPsuccess) {
    SServerSend += TCsubmitResult + SPIFFSstatus + "<br>";
    SServerSend += FPSTR(webPageTC_0);
    SServerSend += F("<b>You can add up to&nbsp;");
    SServerSend += MAXTCommands;
    SServerSend += F("&nbsp;Time Commands</b>");
    SServerSend += FPSTR(webPageTC_1);
    SServerSend += TCommandsCall();
    SServerSend += FPSTR(webPageTC_2);
  }
  SServerSend += timestamp();
  SServerSend += FPSTR(webPageHomeButton);
  if (NTPsuccess) SServerSend += FPSTR(SPIFFSTCommandsList_1);
  if (NTPsuccess) SServerSend += SPIFFSTCommandsList;
  if (NTPsuccess) SServerSend += FPSTR(SPIFFSTCommandsList_2);
  if (NTPsuccess) SServerSend += FPSTR(webPageClearTCButton);
  SServerSend += FPSTR(webPageRestartButton);
  SServerSend += webPageFooter;
  delay(100);
  server.send(200, "text/html", SServerSend);
}

// When a new Time Command is added from the web interface
void handle_TCsubmit() {
  String state = server.arg("tmcommand");
  if (countTCommands < MAXTCommands) {
    int TCommandPos = -1;
    for (int i = 0; i < numAvailFuncs; ++i) {
      if (state == TCommandsFNames[i]) {
        TCommandPos = i;
      }
    }
    if (TCommandPos == -1) {
      TCsubmitResult = F("Command not found!");
    } else {
      if ((server.arg("AlarmType") == "") || (server.arg("tchour") == "") || (server.arg("tcminutes") == "")) {
        TCsubmitResult = F("Wrong or no selection in Schedule or Time!");
      } else if (((server.arg("tcdow") == "") || (server.arg("tcdow") == "nodow")) && ((server.arg("AlarmType") == "AlarmRepeatDOW") || (server.arg("AlarmType") == "AlarmOnceDOW"))) {
        TCsubmitResult = F("You have to select a Day with that alarm type!");
      } else if (((server.arg("tchour").toInt() >= 0) && (server.arg("tchour").toInt() < 24)) || ((server.arg("tcminutes").toInt() >= 0) && (server.arg("tcminutes").toInt() < 59))) {
        String S2Write;
        int s2d, s2h, s2m;
        s2d = server.arg("tcdow").toInt();
        s2h = server.arg("tchour").toInt();
        s2m = server.arg("tcminutes").toInt();
        S2Write = String(TCommandPos) + TCSaveFileDelimiter;
        S2Write += server.arg("AlarmType") + TCSaveFileDelimiter;
        if ((server.arg("AlarmType") == "AlarmRepeatDaily") || (server.arg("AlarmType") == "AlarmOnce")) {
          S2Write += "nodow";
          S2Write += TCSaveFileDelimiter;
        } else S2Write += server.arg("tcdow") + TCSaveFileDelimiter;
        S2Write += server.arg("tchour") + TCSaveFileDelimiter;
        S2Write += server.arg("tcminutes") + TCSaveFileDelimiter;
        time_t s_tm;
        if (server.arg("AlarmType") == "AlarmOnce") {
          if (s2h > hour()) {
            s_tm = tmConvert_t(year(), month(), day(), s2h, s2m, 0);
          } else if ((s2h == hour()) && (s2m > minute())) {
            s_tm = tmConvert_t(year(), month(), day(), s2h, s2m, 0);
          } else {
            s_tm = tmConvert_t(year(), month(), day() + 1, s2h, s2m, 0);
          }
          S2Write += String(s_tm) + String(TCSaveFileDelimiter) + "1";
        } else if (server.arg("AlarmType") == "AlarmOnceDOW") {
          if (s2d == weekday()) {
            if (s2h > hour()) {
              s_tm = tmConvert_t(year(), month(), day(), s2h, s2m, 0);
            } else if ((s2h == hour()) && (s2m > minute())) {
              s_tm = tmConvert_t(year(), month(), day(), s2h, s2m, 0);
            } else {
              s_tm = tmConvert_t(year(), month(), day() + 7, s2h, s2m, 0);
            }
          } else {
            int s2dd = s2d - weekday();
            if (s2dd < 0) {
              s2dd = s2dd + 7;
            }
            s_tm = tmConvert_t(year(), month(), day() + s2dd, s2h, s2m, 0);
          }
          S2Write += String(s_tm) + String(TCSaveFileDelimiter) + "1";
        } else S2Write += "0" + String(TCSaveFileDelimiter) + "0";
        writeSpiffsTC(S2Write);
      }
    }
  } else {
    TCsubmitResult = F("Up to ");
    TCsubmitResult += MAXTCommands;
    TCsubmitResult += F(" TimeCommands are allowed!");
  }
  delay(100);
  handle_timecommands();
}

// Execute Time Command remove from save file from web interface
void handle_TCsubmitRemove() {
  delay(100);
  if (server.arg("TC2Remove") != "") {
    RemoveLinesSpiffsTC(server.arg("TC2Remove"));
  } else TCsubmitResult = F("No Time Command selected to delete!");
  delay(100);
  handle_timecommands();
}

// Main web page for controling 433MHz switches directly
void handle_root() {
  String SServerSend;
  SServerSend = webPageHeader("THE433 control", "THE433 control", "#000", "#fff") + pinstate;
  SServerSend += FPSTR(webPage433form);
  SServerSend += timestamp();
  SServerSend += SPIFFSstatus;
  SServerSend += FPSTR(webPageHomeButton);
  SServerSend += webPageFooter;
  server.send(200, "text/html", SServerSend);
}

// Form of the main web page, activates a 433MHz switch or enters Time Commands interface
void handle_submit() {
  String state = server.arg("pin");
  if (state == TCOMMANDWORD) { //"ENTERTIMECOMMANDS"
    delay(1000);
    handle_timecommands();
  } else {
    int TCommandPos = -1;
    for (int i = 0; i < numAvailFuncs; ++i) {
      if (state == TCommandsFNames[i]) {
        TCommandPos = i;
      }
    }
    if (TCommandPos != -1) {
      TCommandsFunctions[TCommandPos]();
      delay(3000);
      handle_root();
    } else {
      pinstate = F("<h2>-</h2>");
      digitalWrite(ONBOARDLED, HIGH);
      delay(3000);
      handle_root();
    }
  }
}

// -------------------------------- END server functions ---------------------------------

// -------------------------------- START SPIFFS functions ---------------------------------
// Format SPIFFS of esp8266, not accessible from the web interface
void formatspiffs() {
  String SServerSend;
  SServerSend = webPageHeader("THE433 control", "THE433 control", "#000", "#fff");
  SServerSend += F("<h2>! formatting SPIFFS !<br>Please wait 30 seconds.<br>! Device will reboot then !</h2>");
  SServerSend += timestamp();
  SServerSend += webPageFooter;
  server.send(200, "text/html", SServerSend);
  SPIFFS.begin();
  SPIFFS.format();
  delay(30000);
  Serial.println(F("SPIFFS formatted!"));
  handle_APrestart();
}

// Deletes the TCSaveFile that Time Commands are stored in from SPIFFS
void removeTCSaveFile() {
  SPIFFS.remove(TCSaveFile);
  delay(1000);
  Serial.println(TCSaveFile " removed.");
  //handle_APrestart();
  handle_restart();
}

// Reads the TCSaveFile that Time Commands are stored in from SPIFFS
// at boot and sets the needed Alarm timers.
// Also lists stored Time Commands for the web interface
void readSpiffsTC() {
  SPIFFSstatus = "";
  SPIFFSTCommandsList = "";
  if (SPIFFS.begin()) {
    SPIFFSstatus = F("SPIFFS mounted.\n");
    Serial.println(SPIFFSstatus);
    if (SPIFFS.exists(TCSaveFile)) {
      Serial.println("reading " TCSaveFile);
      File TCconfigFile = SPIFFS.open(TCSaveFile, "r");
      if (TCconfigFile) {
        SPIFFSTCommandsList = FPSTR(SPIFFSTCommandsList_3);
        Serial.println("opened " TCSaveFile);
        while (TCconfigFile.available()) {
          String line = TCconfigFile.readStringUntil('\n');
          line.replace("\n", "");
          line.trim();
          Serial.println(line);
          countTCommands++;
          parseTC(line, countTCommands);
          delay(100);
          SPIFFSTCommandsList += FPSTR(SPIFFSTCommandsList_4);
          SPIFFSTCommandsList += line + "\">&nbsp;&nbsp;";
          SPIFFSTCommandsList += parseTC2HTML(line, countTCommands);
          SPIFFSTCommandsList += "</li>";
          delay(100);
        }
        SPIFFSstatus += " " + String(countTCommands) + " commands read.\n";
        SPIFFSTCommandsList += FPSTR(SPIFFSTCommandsList_5);
      } else {
        Serial.println("Error opening " TCSaveFile "!");
        SPIFFSstatus = "Error opening " TCSaveFile "!\n";
      }
      TCconfigFile.close();
    } else {
      Serial.println(TCSaveFile " not found!");
      SPIFFSstatus += F(" TimeCommands file not found!\n");
    }
  } else {
    SPIFFSstatus = F("SPIFFS mount failed!\n");
  }
  SPIFFSstatus = "<p>" + SPIFFSstatus + "</p>\n";
}

// Writes a newly entered Time Command in TCSaveFile stored in SPIFFS
void writeSpiffsTC(String inputline) {
  TCsubmitResult = "";
  File TCconfigFile = SPIFFS.open(TCSaveFile, "a");
  if (!TCconfigFile) {
    Serial.println(TCSaveFile " doesn't exist yet. Creating it");
    File TCconfigFile = SPIFFS.open(TCSaveFile, "w");
    if (!TCconfigFile) {
      Serial.println(TCSaveFile "file creation failed!!!");
      TCsubmitResult = TCSaveFile "file creation failed!!!";
      return;
    }
  }
  TCconfigFile.println(inputline);
  delay(100);
  TCconfigFile.close();
  TCsubmitResult = "TimeCommand written in " TCSaveFile;
  TCsubmitResult += FPSTR(esprestartmsg);
  TCsubmitResult += FPSTR(webPageRestartButton);
}

// Removes a given line from TCSaveFile
void RemoveLinesSpiffsTC(String Line2Remove) {
  TCsubmitResult = "";
  if (SPIFFS.begin()) {
    SPIFFSstatus = F("SPIFFS mounted.\n");
    Serial.println(SPIFFSstatus);
    if (SPIFFS.exists(TCSaveFile)) {
      Serial.println("reading " TCSaveFile);
      File TCconfigFile = SPIFFS.open(TCSaveFile, "r");
      File tempTCconfigFile = SPIFFS.open("/TCconfig.temp", "w");
      if (!TCconfigFile || !tempTCconfigFile) {
        Serial.println(F("/TCconfig.temp file creation failed!!! Aborting!"));
        TCsubmitResult = F("/TCconfig.temp file creation failed!!! Aborting!");
        return;
      }
      Serial.println("opened " TCSaveFile);
      Serial.println(F("opened /TCconfig.temp"));
      while (TCconfigFile.available()) {
        String line = TCconfigFile.readStringUntil('\n');
        line.replace("\n", "");
        line.trim();
        if (Line2Remove != line) {
          Serial.println(line);
          tempTCconfigFile.println(line);
        }
        delay(100);
      }
      Serial.println(F("/TCconfig.temp written."));
      delay(10);
      // close files
      tempTCconfigFile.close();
      TCconfigFile.close();
      // replace config file
      if (SPIFFS.exists("/TCconfig.temp")) {
        SPIFFS.remove(TCSaveFile);
        delay(100);
        Serial.println(TCSaveFile " removed.");
        SPIFFS.rename("/TCconfig.temp", TCSaveFile);
        if (SPIFFS.exists(TCSaveFile)) {
          Serial.println(TCSaveFile " replaced.");
          TCsubmitResult = "TimeCommand removed!";
          TCsubmitResult += FPSTR(esprestartmsg);
          TCsubmitResult += FPSTR(webPageRestartButton);
        }
      }
    } else {
      Serial.println(TCSaveFile " not found!");
      SPIFFSstatus += F(" TimeCommands file not found!\n");
    }
  } else {
    SPIFFSstatus = F("SPIFFS mount failed!\n");
  }
  SPIFFSstatus = "<p>" + SPIFFSstatus + "</p>\n";
}

// -------------------------------- END SPIFFS functions ---------------------------------

// -------------------------------- setup & loop ---------------------------------
void setup(void) {
  pinMode(ONBOARDLED, OUTPUT);
  byte ledStatus = HIGH;

  countTCommands = 0;

  mySwitch.enableTransmit(RCSwitchPin);

  Serial.begin(57600);

  WiFi.mode(WIFI_STA);
  // uncomment the following if you set a static IP in the begining
  WiFi.config(nkip, nkgateway, nksubnet);

  // call espWiFi2eeprom to connect to saved to eeprom AP or
  // to create an AP to store new values for SSID and password
  espNKWiFiconnect();
  //--

  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());

  // espWiFi2eeprom: with the two following you can restart or clear eeprom
  // from your main esp8266 server
  server.on(restartcommand, handle_APrestart);
  server.on(cleareepromcommand, handle_clearAPeeprom);
  //--

  // get NTP time and convert it to local
  // NKNTP.h and NKNTP.ino files must be present
  // in NKNTP.h you can set your time zone and DST
  NTP2localTime();

  digitalWrite(ONBOARDLED, HIGH);

  delay(100);

  // Empty array of Time Commands at boot
  nullFillalarmepochs();

  delay(100);

  // If we got NTP time then Time Commands are available
  if (NTPsuccess) {
    readSpiffsTC();
    server.on("/TCsubmit", handle_TCsubmit);
    server.on("/removeTCSaveFile", removeTCSaveFile);
    server.on("/formatspiffs", formatspiffs);
    server.on("/TClistdelline", handle_TCsubmitRemove);
  }
  server.on("/esp433restart", handle_restart);

  server.on("/", handle_root);
  server.on("/submit", handle_submit);

  server.begin();

  // If we got NTP time set up the cleanup function of Once only executed Time Commands
  if (NTPsuccess)  {
    delay(100);
    Alarm.timerRepeat(33 * 60, CleanupTC); // (minutes * seconds) that must have passed in order to clean the alarm events
    Alarm.delay(0);
    Serial.println("Cleanup set");
  }

  Serial.println(F("HTTP server started"));
  //debug
  //for (int i = 0; i <= 10; i++) Serial.println(Alarm.read(i));
}

void loop(void) {
  server.handleClient();
  // the following is needed for the TimeAlarms libray
  Alarm.delay(0);
}

