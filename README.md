# THE433

## ESP8266 RF433 Interface
## "control 433MHz (compatible with Arduino libs) sockets/devices through web and time rules"

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

* Alarm Once ! up to 7 days ahead.. no more !
(once when the time next reaches the given hour, minute)
i.e. now it's Monday 18:10. If you set an Alarm Once for 18:15 it will fire today.
If you set it for 18:05 it will fire the next day (Tuesday) at 18:05.

* Daily Alarm Repeat
(every day at the given hour, minute)

* Weekly Alarm Once ! up to 7 days ahead.. no more !
(once only on the next DayOfWeek, hour, minute)
If you set as day the running day and hour after the current hour
the alarm will fire today.

* Weekly Alarm Repeat
(every week on the given DayOfWeek, hour, minute)


A cleanup function that runs every 33 minutes checks if any of the *Once commands
are executed and deletes them from the SPIFFS save file. 
! You will continue to see them in the list until your next esp8266 reboot !

**! Please WAIT between refreshing the web interface if you have restarted the esp8266 !**
(a 3 second delay is added to every server responce on purpose)

**! If your esp8266 can't get the NTP time then Time Commands are not available !**

**! The esp8266 does not have an RTC. This sketch is based in software time keeping
with the Time library and it will have some drift overtime. A restart of the device will update time !**

----------------

used/tested Arduino IDE: 1.6.5-r5

Files:
* THE433.ino // main sketch
* NKTP.h & NKNTP.ino // get NTP time and convert to local time
* espWiFi2eeprom.h & espWiFi2eeprom.ino // esp8266 WiFi configuration to eeprom manager for Arduino IDE, check espWiFi2eeprom.h

Libraries needed:
* ESP/Arduino core (used working version 2.3.0)
* RCSwitch library for 433MHz RF control // https://github.com/sui77/rc-switch/
* RFControl library for 433MHz too.. // https://github.com/pimatic/RFControl
* (not a library) espWiFi2eeprom  // https://github.com/nikant/espWiFi2eeprom
* Time library // https://github.com/PaulStoffregen/Time
* TimeAlarms library modified dtNBR_ALARMS from 6 to 20, NO NEED WITH THE NEW LIBRARY  // https://github.com/PaulStoffregen/TimeAlarms
* TimeZone library // https://github.com/JChristensen/Timezone

\------------ Thanks goes to fivosv for his patience to help me \------------

### Preparing the code for your needs:

To use this you have to know how to work with RF 433MHz switches/sockets and have
the needed transmitters/receivers for your esp8266 or NodeMCU dev kit.

**1. In order to connect to WiFi this uses the espWiFi2eeprom added files. You can read an example of how they work here: https://github.com/nikant/espWiFi2eeprom and see some screenshots here: https://nobugsjustfeatures.wordpress.com/2016/03/27/esp8266-yet-another-esp-wifi-config-to-eeprom/**

**2. Set your own time zone in the NKNTP.h file.**

**3. Set the appropriate 433 MHz RCSwitch codes to the "*433 functions*" section add new functions if needed**

Code to edit for your RF 433 sockets or other devices:
Find the // -------------------------------- START 433 functions ---------------------------------
and edit switchon1() and switchoff1() functions according to your data sniffed from RCSwitch library

    // -------------------------------- with RCSwitch library -------------
    .
    .
    .
    void switchon1() {
      RCSwitchCall(260, "010001111000101001010101", "<h2>RC switch 1 ON</h2>");
      //Pulse Length-^^ 
      //Binary Code-------------^^^^ 
      //HTML string for web page------------------------------^^^^^
    }
    
    void switchoff1() {
      RCSwitchCall(260, "000010000000100101010100", "<h2>RC switch 1 OFF</h2>");
    }

or with  RFControl library

    // -------------------------------- with RFControl library -------------
    void switchon2() {
      unsigned long nsigbuckets[] = {1256, 496, 2984, 6256, 0, 0, 0, 0};
      RFControlCall(nsigbuckets, "01101010010000000101011111011001101001000100101024", "<h2>RC2-1</h2>");
    }
    
    void switchoff2() {
      unsigned long nsigbuckets[] = {1256, 492, 2984, 6258, 0, 0, 0, 0};
      RFControlCall(nsigbuckets, "01101101100100000110011111011010011001001010101024", "<h2>RC2-0</h2>");
    }

The names of the above functions or others you have created must be entered also on the part of the code that enumerates the Time Commands which is the *TcommandsFunctions* array.

    // Available commands for all the 433MHz devices to be controled
    // All functions available at the "433 functions" section below
    // Function names
    void (*TCommandsFunctions[])() = {switchon1, switchoff1, switchon2, switchoff2};

Also the equal number of names to the *TcommandsFNames* array. Those last strings are also used from the main form of the web interface to command an RF switch.

    // Keywords that fire the 433MHz functions from the HTML start page
    char* TCommandsFNames[] = {"SWITCH-1-ON", "SWITCH-1-OFF", "SWITCH-2-ON", "SWITCH-2-OFF"};


**4.Other parts of the code to edit**

Your esp8266 local network IP and port

    // Update these with values suitable for your network.
    IPAddress nkip(192, 168, 1, 101); //Node static IP
    IPAddress nkgateway(192, 168, 1, 1);
    IPAddress nksubnet(255, 255, 255, 0);
    
    // Port of esp8266 webserver
    ESP8266WebServer server(80);

The GPIO of the 433MHz transmitter 

    // 433MHz RF pins for esp8266 and the onboard led
    // pin 4 is GPIO4 which is D2 in the NodeMCU v1.0 board.
    #define RCSwitchPin 4

If you have a NodeMCU devboard you can use the onboard led to blink when a signal is trasmitted
or add your own led.

    // set this to 1 for the NodeMCU devboard on board LED
    #define HASONBOARDLED 0

The keyword to enter the Time Commands interface from the main web form

    // Keyword to enter the Time Commands center from the start page
    #define TCOMMANDWORD "ENTERTIMECOMMANDS"




----------------

### Example: 

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

### LICENSE

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
