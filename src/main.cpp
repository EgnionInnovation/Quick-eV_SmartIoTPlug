#include <Arduino.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <OTA_cert.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
// #include <TinyGSM.h>
// #include <ArduinoHttpClient.h>
#include "time.h"
#include <SoftwareSerial.h>
#include <FirebaseClient.h>

#define debugging true
#define Relays_Pin 32
#define Emergency_Button 12
void IRAM_ATTR ISR();

// ---------------------------------------------------------------------------------------- Device Parameters
String equipment_id, equipment_version, password_plain, password_cipher;
String mac_address, ip_address;
bool provisioning_required = false;
int is_active = 0, is_connected = 0;
int duration;
String relay_status = "OFF";
String firmware_version, hardware_version, region;
String password, encrypted_password, key, iv;
String internet_source = "WiFi";

// ---------------------------------------------------------------------------------------- EEPROM Parameters
int equipment_id_address = 1, equipment_version_address = 20;

// ---------------------------------------------------------------------------------------- WiFi Manager
#define CAPTIVE_PORTAL_TIMEOUT 30
WiFiManager wm;
// WiFiManagerParameter eq_id("1", "Equipment ID", "", 40);
WiFiManagerParameter eq_ver("2", "Equipment Version", "", 40);
String eq_id_str = "", eq_ver_str = "";
void saveParamsCallback();
int writeStringToEEPROM(int addrOffset, const String &strToWrite);
int readStringFromEEPROM(int addrOffset, String *strToRead);

// ---------------------------------------------------------------------------------------- OTA parameters
String FirmwareVer = {"1.21"};
#define URL_fw_Version "https://raw.githubusercontent.com/EgnionInnovation/Quick-eV_SmartIoTPlug/main/firmware_version.txt"
#define URL_fw_Bin "https://raw.githubusercontent.com/EgnionInnovation/Quick-eV_SmartIoTPlug/main/fw/firmware.bin"
void firmwareUpdate();
int FirmwareVersionCheck();

unsigned long previousMillis = 0; // will store last time LED was updated
unsigned long previousMillis_2 = 0;
const long interval = 60000;
const long mini_interval = 60000;

void repeatedCall();
void firmwareUpdate(void);
int FirmwareVersionCheck(void);

// ---------------------------------------------------------------------------------------- Serial with Fixed ESP
SoftwareSerial ESPFixed;
void send_data();
bool receive_data(unsigned long);
unsigned long send_data_time = millis(), SEND_DATA_INTERVAL = 1000;

// ---------------------------------------------------------------------------------------- Measured Properties' Variables
float kwh_total, kwh_L1, kwh_L2, kwh_L3;
float amps_L1, amps_L2, amps_L3;
float volts_L1, volts_L2, volts_l3;
float freq, power_factor, temperature;
unsigned long measure_power_time = millis(), MEASURE_POWER_INTERVAL = 10000;

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
// The API key can be obtained from Firebase console > Project Overview > Project settings.
// #define API_KEY "AIzaSyD4HIIFu-PHnXDRJolS1o61Hp7ELkAMjj0"

/**
 * This information can be taken from the service account JSON file.
 *
 * To download service account file, from the Firebase console, goto project settings,
 * select "Service accounts" tab and click at "Generate new private key" button
 */

