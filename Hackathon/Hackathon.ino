/*
  DHTServer - ESP8266 with a DHT sensor as an input
*/

#include <ESP8266WiFi.h>
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

/************************* WiFi Access Point *********************************/
const char* ssid     = "Hackathon";
const char* password = "hackathon";

/************************* MQTT Server *********************************/
char* mqtt_server           = "192.168.1.140";
int mqtt_server_port        = 1883;
const char* mqtt_user       = "admin";
const char* mqtt_password   = "manager1";
String      message         = "";
String      topicTemp       = "";
String      topicHumid      = "";
String      topicVoltage    = "";
String      topicAggregate  = "";

/************************* ESP8266 WiFiClient *********************************/
WiFiClient wifiClient;

/************************* Prototypes *********************************/
void callback(char* topic, byte* payload, unsigned int length);


/************************* MQTT client *********************************/
PubSubClient client(mqtt_server, mqtt_server_port, callback, wifiClient );

/************************* DHT Sensor *********************************/
DHT dht(DHTPIN, DHTTYPE, 11);


float         humidity, temp_f;           // Values read from sensor
int           voltage;                    // ESP voltage

unsigned long previousMillis = 0;         // will store last temp was read
const long    interval = 2000;            // interval at which to read sensor

unsigned long count = 0;                  // counter for messagepoints

void lightOn() {
  digitalWrite(BUILTIN_LED, LOW); 
}

void lightOff() {
 digitalWrite(BUILTIN_LED, HIGH);  
}

void blink(int count) {
  for(unsigned i = 1; i <= count; ++i) {
      lightOn();
      delay(500);
      lightOff();
      delay(500);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String text = ((char*)payload);
  
  if ( strstr((char*)payload,"on") != NULL ) {
    Serial.println("on");
    lightOn();
  } else if ( strstr((char*)payload,"off") != NULL ) {
    Serial.println("off");
    lightOff();
  } else if ( strstr((char*)payload,"blink") != NULL ) {
    Serial.println("blink");
    blink(10);
  } else
    lightOff();
}

/************* Utility function to retrieve data from DHT22 ******************************/
void gettemperature() {
  // Wait at least 2 seconds seconds between measurements.
  // if the difference between the current time and last time you read
  // the sensor is bigger than the interval you set, read the sensor
  // Works better than delay for things happening elsewhere also
  unsigned long currentMillis = millis();

  if(currentMillis - previousMillis >= interval) {
    // save the last time you read the sensor
    previousMillis = currentMillis;

    // Reading temperature for humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
    humidity = dht.readHumidity();          // Read humidity (percent)
    temp_f = dht.readTemperature();         // Read temperature as Celsius
    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temp_f)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }
  }
}

/************* Functionname says it all! ******************************/
void setup(void) {

  Serial.begin(115200);

  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output

  lightOff();                       // Turn off LED
  
  // Create String for MQTT Topics
  topicTemp       = "iotdemo/temperature/"+ String( ESP.getChipId() );
  topicHumid      = "iotdemo/humidity/"+ String( ESP.getChipId() );
  topicVoltage    = "iotdemo/voltage/"+ String( ESP.getChipId() );
  topicAggregate  = "sensor.receiver/" + String( ESP.getChipId() );

  dht.begin();

  Serial.print("Chip-ID =");
  Serial.print ( ESP.getChipId() );

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.print("\n\r \n\rConnecting to ");
  Serial.println(ssid);


  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n\rESP8266 & DHT22 based temperature and humidity sensor working!");
  Serial.print("\n\rIP address: ");
  Serial.println(WiFi.localIP());
}

/******* Utility function to connect or re-connect to MQTT-Server ********************/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection to ");
    Serial.print(mqtt_server);
    Serial.print(" with ");
    Serial.print(mqtt_user);
    Serial.print(" / ");
    Serial.print(mqtt_password);
    Serial.print("\n");

    // Attempt to connect
    if (client.connect(String( ESP.getChipId() ).c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");

      // subscribe to topic
      if (client.subscribe("iotdemocommand/light")){
        Serial.println("Successfully subscribed");
      }
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/************* Functionname says it all! ******************************/
void loop(void) {


  if (!client.connected()) {  // Connect to mqtt broker
    reconnect();
  }
  client.loop();

  gettemperature();           // read sensordata
 
  voltage = readvdd33();

  // Now we can publish stuff!
  message = String((int)temp_f) + ", " + String(count);

  Serial.print(F("\nSending temperature value in Celsius <"));
  Serial.print(message);
  Serial.print(">");
  client.publish(topicTemp.c_str(), message.c_str());

  message = String((int)humidity) + ", " + String(count);

  Serial.print(F("\nSending humidity value <"));
  Serial.print(message);
  Serial.print(">");
  client.publish(topicHumid.c_str(), message.c_str());

  message = String((int)voltage) + ", " + String(count);

  Serial.print(F("\nSending sensor voltage <"));
  Serial.print(message);
  Serial.print(">");
  client.publish(topicVoltage.c_str(), message.c_str());

  message = String((int)temp_f) + "," + String((int)humidity) + "," + String((int)voltage);
  Serial.print(F("\nSending sensor aggregated data <"));
  Serial.print(message);
  Serial.print(">");
  client.publish(topicAggregate.c_str(), message.c_str());

  count = count + 1;           // increase counter

  delay(20000);
}

