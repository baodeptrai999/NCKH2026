// Your WiFi credentials.
// char ssid[] = "Hoàng Anh";
// char pass[] = "0969917383";

#define ssid      "ESP32"             //AP SSID
#define pass      "123456789"                //AP Password

String  Essid = "";                 //EEPROM Network SSID
String  Epass = "";                 //EEPROM Network Password
String  Etoken = "";                //EEPROM Network token

String  sssid = "";                 //Read SSID From Web Page
String  passs = "";                 //Read Password From Web Page
String  token = "";                 //Read token From Web Page