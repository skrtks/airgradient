#include <SHTSensor.h>
#include <arduino-sht.h>

/*
Important: This code is only for the DIY PRO PCB Version 3.7 that has a push button mounted.

This is the code for the AirGradient DIY PRO Air Quality Sensor with an ESP8266 Microcontroller with the SGP40 TVOC module from AirGradient.

It is a high quality sensor showing PM2.5, CO2, Temperature and Humidity on a small display and can send data over Wifi.

Build Instructions: https://www.airgradient.com/open-airgradient/instructions/diy-pro-v37/

Kits (including a pre-soldered version) are available: https://www.airgradient.com/open-airgradient/kits/

The codes needs the following libraries installed:
“WifiManager by tzapu, tablatronix” tested with version 2.0.11-beta
“U8g2” by oliver tested with version 2.32.15
"Sensirion I2C SGP41" by Sensation Version 0.1.0
"Sensirion Gas Index Algorithm" by Sensation Version 3.2.1
"Arduino-SHT" by Johannes Winkelmann Version 1.2.2

Configuration:
Please set in the code below the configuration parameters.

If you have any questions please visit our forum at https://forum.airgradient.com/

If you are a school or university contact us for a free trial on the AirGradient platform.
https://www.airgradient.com/

CC BY-SA 4.0 Attribution-ShareAlike 4.0 International License

*/


#include <AirGradient.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>

#include <EEPROM.h>
#include "SHTSensor.h"

//#include "SGP30.h"
#include <SensirionI2CSgp41.h>
#include <NOxGasIndexAlgorithm.h>
#include <VOCGasIndexAlgorithm.h>

#include <U8g2lib.h>
#include <ElegantOTA.h>

#include <arduino_homekit_server.h>

AirGradient ag = AirGradient();
SensirionI2CSgp41 sgp41;
VOCGasIndexAlgorithm voc_algorithm;
NOxGasIndexAlgorithm nox_algorithm;
SHTSensor sht;
const int port = 9926;
ESP8266WebServer server(port);
bool display = true;

// time in seconds needed for NOx conditioning
uint16_t conditioning_s = 10;

// for peristent saving and loading
int addr = 4;
byte value;

// Display bottom right
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Replace above if you have display on top left
//U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);


// CONFIGURATION START
// set to true to switch from Celcius to Fahrenheit
boolean inF = false;

// PM2.5 in US AQI (default ug/m3)
boolean inUSAQI = false;

// Display Position
boolean displayTop = true;

// set to true if you want to connect to wifi. You have 60 seconds to connect. Then it will go into an offline mode.
boolean connectWIFI=true;

// CONFIGURATION END


unsigned long currentMillis = 0;

const int oledInterval = 5000;
unsigned long previousOled = 0;

const int sendToServerInterval = 10000;
unsigned long previoussendToServer = 0;

const int tvocInterval = 1000;
unsigned long previousTVOC = 0;
int TVOC = 0;
int NOX = 0;

const int co2Interval = 5000;
unsigned long previousCo2 = 0;
int Co2 = 0;

const int pm25Interval = 5000;
unsigned long previousPm25 = 0;
int pm25 = 0;

const int tempHumInterval = 2500;
unsigned long previousTempHum = 0;
float temp = 0;
int hum = 0;

int buttonConfig=0;
int lastState = LOW;
int currentState;
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;

void startServer();

void setup() {
  Serial.begin(115200);
  Serial.println("Hello");
  u8g2.begin();
  sht.init();
  sht.setAccuracy(SHTSensor::SHT_ACCURACY_MEDIUM);
  //u8g2.setDisplayRotation(U8G2_R0);

  EEPROM.begin(512);
  delay(500);

  buttonConfig = String(EEPROM.read(addr)).toInt();
  if (buttonConfig>3) buttonConfig=0;
  delay(400);
  setConfig();
  Serial.println("buttonConfig: "+String(buttonConfig));
   updateOLED2("Press Button", "Now for", "Config Menu");
    delay(2000);
  pinMode(D7, INPUT_PULLUP);
  currentState = digitalRead(D7);
  if (currentState == LOW)
  {
    updateOLED2("Entering", "Config Menu", "");
    delay(3000);
    lastState = HIGH;
    setConfig();
    inConf();
  }

  if (connectWIFI)
  {
     connectToWifi();
  }

  updateOLED2("Warming Up", "Serial Number:", String(ESP.getChipId(), HEX));
  sgp41.begin(Wire);
  ag.CO2_Init();
  ag.PMS_Init();
  ag.TMP_RH_Init(0x44);
  startServer();
  toggleDisplay();
  // my_homekit_setup();
}