#define DATABASE_URL "https://quick-ev-uat-default-rtdb.firebaseio.com"
#define FIREBASE_PROJECT_ID "quick-ev-uat"
#define FIREBASE_CLIENT_EMAIL "gen-2-firmware@quick-ev-uat.iam.gserviceaccount.com"
const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\nMIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCdN/pFBzozl1YY\n6+KjsLf78fdVy7gHHleTi7xLk/UtdgzF7bc4iA5QUW3pC2lvCz10HY4LKgCsDAu9\nRV8O0jc9ZEh2YtIzMh3LHVlCCTAJfTBDEL4iPGPTHVcX9aOL1qoQcdqAW23fDBlU\nsTPeudSgRKLyI5NdKETxUdV4aZYPA4boiLEKm8H2bGF3uNqa1KLEOEypXtxIJbnD\nnw02dx/mVi1IkQfFn/D8Z3XYAuoB/fvnrTOQOEAdWof5iLKTex2rD4wasNbhAaVz\n/ZGRBsbT4FfUSfTrQQgtTjFigKmSwG3Laa0r9EuhiUNz+Pxpz2iUfNs31EqCRi+l\nDj8fkRDfAgMBAAECggEAGVQ/62gXLeGnWtuCB4o8kggxJ27rRqZSSLCeFPUQ7pKS\nyz6Zoq900ubTlNSkV2IRtAfg7xaExjMonwUyo+IlSSxDamNQZzQfTa58R2HQje7P\n3DYx07U0Bfq3oeIOx8Q9YOne4IgaYvGBkT39U9hPk8SLFgS6RKtstA5RnJUyOle/\nuWnsMD1bsOUacQOK0LrsyWbokXJH8yKs31J69eIcbWziS+r6VwkTLTJk7pYEc+zv\nlPqabPdNy3abvuAQtvoa1/BfBds9zwJKzz9E1f5pK/74BQefUjsgGzi35a0Ex//6\n+zAJj0x0mJ72fpyrRbJPpD+K+8lo1CdcZ6Or/ro+AQKBgQDbUyaclVU3+WYq+AfL\noVbLsvjsbfUrXaOvNGZFQAljPIG1+gJdLX2Dfqh4Bfc7gVUABOtPQ6kNv5bDOjcg\nI+z1i4jgp0Zdb/glZAJPlH8LbMKg7tnJHCh6vDrtGnIdtFxMSBFP7HXvzUXwS5L5\nqipnli59IuO+qv07vfez2YvKAQKBgQC3gjBfl5UStGaYHB0/aUV0t9xFinhcwrrG\nwYX/UbmIvW/zDOrFKtxEY0rHJYNlCfqxPBxtnSWl7X8fmNU5ktl7GoOagnQ90Jyk\nBKHr/fqd+Hd1y86NQbUZkwxH8fY3d+jdiSO1VlU0MoqnQ9EMA4fsxVkdbBT185NG\n66gTnEga3wKBgQDFurU4vbjSedoOKwZ8IrxpcLTSEl/R9N24+viovg64lLgsI3U2\nI+jgP7QKYPZ/gx3qooSyNUGXz35QC4/fPgRHasDAKI5bdrK0ovEiZbITzr248R0P\nHn+wBzrov8rZ1NzROLfC4l+BDgNbnAapZyxLry3CS04fe3BKB/3k+t/4AQKBgQCF\npcl5NTqavswhaAhdEFxHX0iLVQfH9wJ0kqj2hByt29nWl8e8BTUakX36f/Wr9pKf\n1fmWU2cB63A0IOjZ33uIzoyeUPg7tN0AD2emKfkGZ9kI73/lHL+6en/tPelmCGyO\nO7zH6rJvK4gTva5YI5Iw/KGkbfzuC5Fti1+DuMRC3QKBgAFDPkN1TMrccnbbpRcT\nSjQYBomobmJB0IUToExt5HNpW+C0IJx6Uvf2cTXQl+UD+Q72x221vzSF+WrzaHLF\nTk8hYG5PR0CmaaIPpSTEdZu6YeYz69q/mI4YSSMCHhr2DKwNnk/NyeBERIvyr444\nbtb0tSY41Ids/l63ORPIdy2F\n-----END PRIVATE KEY-----\n";

