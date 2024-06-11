#include <Arduino.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <OTA_cert.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <TinyGSM.h>
#include <ArduinoHttpClient.h>
#include "time.h"
#include <SoftwareSerial.h>
#include <Firebase_ESP_Client.h>


#define debug true
#define Relays_Pin 32
#define Emergency_Button 12

// ---------------------------------------------------------------------------------------- Device Parameters
String equipment_id, password_plain, password_cipher;
String mac_address, ip_address;
bool provisioning_required = false;
String firmware_version, hardware_version, region;
String password, encrypted_password, key, iv;
int relay_status;

// ---------------------------------------------------------------------------------------- EEPROM Parameters
int equipment_id_address = 1, password_address = 20, encrypted_password_address = 30;

// ---------------------------------------------------------------------------------------- WiFi Manager
#define CAPTIVE_PORTAL_TIMEOUT 60
WiFiManager wm;
WiFiManagerParameter eq_id("1", "Equipment ID", "", 40);
// WiFiManagerParameter server_name("2", "Server Name", "", 40);
// WiFiManagerParameter charger_identifier("3", "Charger ID", "", 40);
String eq_id_str = ""; //, server_device_name = "", charger_name = "";
void saveParamsCallback();
int writeStringToEEPROM(int addrOffset, const String &strToWrite);
int readStringFromEEPROM(int addrOffset, String *strToRead);

// ---------------------------------------------------------------------------------------- OTA parameters
String FirmwareVer = {"1.0"};
#define URL_fw_Version "https://raw.githubusercontent.com/EgnionInnovation/Quick-eV_SmartIoTPlug/main/firmware_version.txt"
#define URL_fw_Bin "https://raw.githubusercontent.com/EgnionInnovation/Quick-eV_SmartIoTPlug/main/fw/firmware.bin"
void firmwareUpdate();
int FirmwareVersionCheck();

unsigned long previousMillis = 0; // will store last time LED was updated
unsigned long previousMillis_2 = 0;
const long interval = 3600000;
const long mini_interval = 60000;

void repeatedCall();
void firmwareUpdate(void);
int FirmwareVersionCheck(void);

// ---------------------------------------------------------------------------------------- Serial with Fixed ESP
SoftwareSerial ESPFixed;
void send_data();
void receive_data();
unsigned long send_data_time, SEND_DATA_INTERVAL;

// ---------------------------------------------------------------------------------------- NTP Server
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 18000;
int daylightOffset_sec = 0;
struct tm timeinfo;
int daylight_saving_state = 2;
String ntp_time = "";
void printLocalTime();

char timeSec[3];
char timeMin[3];
char timeHour[3];
char timeDay[3];
char timeWeek[3];
char timeMonth[3];
char timeYear[5];
char timeZone[4];

// ---------------------------------------------------------------------------------------- Firebase
// Insert Firebase project API Key
#define API_KEY "AIzaSyACMnlR3Bobo1B0uDN9Q1kxHPWrdeIs0ng"
bool id_status[1000];

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://evm-vending-admin-web-default-rtdb.firebaseio.com/"

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// ---------------------------------------------------------------------------------------- API Server
char *serverName = "https://staging.quick-ev.com/public/api";
WiFiClientSecure client;
const char *test_root_ca =
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "-----END CERTIFICATE-----\n";