void startServer() {
    ElegantOTA.begin(&server);
    server.on("/metrics", sendToServer);
    server.on("/toggle_display", toggleDisplay);
    server.on("/homekit_reset", homekitReset);
    server.onNotFound(HandleNotFound);

    server.begin();
    Serial.println("HTTP server started at ip " + WiFi.localIP().toString() + ":" + String(port));
    updateOLED2("Listening on", WiFi.localIP().toString() + ": ", String(port));
    delay(6000);
}

void loop() {
  server.handleClient();
  currentMillis = millis();
  updateTVOC();
  updateOLED();
  updateCo2();
  updatePm25();
  updateTempHum();
  // my_homekit_loop();
}

void inConf(){
  setConfig();
  currentState = digitalRead(D7);

  if (currentState){
    Serial.println("currentState: high");
  } else {
    Serial.println("currentState: low");
  }

  if(lastState == HIGH && currentState == LOW) {
    pressedTime = millis();
  }

  else if(lastState == LOW && currentState == HIGH) {
    releasedTime = millis();
    long pressDuration = releasedTime - pressedTime;
    if( pressDuration < 1000 ) {
      buttonConfig=buttonConfig+1;
      if (buttonConfig>3) buttonConfig=0;
    }
  }

  if (lastState == LOW && currentState == LOW){
     long passedDuration = millis() - pressedTime;
      if( passedDuration > 4000 ) {
        // to do
//        if (buttonConfig==4) {
//          updateOLED2("Saved", "Release", "Button Now");
//          delay(1000);
//          updateOLED2("Starting", "CO2", "Calibration");
//          delay(1000);
//          Co2Calibration();
//       } else {
          updateOLED2("Saved", "Release", "Button Now");
          delay(1000);
          updateOLED2("Rebooting", "in", "5 seconds");
          delay(5000);
          EEPROM.write(addr, char(buttonConfig));
          EEPROM.commit();
          delay(1000);
          ESP.restart();
 //       }
    }

  }
  lastState = currentState;
  delay(100);
  inConf();
}


void setConfig() {
  if (buttonConfig == 0) {
    updateOLED2("Temp. in C", "PM in ug/m3", "Long Press Saves");
      u8g2.setDisplayRotation(U8G2_R0);
      inF = false;
      inUSAQI = false;
  }
    if (buttonConfig == 1) {
    updateOLED2("Temp. in C", "PM in US AQI", "Long Press Saves");
      u8g2.setDisplayRotation(U8G2_R0);
      inF = false;
      inUSAQI = true;
  } else if (buttonConfig == 2) {
    updateOLED2("Temp. in F", "PM in ug/m3", "Long Press Saves");
    u8g2.setDisplayRotation(U8G2_R0);
      inF = true;
      inUSAQI = false;
  } else  if (buttonConfig == 3) {
    updateOLED2("Temp. in F", "PM in US AQI", "Long Press Saves");
      u8g2.setDisplayRotation(U8G2_R0);
       inF = true;
      inUSAQI = true;
  }



  // to do
  // if (buttonConfig == 8) {
  //  updateOLED2("CO2", "Manual", "Calibration");
  // }
}

void updateTVOC()
{
 uint16_t error;
    char errorMessage[256];
    uint16_t defaultRh = 0x8000;
    uint16_t defaultT = 0x6666;
    uint16_t srawVoc = 0;
    uint16_t srawNox = 0;
    uint16_t defaultCompenstaionRh = 0x8000;  // in ticks as defined by SGP41
    uint16_t defaultCompenstaionT = 0x6666;   // in ticks as defined by SGP41
    uint16_t compensationRh = 0;              // in ticks as defined by SGP41
    uint16_t compensationT = 0;               // in ticks as defined by SGP41

    delay(1000);

    compensationT = static_cast<uint16_t>((temp + 45) * 65535 / 175);
    compensationRh = static_cast<uint16_t>(hum * 65535 / 100);

    if (conditioning_s > 0) {
        error = sgp41.executeConditioning(compensationRh, compensationT, srawVoc);
        conditioning_s--;
    } else {
        error = sgp41.measureRawSignals(compensationRh, compensationT, srawVoc,
                                        srawNox);
    }

    if (currentMillis - previousTVOC >= tvocInterval) {
      previousTVOC += tvocInterval;
      TVOC = voc_algorithm.process(srawVoc);
      NOX = nox_algorithm.process(srawNox);
      Serial.println(String(TVOC));
    }
}