// #define DATABASE_URL "https://test-f397b-default-rtdb.firebaseio.com/"
// #define FIREBASE_PROJECT_ID "test-f397b"
// #define FIREBASE_CLIENT_EMAIL "firebase-adminsdk-ocjj0@test-f397b.iam.gserviceaccount.com"
// const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\nMIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQC1jO/6SZPQ7drL\nlFSoHel/APro2naCKWGhpT8S7wQHaTxhT3H8kgeo4eoyIj0LjkP07jSTRfqqBL2/\nL0eOAg+mtdztrI8PnGE8e2V9exqcepEOp2Xt+geqNL7hhLCH7dm4sMbbG/K49cF1\nz7U/FqZSsLmRvopNwcCPZXo4o6HbPqknsHEvkK6bJb6GMKlmOsFVSG+4qMXhczR2\n5HAH0NZ3aFJqU8H1FtFnhUAYjHNf2hA2ClqAFolQI7XfuANh31v19v0ycVkQxx7c\nwPIoLYswX/vmnBHtn1D4+gAzbD92q3wPPhzQCRPJLB8encbMv1LN/eXbBWOioGgJ\nqX7uLl0jAgMBAAECggEAC0qUslBZLSld7kNvcHVLzGZXNJxBup6wP8lzPs42xe85\nniO+xyKd71b9pdiTS2CxwU3/Xxl/GYvp8TYTkTV3m7q73txYmXP0aDqUeVVqtki5\nNNwcbsyaJW/aX1RNVmbon5/+/imi0vYV7inY7++MsJ/lKrdbCrL+MuzwyQ0ESApm\nwu+JRAwMnWAbBxf5rJBy/236g3D1abJk7MJU6M4lgaDnqdHqLHenFgynGYarIIR+\n89wPZ8G//ivHP0tDZ+BXQNGh7DXQ8QVAN5Z5mBg4cYrZAgRyXZPZ95xMMkLTDsrB\n1zuktQ09ffVLK2pHJ0niPpvPtTP3MjFTcaiUkbEgAQKBgQDmQm3Qn7P9RDYC9SYn\nZRpph8KfSSd/3gvXK7ZV0L8XrSJjt8xKb9Dtkt8AzG0qMWZRKNHpX3HbBaNNCGOx\nk7+v3qWl3hIjzfDvbdbcemnIDiW9rsqSLgcbI1lIF6Ewd6Tpe8rDnPV8wLwPmaNe\nJnbNrsvnZnzPh9HYhi72fAjMAQKBgQDJ2I37iD4giyV4ScFaRtWBQ0ll04a/MGKt\nfPg8L1cwnCWxurh029Tdsb7LmKKi66ZGsRUQqv8jXhyVXdRGjdNanuytosVijhEl\nVN50OtcSqW6W2MNoAYcnzLFP8KTKDv19GMI8xGoaeZDmhmUE0GyZsQdkI2hlfgI1\n47JqqI55IwKBgGEy74pesCscbTRoafe9TR35KiX1SpBGmnb1Q94L5W7ILjkr8DgH\n5Yk0M6Dxqq9h9RATjDDYkoZjZeDxxqvCc+t4sDJJgRzOJYPcuROPNTI3DqV4sJhu\nh59kF59AIlIEX4AUOq7ChjpoXbq0H2tyDzqaLAb9k3hDnEirtA1mpIwBAoGBAK7V\n3E42+hF4VbF2uXt4BbHc1bPU4E+1GpRJvj9rhit95YyoPuRCEoUhVDHIeX+DfNiY\nxLVWWH+LIlkjGB8w9BT3uezBJBY1Fpbuh23IFcl9Z2RUSBZL1IVd4Wxr9mFrUJjO\nHFlEjN9301JKsS/VVWxfEhbkMKZQ2ptRKpcGf7pfAoGBAI9hwu/T0HuMYh/iT+R7\nO5Rb1GTewDxlvzdR9PJYDJQh7Gbu3ADflJ+F1AwZyt9oOaGxqPWLj7lbjtfGcR9X\nqeCdDNfy4UZH5pga81QQ8g4r/onCQXu1Zn4rSfedcp+97LVqe+Gw5g51QsMv88aV\nu2B3ph2ClZGgHZ3mMZoQo58h\n-----END PRIVATE KEY-----\n";

void timeStatusCB(uint32_t &ts);

void asyncCB(AsyncResult &aResult);

void printResult(AsyncResult &aResult);

unsigned long ms = 0;

DefaultNetwork network; // initilize with boolean parameter to enable/disable network reconnection

ServiceAuth sa_auth(timeStatusCB, FIREBASE_CLIENT_EMAIL, FIREBASE_PROJECT_ID, PRIVATE_KEY, 3000 /* expire period in seconds (<3600) */);

FirebaseApp app;
RealtimeDatabase Database;

#include <WiFiClientSecure.h>
WiFiClientSecure ssl_client, ssl1;

using AsyncClient = AsyncClientClass;

AsyncClient aClient(ssl_client, getNetwork(network)), client1(ssl1, getNetwork(network));
AsyncResult result1;

void printResult(AsyncResult &aResult);

unsigned long read_firebase_time = millis(), READ_FIREBASE_INTERVAL = 1000;
// ---------------------------------------------------------------------------------------- API Server
char *serverName = "www.staging.quick-ev.com";
WiFiClientSecure client;
bool client_connected = false;
const char *test_root_ca =
	"-----BEGIN CERTIFICATE-----\n"
	"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\n"
	"-----END CERTIFICATE-----\n";

	StaticJsonDocument<512> doc;
	char response[512];
	StaticJsonDocument<512> api_response;

