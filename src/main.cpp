#include <Arduino.h>
#include <WiFiManager.h>
#include <EEPROM.h>

// Device Parameters -------------------------------------------------------------------------
String equipment_id, password_plain, password_cipher;

// WiFi Manager -------------------------------------------------------------------------
#define CAPTIVE_PORTAL_TIMEOUT 60
WiFiManager wm;
WiFiManagerParameter eq_id("1", "Equipment ID", "", 40);
// WiFiManagerParameter server_name("2", "Server Name", "", 40);
// WiFiManagerParameter charger_identifier("3", "Charger ID", "", 40);
String eq_id_str = ""; //, server_device_name = "", charger_name = "";
void saveParamsCallback();
int writeStringToEEPROM(int addrOffset, const String &strToWrite);
int readStringFromEEPROM(int addrOffset, String *strToRead);

void setup()
{
  WiFi.mode(WIFI_AP); // explicitly set mode, esp defaults to STA+AP
  WiFi.enableAP(true);
  EEPROM.begin(100);
  // wm.resetSettings();
  wm.addParameter(&eq_id);
//   wm.addParameter(&server_name);
//   wm.addParameter(&charger_identifier);
  wm.setTimeout(CAPTIVE_PORTAL_TIMEOUT); // if nobody logs in to the portal, continue after timeout
  wm.setConnectTimeout(CAPTIVE_PORTAL_TIMEOUT);
  wm.setAPClientCheck(true); // avoid timeout if client connected to softap
  wm.setSaveParamsCallback(saveParamsCallback);
  std::vector<const char *> menu = { "wifi", "sep","restart", "exit" };
  wm.setMenu(menu);
  wm.setClass("invert");

  if (wm.startConfigPortal("Quick-eV IoT Adapter", "1234567890")) {
    Serial.println("Portal Running");
  }
  else
  {
    if (wm.autoConnect("Quick-eV IoT Adapter", "1234567890"))
      Serial.println("Connected");
    else
    {
      Serial.println("Failed");
      ESP.restart();
    }
  }

  readStringFromEEPROM(1, &eq_id_str);
//   readStringFromEEPROM(20, &server_device_name);
//   readStringFromEEPROM(50, &charger_name);

  equipment_id = eq_id_str;
//   ch_port = charger_name;
//   in_topic = "devices/" + server_device_name + "/inmessages/";
//   out_topic = "devices/" + server_device_name + "/outmessages/";

  Serial.println(eq_id_str);
//   Serial.println(server_device_name);
//   Serial.println(charger_name);
}

void loop()
{
    wm.process();
}

void saveParamsCallback() {
  Serial.println("Get Params:");

  Serial.print(eq_id.getID());
  Serial.print(" : ");
  eq_id_str = String(eq_id.getValue());
  Serial.println(eq_id_str);
  writeStringToEEPROM(1, eq_id_str);

//   Serial.print(server_name.getID());
//   Serial.print(" : ");
//   server_device_name = String(server_name.getValue());
//   Serial.println(server_device_name);
//   writeStringToEEPROM(20, server_device_name);
  
//   Serial.print(charger_identifier.getID());
//   Serial.print(" : ");
//   charger_name = String(charger_identifier.getValue());
//   Serial.println(charger_name);
//   writeStringToEEPROM(50, charger_name);
}

int writeStringToEEPROM(int addrOffset, const String &strToWrite) {
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  EEPROM.commit();

  for (int i = 0; i < len; i++) {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
  EEPROM.commit();

  return addrOffset + 1 + len;
}

int readStringFromEEPROM(int addrOffset, String *strToRead) {
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];

  for (int i = 0; i < newStrLen; i++) {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';

  *strToRead = String(data);
  return addrOffset + 1 + newStrLen;
}