void updateCo2()
{
    if (currentMillis - previousCo2 >= co2Interval) {
      previousCo2 += co2Interval;
      Co2 = ag.getCO2_Raw();
      Serial.println(String(Co2));
    }
}

void updatePm25()
{
    if (currentMillis - previousPm25 >= pm25Interval) {
      previousPm25 += pm25Interval;
      pm25 = ag.getPM2_Raw();
      Serial.println(String(pm25));
    }
}

void updateTempHum()
{
    if (currentMillis - previousTempHum >= tempHumInterval) {
      previousTempHum += tempHumInterval;

      if (sht.readSample()) {
      Serial.print("SHT:\n");
      Serial.print("  RH: ");
      Serial.print(sht.getHumidity(), 2);
      Serial.print("\n");
      Serial.print("  T:  ");
      Serial.print(sht.getTemperature(), 2);
      Serial.print("\n");
      temp = sht.getTemperature();
      hum = sht.getHumidity();
  } else {
      Serial.print("Error in readSample()\n");
  }
      Serial.println(String(temp));
    }
}

void updateOLED() {
   if (currentMillis - previousOled >= oledInterval) {
     previousOled += oledInterval;

    String ln3;
    String ln1;

    if (inUSAQI) {
      ln1 = "AQI:" + String(PM_TO_AQI_US(pm25)) +  " CO2:" + String(Co2);
    } else {
      ln1 = "PM:" + String(pm25) +  " CO2:" + String(Co2);
    }

     String ln2 = "TVOC:" + String(TVOC) + " NOX:" + String(NOX);

      if (inF) {
        ln3 = "F:" + String((temp* 9 / 5) + 32) + " H:" + String(hum)+"%";
        } else {
        ln3 = "C:" + String(temp) + " H:" + String(hum)+"%";
       }
     updateOLED2(ln1, ln2, ln3);
   }
}

void updateOLED2(String ln1, String ln2, String ln3) {
  if (DISPLAY) {
    char buf[9];
    u8g2.firstPage();
    u8g2.firstPage();
    do {
    u8g2.setFont(u8g2_font_t0_16_tf);
    u8g2.drawStr(1, 10, String(ln1).c_str());
    u8g2.drawStr(1, 30, String(ln2).c_str());
    u8g2.drawStr(1, 50, String(ln3).c_str());
      } while ( u8g2.nextPage() );
  }
}

void sendToServer() {
    String message = "";
    String idString = "{id=\"" + String("pi") + "\",mac=\"" + WiFi.macAddress().c_str() + "\"}";

    // Update sensor data
      message += "# HELP pm02 Particulate Matter PM2.5 value\n";
      message += "# TYPE pm02 gauge\n";
      message += "pm02";
      message += idString;
      message += String(pm25);
      message += "\n";
      message += "# HELP rco2 CO2 value, in ppm\n";
      message += "# TYPE rco2 gauge\n";
      message += "rco2";
      message += idString;
      message += String(Co2);
      message += "\n";
      message += "# HELP atmp Temperature, in degrees Celsius\n";
      message += "# TYPE atmp gauge\n";
      message += "atmp";
      message += idString;
      message += String(temp);
      message += "\n# HELP rhum Relative humidity, in percent\n";
      message += "# TYPE rhum gauge\n";
      message += "rhum";
      message += idString;
      message += String(hum);
      message += "\n# HELP tvoc Total volatile organic components, in μg/m³\n";
      message += "# TYPE tvoc gauge\n";
      message += "tvoc";
      message += idString;
      message += String(TVOC);
      message += "\n# HELP nox, in μg/m³\n";
      message += "# TYPE nox gauge\n";
      message += "nox";
      message += idString;
      message += String(NOX);
      message += "\n";

    if (WiFi.status() == WL_CONNECTED) {
        server.send(200, "text/plain", message);
    } else {
        Serial.println("WiFi Disconnected");
    }
}

