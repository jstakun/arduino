#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#include <WiFiClient.h>
#include <DHT.h>
#include <PubSubClient.h>


extern "C" {
#include "user_interface.h"
  uint16 readvdd33(void);
}

/************************* DHT22 Sensor *********************************/
#define DHTTYPE DHT22
#define DHTPIN  12

/************************* ESP8266 WiFiClient *********************************/
WiFiClient wifiClient;

/************************* DHT Sensor *********************************/
DHT dht(DHTPIN, DHTTYPE, 11);

/************************* MQTT client *********************************/
PubSubClient mqttClient(wifiClient);

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "1883";
char mqtt_username[34];
char mqtt_password[34];

//flag for saving data
bool shouldSaveConfig = false;

String topicAggregate  = "";

String      message    = "";

float humidity, temp_f;           // Values read from sensor
int   voltage;

unsigned long previousMillis = 0;         // will store last temp was read
const long    interval = 2000;            // interval at which to read sensor
unsigned long count = 0;                  // counter for messagepoints

const char* apPwd = "sensor123";

/************* Utility function to retrieve data from DHT22 ******************************/
void gettemperature() {
  // Wait at least 2 seconds seconds between measurements.
  // if the difference between the current time and last time you read
  // the sensor is bigger than the interval you set, read the sensor
  // Works better than delay for things happening elsewhere also
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you read the sensor
    previousMillis = currentMillis;

    // Reading temperature for humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
    humidity = dht.readHumidity();          // Read humidity (percent)
    temp_f = dht.readTemperature();         // Read temperature as Celsius
    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temp_f)) {
      Serial.println("Failed to read from DHT sensor!");
      humidity = 0;
      temp_f = 0;
      return;
    }
  }
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_username, json["mqtt_username"]);
          strcpy(mqtt_password, json["mqtt_password"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read



  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
  WiFiManagerParameter custom_mqtt_username("username", "mqtt_username", mqtt_username, 32);
  WiFiManagerParameter custom_mqtt_password("password", "mqtt_password", mqtt_password, 32);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_username);
  wifiManager.addParameter(&custom_mqtt_password);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point
  //and goes into a blocking loop awaiting configuration
  String apId = "AP" + String( ESP.getChipId() );

  if (!wifiManager.autoConnect(apId.c_str(), apPwd)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected to wifi!");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_username, custom_mqtt_username.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_username"] = mqtt_username;
    json["mqtt_password"] = mqtt_password;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  topicAggregate  = "sensor.receiver/" + String( ESP.getChipId() );

  dht.begin();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  mqttClient.setServer(mqtt_server, atoi(mqtt_port));

  Serial.println("\n\rESP8266 & DHT22 based temperature and humidity sensor working!");
  Serial.print("\n\rIP address: ");
  Serial.println(WiFi.localIP());

}

/******* Utility function to connect or re-connect to MQTT-Server ********************/
void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection to ");
    Serial.print(mqtt_server);
    Serial.print(" with ");
    Serial.print(mqtt_username);
    Serial.print(" / ");
    Serial.print(mqtt_password);
    Serial.print("\n");

    // Attempt to connect
    if (mqttClient.connect(String( ESP.getChipId() ).c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");

      // subscribe to topic
      //if (mqttClient.subscribe("iotdemocommand/light")){
      //  Serial.println("Successfully subscribed");
      //}

    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/************* Functionname says it all! ******************************/
void loop(void) {


  if (!mqttClient.connected()) {  // Connect to mqtt broker
    reconnect();
  }
  mqttClient.loop();

  gettemperature();           // read sensordata

  voltage = readvdd33();

  message = String((int)temp_f) + "," + String((int)humidity) + "," + String((int)voltage);
  Serial.print(F("\nSending sensor aggregated data <"));
  Serial.print(message);
  Serial.print(">");
  mqttClient.publish(topicAggregate.c_str(), message.c_str());

  count = count + 1;           // increase counter

  delay(10000);
}