void setup()
{
	delay(1000); // 1 second for settling
	// ---------------------------------------------------------------- Begin serial monitor, serial with fixed ESP and EEPROM
	Serial.begin(115200);
	ESPFixed.begin(9600, SWSERIAL_8N1, 34, 33, false);
	EEPROM.begin(150);

	// for (int xyz = 0; xyz < 150; xyz++)
	//     EEPROM.writeString(xyz, "");

	if (debugging)
	{
		Serial.println("Started!");
		Serial.println("Firmware Version: " + FirmwareVer);
	}
	// ---------------------------------------------------------------- WiFi Manager
	WiFi.mode(WIFI_AP); // explicitly set mode, esp defaults to STA+AP
	WiFi.enableAP(true);
	// wm.resetSettings();
	// wm.addParameter(&eq_id);
	wm.addParameter(&eq_ver);
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

	readStringFromEEPROM(equipment_id_address, &eq_id_str);
	readStringFromEEPROM(equipment_version_address, &eq_ver_str);

	equipment_id = eq_id_str;
	equipment_version = eq_ver_str;

	Serial.println(eq_id_str);
	Serial.println(equipment_version);

	// ---------------------------------------------------------------- API Server
	// client.setCACert(test_root_ca);
	client.setInsecure();

	readStringFromEEPROM(equipment_id_address, &equipment_id);
	Serial.println("\nEquipment ID: " + equipment_id + "\n");
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
			// char buf[80];
			// strcpy(buf, serverName);
			// strcat(buf, "/equipment/provisioning");
			char *provisioning_server_name = "https://staging.quick-ev.com/public/api/equipment/provisioning";

			// Your Domain name with URL path or IP address with path
			https.begin(client, provisioning_server_name);

			doc["mac_address"] = String(WiFi.macAddress());
			doc["ip_address"] = WiFi.localIP().toString();
			doc["firm_version"] = FirmwareVer;
			doc["hardware_version"] = equipment_version;
			doc["status"] = "online";
			doc["region"] = region;

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

			DeserializationError err = deserializeJson(api_response, payload);
			if (err)
			{
				if (debugging)
				{
					Serial.print(F("deserializeJson() failed with code "));
					Serial.println(err.f_str());
				}
			}
			else
			{
				String equipment_id = api_response["equipment_id"];

				writeStringToEEPROM(equipment_id_address, equipment_id);
				if (equipment_id != "")
				{
					provisioning_required = false;
					if (debugging)
					{
						Serial.println("\nInit API call successful.\n");
					}
				}
				else
				{
					if (debugging)
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
	Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
	ssl1.setInsecure();
	if (debugging)
		Serial.println("Initializing app...");

	ssl_client.setInsecure();

	// Initialize the FirebaseApp or auth task handler.
	// To deinitialize, use deinitializeApp(app).
	initializeApp(aClient, app, getAuth(sa_auth), asyncCB, "authTask");
	app.getApp<RealtimeDatabase>(Database);

	Database.url(DATABASE_URL);

	client1.setAsyncResult(result1);

	// ---------------------------------------------------------------- Pin Modes
	pinMode(Relays_Pin, OUTPUT);
	digitalWrite(Relays_Pin, LOW);
	pinMode(Emergency_Button, INPUT_PULLUP);
	attachInterrupt(Emergency_Button, ISR, FALLING);

	// ---------------------------------------------------------------- NTP Server
	configTime(0, 0, ntpServer);
	setenv("TZ", "PKT-5", 1);
	tzset();
	printLocalTime();
}

void loop()
{
	if (WiFi.isConnected())
		is_connected = 1;
	else
		is_connected = 0;

	wm.process();
	
	if (!relay_status)
		repeatedCall();

	Database.loop();
	printResult(result1);
	JWT.loop(app.getAuth());
	app.loop();

	if (millis() > read_firebase_time + READ_FIREBASE_INTERVAL)
	{
		relay_status = Database.get<String>(client1, ("/relay/" + equipment_id + "/relay_status"));

		if (debugging)
		{
			Serial.println("Relay Status: " + String(relay_status));
		}
		read_firebase_time = millis();
	}

	if (relay_status == "OFF")
		digitalWrite(Relays_Pin, LOW);
	else if (relay_status == "ON")
		digitalWrite(Relays_Pin, HIGH);

	if (millis() > measure_power_time + MEASURE_POWER_INTERVAL)
	{
		if (WiFi.status() == WL_CONNECTED)
		{
			printLocalTime();

			HTTPClient https;
			// char buf[80];
			// strcpy(buf, serverName);
			// strcat(buf, "/equipment/provisioning");
			char *provisioning_server_name = "https://staging.quick-ev.com/public/api/equipment/readings/detailed";

			// Your Domain name with URL path or IP address with path
			https.begin(client, provisioning_server_name);

			doc["equipment_id"] = equipment_id;
			doc["total_kwh_consumed"] = String(kwh_total);
			doc["kwh_ln1"] = String(kwh_L1,2);
			doc["kwh_ln2"] = String(kwh_L2,2);
			doc["kwh_ln3"] = String(kwh_L3,2);
			doc["amps_ln1"] = String(amps_L1,2);
			doc["amps_ln2"] = String(amps_L2,2);
			doc["amps_ln3"] = String(amps_L3,2);
			doc["volts_ln1"] = String(volts_L1,2);
			doc["volts_ln2"] = String(volts_L2,2);
			doc["volts_ln3"] = String(volts_l3,2);
			doc["frequency"] = String(freq,2);
			doc["power_factor"] = String(power_factor,2);
			doc["temp"] = String(temperature,2);
			doc["relay_status"] = relay_status;
			doc["timestamp"] = ntp_time;
			doc["firmware_version"] = FirmwareVer;
			doc["equipment_version"] = equipment_version;
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

			DeserializationError err = deserializeJson(api_response, payload);
			if (err)
			{
				if (debugging)
				{
					Serial.print(F("deserializeJson() failed with code "));
					Serial.println(err.f_str());
				}
			}
			else
			{
				Serial.println("\nReadings API call successful\n");
			}

			// Free resources
			https.end();
		}
		else
		{
			Serial.println("WiFi Disconnected");
		}

		measure_power_time = millis();
	}

	if (ESPFixed.available())
	{
		Serial.println("Receiving Data...");
		receive_data(500);
	}

	if (millis() > send_data_time + SEND_DATA_INTERVAL)
	{
		if (debugging)
		{
			Serial.println("Sending data to Fixed ESP.");
		}
		send_data();
		send_data_time = millis();
	}
}

// void send_data()
// {
//     ESPFixed.print(relay_status + "," + String(is_connected));
// }

// void receive_data()
// {
//     String dat = ESPFixed.readString();
//     int c = 0;
//     String val_read = "";

//     while (dat.charAt(c) != ',')
//     {
//         val_read += dat.charAt(c);
//         c++;
//     }
//     kwh_total = val_read.toFloat();
//     if (debugging)
//     {
//         Serial.print("kWh_Total: ");
//         Serial.println(kwh_total);
//     }
//     c++;
//     val_read = "";

//     while (dat.charAt(c) != ',')
//     {
//         val_read += dat.charAt(c);
//         c++;
//     }
//     kwh_L1 = val_read.toFloat();
//     if (debugging)
//     {
//         Serial.print("kWh_L1: ");
//         Serial.println(kwh_L1);
//     }
//     c++;
//     val_read = "";

//     while (dat.charAt(c) != ',')
//     {
//         val_read += dat.charAt(c);
//         c++;
//     }
//     kwh_L2 = val_read.toFloat();
//     if (debugging)
//     {
//         Serial.print("kWh_L2: ");
//         Serial.println(kwh_L2);
//     }
//     c++;
//     val_read = "";

//     while (dat.charAt(c) != ',')
//     {
//         val_read += dat.charAt(c);
//         c++;
//     }
//     kwh_L3 = val_read.toFloat();
//     if (debugging)
//     {
//         Serial.print("kWh_L3: ");
//         Serial.println(kwh_L3);
//     }
//     c++;
//     val_read = "";

//     while (dat.charAt(c) != ',')
//     {
//         val_read += dat.charAt(c);
//         c++;
//     }
//     amps_L1 = val_read.toFloat();
//     if (debugging)
//     {
//         Serial.print("Current_L1: ");
//         Serial.println(amps_L1);
//     }
//     c++;
//     val_read = "";

//     while (dat.charAt(c) != ',')
//     {
//         val_read += dat.charAt(c);
//         c++;
//     }
//     amps_L2 = val_read.toFloat();
//     if (debugging)
//     {
//         Serial.print("Current_L2: ");
//         Serial.println(amps_L2);
//     }
//     c++;
//     val_read = "";

//     while (dat.charAt(c) != ',')
//     {
//         val_read += dat.charAt(c);
//         c++;
//     }
//     amps_L3 = val_read.toFloat();
//     if (debugging)
//     {
//         Serial.print("Current_L3: ");
//         Serial.println(amps_L3);
//     }
//     c++;
//     val_read = "";

//     while (dat.charAt(c) != ',')
//     {
//         val_read += dat.charAt(c);
//         c++;
//     }
//     volts_L1 = val_read.toFloat();
//     if (debugging)
//     {
//         Serial.print("Voltage_L1: ");
//         Serial.println(volts_L1);
//     }
//     c++;
//     val_read = "";

//     while (dat.charAt(c) != ',')
//     {
//         val_read += dat.charAt(c);
//         c++;
//     }
//     volts_L2 = val_read.toFloat();
//     if (debugging)
//     {
//         Serial.print("Voltage_L2: ");
//         Serial.println(volts_L2);
//     }
//     c++;
//     val_read = "";

//     while (dat.charAt(c) != ',')
//     {
//         val_read += dat.charAt(c);
//         c++;
//     }
//     volts_l3 = val_read.toFloat();
//     if (debugging)
//     {
//         Serial.print("Voltage_L3: ");
//         Serial.println(volts_l3);
//     }
//     c++;
//     val_read = "";

//     while (dat.charAt(c) != ',')
//     {
//         val_read += dat.charAt(c);
//         c++;
//     }
//     freq = val_read.toFloat();
//     if (debugging)
//     {
//         Serial.print("Frequency: ");
//         Serial.println(freq);
//     }
//     c++;
//     val_read = "";

//     while (dat.charAt(c) != ',')
//     {
//         val_read += dat.charAt(c);
//         c++;
//     }
//     power_factor = val_read.toFloat();
//     if (debugging)
//     {
//         Serial.print("Power Factor: ");
//         Serial.println(power_factor);
//     }
//     c++;
//     val_read = "";

//     while (dat.charAt(c) != '\0')
//     {
//         val_read += dat.charAt(c);
//         c++;
//     }
//     temperature = val_read.toFloat();
//     if (debugging)
//     {
//         Serial.print("Temperature: ");
//         Serial.println(temperature);
//     }
// }

void send_data()
{
	String data = String("@") + equipment_id + "," + relay_status + "," + String(is_connected) + "," + internet_source + "," + ntp_time.substring(12,17) + "#";
	ESPFixed.print(data);
}

bool receive_data(unsigned long timeout = 500)
{
	unsigned long startTime = millis();
	String dat = "";
	bool started = false;

	while ((millis() - startTime) < timeout)
	{
		if (ESPFixed.available())
		{
			char c = ESPFixed.read();

			if (c == '@')
			{
				dat = "";		// reset buffer
				started = true; // start receiving
			}
			else if (c == '#' && started)
			{
				break; // end of message
			}
			else if (started)
			{
				dat += c;
			}
		}
	}

	if (!started || dat.length() == 0)
	{
		if (debugging)
			Serial.println("Timeout or no data received.");
		return false;
	}

	// Split the string by commas
	int index = 0;
	float values[13]; // Adjust if number of fields change
	String val = "";
	int valCount = 0;

	for (unsigned int i = 0; i <= dat.length(); i++)
	{
		if (dat.charAt(i) == ',' || i == dat.length())
		{
			values[valCount++] = val.toFloat();
			val = "";
		}
		else
		{
			val += dat.charAt(i);
		}
	}

	if (valCount != 13)
	{
		if (debugging)
			Serial.println("Malformed packet. Incorrect field count.");
		return false;
	}

	// Assign to your variables
	kwh_total = values[0];
	kwh_L1 = values[1];
	kwh_L2 = values[2];
	kwh_L3 = values[3];
	amps_L1 = values[4];
	amps_L2 = values[5];
	amps_L3 = values[6];
	volts_L1 = values[7];
	volts_L2 = values[8];
	volts_l3 = values[9];
	freq = values[10];
	power_factor = values[11];
	temperature = values[12];

	if (debugging)
	{
		Serial.printf("kWh Total: %.2f, L1: %.2f, L2: %.2f, L3: %.2f\n", kwh_total, kwh_L1, kwh_L2, kwh_L3);
		Serial.printf("Amps L1: %.2f, L2: %.2f, L3: %.2f\n", amps_L1, amps_L2, amps_L3);
		Serial.printf("Volts L1: %.2f, L2: %.2f, L3: %.2f\n", volts_L1, volts_L2, volts_l3);
		Serial.printf("Freq: %.2f Hz, PF: %.2f, Temp: %.2f C\n", freq, power_factor, temperature);
	}

	return true;
}

void timeStatusCB(uint32_t &ts)
{
#if defined(ESP8266) || defined(ESP32) || defined(CORE_ARDUINO_PICO)
	if (time(nullptr) < FIREBASE_DEFAULT_TS)
	{

		configTime(3 * 3600, 0, "pool.ntp.org");
		while (time(nullptr) < FIREBASE_DEFAULT_TS)
		{
			delay(100);
		}
	}
	ts = time(nullptr);
#elif __has_include(<WiFiNINA.h>) || __has_include(<WiFi101.h>)
	ts = WiFi.getTime();
#endif
}

void asyncCB(AsyncResult &aResult)
{
	// WARNING!
	// Do not put your codes inside the callback and printResult.

	printResult(aResult);
}

void printResult(AsyncResult &aResult)
{
	if (aResult.isEvent())
		Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());

	if (aResult.isDebug())
		Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());

	if (aResult.isError())
		Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());

	if (aResult.available())
		Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
}