void setup()
{
  delay(1000); // 1 second for settling
  // ---------------------------------------------------------------- Begin serial monitor, serial with fixed ESP and EEPROM
  Serial.begin(115200);
  ESPFixed.begin(9600, SWSERIAL_8N1, 34, 33, false);
  EEPROM.begin(150);
  if (debug)
  {
    Serial.println("Started!");
    Serial.println("Firmware Version: " + FirmwareVer);
  }
  // ---------------------------------------------------------------- WiFi Manager
  WiFi.mode(WIFI_AP); // explicitly set mode, esp defaults to STA+AP
  WiFi.enableAP(true);
  // wm.resetSettings();
  wm.addParameter(&eq_id);
  //   wm.addParameter(&server_name);
  //   wm.addParameter(&charger_identifier);
  wm.setTimeout(CAPTIVE_PORTAL_TIMEOUT); // if nobody logs in to the portal, continue after timeout
  wm.setConnectTimeout(CAPTIVE_PORTAL_TIMEOUT);
  wm.setAPClientCheck(true); // avoid timeout if client connected to softap
  wm.setSaveParamsCallback(saveParamsCallback);
  std::vector<const char *> menu = {"wifi", "sep", "restart", "exit"};
  wm.setMenu(menu);
  wm.setClass("invert");

  if (wm.startConfigPortal("Quick-eV IoT Adapter", "1234567890"))
  {
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

  // ---------------------------------------------------------------- API Server
  client.setCACert(test_root_ca);
  if (!client.connect(serverName, 80))
    Serial.println("Connection failed!");
  else
  {
    Serial.println("Connected to server!");
  }

  readStringFromEEPROM(equipment_id_address, &equipment_id);
  if (equipment_id == "")
  {
    provisioning_required = true;
  }

  // ---------------------------------------------------------------- Provisioning API Call
  while (provisioning_required)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      HTTPClient https;
      char buf[80];
      strcpy(buf, serverName);
      strcat(buf, "/equipment/provisioning");
      char *provisioning_server_name = buf;

      // Your Domain name with URL path or IP address with path
      https.begin(client, buf);

      StaticJsonDocument<1024> doc;
      doc["mac_address"] = String(WiFi.macAddress());
      doc["ip_address"] = WiFi.localIP().toString();
      doc["firm_version"] = firmware_version;
      doc["hardware_version"] = hardware_version;
      doc["status"] = "online";
      doc["region"] = region;

      char response[1024];
      serializeJson(doc, response);

      // If you need an HTTP request with a content type: application/json, use the following:
      https.addHeader("Content-Type", "application/json");
      int httpResponseCode = https.POST(response);

      Serial.println(response);

      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      String payload = "{}";
      payload = https.getString();

      Serial.println(payload);

      StaticJsonDocument<1024> api_response;
      DeserializationError err = deserializeJson(api_response, payload);
      if (err)
      {
        if (debug)
        {
          Serial.print(F("deserializeJson() failed with code "));
          Serial.println(err.f_str());
        }
      }
      else
      {
        String temp_equipment_id = api_response["equipment_id"];
        String temp_password = api_response["password"];
        String temp_encrypted_password = api_response["encrypted_password"];
        String temp_key = api_response["key"];
        String temp_iv = api_response["iv"];

        equipment_id = temp_equipment_id;
        password = temp_password;
        encrypted_password = temp_encrypted_password;
        key = temp_key;
        iv = temp_iv;
        writeStringToEEPROM(equipment_id_address, equipment_id);
        if (equipment_id != "")
        {
          provisioning_required = false;
        }
        else
        {
          if (debug)
            Serial.println("API call not successful. Trying again in 30 seconds.");
          delay(30000);
        }
      }

      // Free resources
      https.end();
    }
    else
    {
      Serial.println("WiFi Disconnected");
    }
  }

  // ---------------------------------------------------------------- Firebase
    /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;  //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // ---------------------------------------------------------------- Pin Modes
  pinMode(Relays_Pin, OUTPUT);
  pinMode(Emergency_Button, INPUT);
  attachInterrupt(Emergency_Button, ISR, FALLING);
}

void loop()
{
  wm.process();
  repeatedCall();

  if (ESPFixed.available())
  {
    Serial.println("Receiving Data...");
    receive_data();
  }

  if (millis() > send_data_time + SEND_DATA_INTERVAL)
  {
    if (debug)
      Serial.println("Sending data to Fixed ESP.");
    send_data();
  }
}

void send_data()
{
  ESPFixed.print("0,0,0,");
}

void receive_data()
{
  String dat = ESPFixed.readString();
  int c = 0;
  String val_read = "";

  // while (dat.charAt(c) != ',')
  // {
  //   val_read += dat.charAt(c);
  //   c++;
  // }
  // rfid_data = val_read;
  // Serial.print("RFID Data: ");
  // Serial.println(rfid_data);
  // c++;
  // val_read = "";

  // while (dat.charAt(c) != ',')
  // {
  //   val_read += dat.charAt(c);
  //   c++;
  // }
  // flag = val_read.toInt();
  // //Serial.print("Flag: ");
  // //Serial.println(flag);
  // c++;
  // val_read = "";

  // while (dat.charAt(c) != '\0')
  // {
  //   val_read += dat.charAt(c);
  //   c++;
  // }
  // RCB2_S3 = val_read.toFloat();
  // Serial.print("RCB2_S3: ");
  // Serial.println(RCB2_S3);
}

