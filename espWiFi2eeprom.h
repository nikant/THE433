// you should change the following two to something of your own to remember
#define AP_RESTART "esprestart"
#define AP_CLEAREEPROM "cleareeprom"
#define AP_WIFICFGPORT 8266
//--
// password for the esp8266 softAP creates when it doesn't find info in the eeprom
// or when something else goes wrong and can't connect
const char AP_password[] = "esp8266control";
//--

#define ESPWIFI2EEPROM_VERSION "0.4"
void espNKWiFiconnect();
void handle_APrestart();
void handle_clearAPeeprom();
extern const char* restartcommand;
extern const char* cleareepromcommand;