void printLocalTime()
{
    if (!getLocalTime(&timeinfo)) {
        if (debugging)
            Serial.println("Failed to obtain time");
        return;
    }

    // Buffers
    char dateTimeBuffer[40];
    char zoneBuffer[6];
    char millisBuffer[5];

    // Format: 2023-Oct-23 05:12:41
    strftime(dateTimeBuffer, sizeof(dateTimeBuffer), "%Y-%b-%d %H:%M:%S", &timeinfo);

    // Timezone abbreviation: CET / CEST
    strftime(zoneBuffer, sizeof(zoneBuffer), "%Z", &timeinfo);

    // Milliseconds padded to 3 digits
    unsigned long ms = millis() % 1000;
    sprintf(millisBuffer, "%03lu", ms);

    // Final timestamp string
    ntp_time = String(dateTimeBuffer) + "." + millisBuffer + " " + String(zoneBuffer);

    if (debugging)
        Serial.println(ntp_time);
}

void IRAM_ATTR ISR()
{
	digitalWrite(Relays_Pin, LOW);
	if (debugging)
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
			// if(debugging)
			// Serial.println("wifi connected");
		}
		else
		{
			if (WiFi.status() != WL_CONNECTED)
			{
				// couldn't connect
				if (debugging)
					Serial.println("[main] Couldn't connect to WiFi after multiple attempts");
				delay(5000);
				ESP.restart();
			}
			if (debugging)
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
		if (debugging)
			Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
		break;

	case HTTP_UPDATE_NO_UPDATES:
		if (debugging)
			Serial.println("HTTP_UPDATE_NO_UPDATES");
		break;

	case HTTP_UPDATE_OK:
		if (debugging)
			Serial.println("HTTP_UPDATE_OK");
		break;
	}
}