void printLocalTime()
{
  if (String(timeMonth).toInt() == 10 & timeWeek == 0 & String(timeDay).toInt() > 24 & daylight_saving_state != 0)
  {
    daylightOffset_sec = 0;
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    daylight_saving_state = 0;
  }

  if (String(timeMonth).toInt() == 3 & timeWeek == 0 & String(timeDay).toInt() > 24 & daylight_saving_state != 1)
  {
    daylightOffset_sec = 3600;
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    daylight_saving_state = 1;
  }

  strftime(timeSec, 3, "%S", &timeinfo);
  strftime(timeMin, 3, "%M", &timeinfo);
  strftime(timeHour, 3, "%H", &timeinfo);
  strftime(timeDay, 3, "%d", &timeinfo);
  strftime(timeWeek, 3, "%w", &timeinfo);
  strftime(timeMonth, 3, "%m", &timeinfo);
  strftime(timeYear, 5, "%Y", &timeinfo);
  strftime(timeZone, 4, "%z", &timeinfo);

  if (!getLocalTime(&timeinfo))
  {
    if (debug)
      Serial.println("Failed to obtain time");
    return;
  }

  ntp_time = String(timeYear) + "-";
  ntp_time += String(timeMonth) + "-";
  ntp_time += String(timeDay) + " ";
  ntp_time += String(timeHour) + ":";
  ntp_time += String(timeMin) + ":";
  ntp_time += String(timeSec) + " ";
  ntp_time += String(timeZone);
}

void IRAM_ATTR ISR()
{
  digitalWrite(Relays_Pin, LOW);
  if (debug)
    Serial.println("Emergency Stop Button Pressed!\nPlease restart device to continue.");
  while (1)
  {
  }
}

void repeatedCall()
{
  static int num = 0;
  unsigned long currentMillis = millis();
  if ((currentMillis - previousMillis) >= interval)
  {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    if (FirmwareVersionCheck())
    {
      firmwareUpdate();
    }
  }
  if ((currentMillis - previousMillis_2) >= mini_interval)
  {
    previousMillis_2 = currentMillis;
    if (WiFi.status() == WL_CONNECTED)
    {
      // if(debug)
      // Serial.println("wifi connected");
    }
    else
    {
      if (WiFi.status() != WL_CONNECTED)
      {
        // couldn't connect
        if (debug)
          Serial.println("[main] Couldn't connect to WiFi after multiple attempts");
        delay(5000);
        ESP.restart();
      }
      if (debug)
        Serial.println("Connected");
    }
  }
}

void firmwareUpdate(void)
{
  WiFiClientSecure OTA_client;
  OTA_client.setCACert(OTAcert);
  httpUpdate.setLedPin(2, LOW);
  t_httpUpdate_return ret = httpUpdate.update(OTA_client, URL_fw_Bin);

  switch (ret)
  {
  case HTTP_UPDATE_FAILED:
    if (debug)
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    if (debug)
      Serial.println("HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    if (debug)
      Serial.println("HTTP_UPDATE_OK");
    break;
  }
}

int FirmwareVersionCheck(void)
{
  if (debug)
    Serial.println(FirmwareVer);
  String payload;
  int httpCode;
  String fwurl = "";
  fwurl += URL_fw_Version;
  fwurl += "?";
  fwurl += String(rand());
  if (debug)
    Serial.println(fwurl);
  WiFiClientSecure *OTA_client = new WiFiClientSecure;

  if (OTA_client)
  {
    OTA_client->setCACert(OTAcert);

    // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
    HTTPClient https;

    if (https.begin(*OTA_client, fwurl))
    { // HTTPS
      if (debug)
        Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      delay(100);
      httpCode = https.GET();
      delay(100);
      if (httpCode == HTTP_CODE_OK) // if version received
      {
        payload = https.getString(); // save received version
      }
      else
      {
        if (debug)
        {
          Serial.print("error in downloading version file:");
          Serial.println(httpCode);
        }
      }
      https.end();
    }
    delete OTA_client;
  }

  if (httpCode == HTTP_CODE_OK) // if version received
  {
    payload.trim();
    if (payload.equals(FirmwareVer))
    {
      if (debug)
        Serial.printf("\nDevice already on latest firmware version:%s\n", FirmwareVer);
      return 0;
    }
    else
    {
      if (debug)
      {
        Serial.println(payload);
        Serial.println("New firmware detected");
      }
      return 1;
    }
  }
  return 0;
}

void saveParamsCallback()
{
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

int writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  EEPROM.commit();

  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
  EEPROM.commit();

  return addrOffset + 1 + len;
}

int readStringFromEEPROM(int addrOffset, String *strToRead)
{
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];

  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';

  *strToRead = String(data);
  return addrOffset + 1 + newStrLen;
}