void toggleDisplay() {
  if (display) {
    display = false;
    u8g2.clearDisplay();
    u8g2.setPowerSave(true);
  } else {
    display = true;
    u8g2.setPowerSave(false);
  }
}

void homekitReset() {
  homekit_storage_reset();
  server.send(200, "text/plain", "Reset homekit service");
}

// Wifi Manager
 void connectToWifi() {
   WiFiManager wifiManager;
   //WiFi.disconnect(); //to delete previous saved hotspot
   String HOTSPOT = "AG-" + String(ESP.getChipId(), HEX);
   updateOLED2("90s to connect", "to Wifi Hotspot", HOTSPOT);
   wifiManager.setTimeout(90);

   if (!wifiManager.autoConnect((const char * ) HOTSPOT.c_str())) {
     updateOLED2("booting into", "offline mode", "");
     Serial.println("failed to connect and hit timeout");
     delay(6000);
   }

}

// Calculate PM2.5 US AQI
int PM_TO_AQI_US(int pm02) {
  if (pm02 <= 12.0) return ((50 - 0) / (12.0 - .0) * (pm02 - .0) + 0);
  else if (pm02 <= 35.4) return ((100 - 50) / (35.4 - 12.0) * (pm02 - 12.0) + 50);
  else if (pm02 <= 55.4) return ((150 - 100) / (55.4 - 35.4) * (pm02 - 35.4) + 100);
  else if (pm02 <= 150.4) return ((200 - 150) / (150.4 - 55.4) * (pm02 - 55.4) + 150);
  else if (pm02 <= 250.4) return ((300 - 200) / (250.4 - 150.4) * (pm02 - 150.4) + 200);
  else if (pm02 <= 350.4) return ((400 - 300) / (350.4 - 250.4) * (pm02 - 250.4) + 300);
  else if (pm02 <= 500.4) return ((500 - 400) / (500.4 - 350.4) * (pm02 - 350.4) + 400);
  else return 500;
};

// Calculate PM2.5 US AQI
int PM_TO_HOMEKIT_AQI(int pm02) {
  if (pm02 <= 10.0) return 2;
  else if (pm02 <= 35.4) return 3;
  else if (pm02 <= 55.4) return 4;
  else return 5;
}

void HandleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/html", message);
}

//==============================
// Homekit setup and loop
//==============================

// access your homekit characteristics defined in my_accessory.c
extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t cha_current_temperature;
extern "C" homekit_characteristic_t cha_current_aqi;
extern "C" homekit_characteristic_t cha_current_pm25;
extern "C" homekit_characteristic_t cha_current_voc;
extern "C" homekit_characteristic_t cha_current_co2;
extern "C" homekit_characteristic_t cha_humidity;

static uint32_t next_heap_millis = 0;
static uint32_t next_report_millis = 0;

void my_homekit_setup() {
	arduino_homekit_setup(&config);
}

void my_homekit_loop() {
	arduino_homekit_loop();
	const uint32_t t = millis();
	if (t > next_report_millis) {
		// report sensor values every 10 seconds
		next_report_millis = t + 10 * 1000;
		my_homekit_report();
	}
	if (t > next_heap_millis) {
		// show heap info every 5 seconds
		next_heap_millis = t + 5 * 1000;
	}
}

void my_homekit_report() {
    cha_current_temperature.value.float_value = temp;
    cha_humidity.value.float_value = hum;
    cha_current_aqi.value.int_value = PM_TO_HOMEKIT_AQI(pm25);
    cha_current_pm25.value.float_value = pm25;
    cha_current_voc.value.float_value = TVOC;
    cha_current_co2.value.float_value = Co2;

    homekit_characteristic_notify(&cha_current_temperature, cha_current_temperature.value);
    homekit_characteristic_notify(&cha_humidity, cha_humidity.value);
    homekit_characteristic_notify(&cha_current_aqi, cha_current_aqi.value);
    homekit_characteristic_notify(&cha_current_pm25, cha_current_pm25.value);
    homekit_characteristic_notify(&cha_current_voc, cha_current_voc.value);
    homekit_characteristic_notify(&cha_current_co2, cha_current_co2.value);
}