int FirmwareVersionCheck(void)
{
	if (debugging)
		Serial.println(FirmwareVer);
	String payload;
	int httpCode;
	String fwurl = "";
	fwurl += URL_fw_Version;
	fwurl += "?";
	fwurl += String(rand());
	if (debugging)
		Serial.println(fwurl);
	WiFiClientSecure *OTA_client = new WiFiClientSecure;

	if (OTA_client)
	{
		OTA_client->setCACert(OTAcert);

		// Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
		HTTPClient https;

		if (https.begin(*OTA_client, fwurl))
		{ // HTTPS
			if (debugging)
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
				if (debugging)
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
			if (debugging)
				Serial.printf("\nDevice already on latest firmware version:%s\n", FirmwareVer);
			return 0;
		}
		else
		{
			if (debugging)
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

	// Serial.print(eq_id.getID());
	// Serial.print(" : ");
	// eq_id_str = String(eq_id.getValue());
	// Serial.println(eq_id_str);
	// writeStringToEEPROM(equipment_id_address, eq_id_str);

	Serial.print(eq_ver.getID());
	Serial.print(" : ");
	eq_ver_str = String(eq_ver.getValue());
	Serial.println(eq_ver_str);
	writeStringToEEPROM(equipment_version_address, eq_ver_str);
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