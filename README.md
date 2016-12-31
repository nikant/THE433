THE433

"control 433MHz (compatible with Arduino libs) sockets/devices through web and time rules"

----------------
(esp8266 + RF 433 transmitter + Arduino IDE)

This is a sketch for controlling RF remote control sockets or relays that operate at the 433MHz frequency
and are compatible with the Arduino/esp8266 RCSwitch libraries.
The main feature is that you can schedule events (up to 10) to turn ON/OFF your devices that repeat daily/weekly
or that actuate once only within a week from setting the command.
A web interface is used to control the RF devices directly given a relative command at the main form and also 
scheduling the time events by giving at the same form the keyword ENTERTIMECOMMANDS (can be changed).

In the whole sketch these scheduled events are named Time Commands.

Time Commands are based on the TimeAlarms library.

Time Commands (up to 10) execute your functions that fire
the 433MHz sockets operating devices and can be set:

- Alarm Once ! up to 7 days ahead.. no more !
(once when the time next reaches the given hour, minute)
i.e. now it's Monday 18:10. If you set an Alarm Once for 18:15 it will fire today.
If you set it for 18:05 it will fire the next day (Tuesday) at 18:05.

- Daily Alarm Repeat
(every day at the given hour, minute)

- Weekly Alarm Once ! up to 7 days ahead.. no more !
(once only on the next DayOfWeek, hour, minute)
If you set as day the running day and hour after the current hour
the alarm will fire today.

- Weekly Alarm Repeat
(every week on the given DayOfWeek, hour, minute)


A cleanup function that runs every 33 minutes checks if any of the *Once commands
are executed and deletes them from the SPIFFS save file. 
! You will continue to see them in the list until your next esp8266 reboot !

! Please WAIT between refreshing the web interface if you have restarted the esp8266 !
(a 3 second delay is added to every server responce on purpose)

! If your esp8266 can't get the NTP time then Time Commands are not available !

! The esp8266 does not have an RTC. This sketch is based in software time keeping
with the Time library and it will have some drift overtime. A restart of the device will update time !

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

----------------

used/tested Arduino IDE: 1.6.5-r5

Files:
- THE433.ino // main sketch
- NKTP.h & NKNTP.ino // get NTP time and convert to local time. Needs modified Timezone library, check NKNTP.h
- espWiFi2eeprom.h & espWiFi2eeprom.ino // esp8266 WiFi configuration to eeprom manager for Arduino IDE, check espWiFi2eeprom.h

Libraries needed:
- ESP/Arduino core (used working version 2.3.0)
- RCSwitch library for 433MHz RF control // https://github.com/sui77/rc-switch/
- RFControl library for 433MHz too.. // https://github.com/pimatic/RFControl
- (not a library) espWiFi2eeprom  // https://github.com/nikant/espWiFi2eeprom
- Time library // https://github.com/PaulStoffregen/Time
- TimeAlarms library modified dtNBR_ALARMS from 6 to 20 // https://github.com/PaulStoffregen/TimeAlarms
- TimeZone library // https://github.com/JChristensen/Timezone

----------------
- thanks goes to fivosv for his patience to help me
----------------

Example usage: 

ESP8266 in the source is set as a WiFi station with a static IP 192.168.1.101

- navigate with your browser at this IP 
- here you can enter either a command to activate a connected device (you must edit the source code) i.e. SWITCH 1 ON
  or you can enter the provided keyword to enter the Time Commands interface i.e. ENTERTIMECOMMANDS
- at the Time Commands interface you can select a function you want to be executed at a specific time
- select the time to activate and submit
- you'll have to restart your esp8266 and you'll be informed about this
- after esp8266 restarts and reconnects you'll find the saved Time Command at the bottom of the web interface (from there you can also delete it)

You can see some screenshots and a video here: https://nobugsjustfeatures.wordpress.com/2016/04/02/the433-home-automation-with-esp8266-nodemcu/

----------------

LICENSE

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



