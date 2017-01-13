#include "DHT.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// Wi-Fi point
const char* ssid     = "MGBot";
const char* password = "Terminator812";

// For ThingWorx IoT
char iot_server[] = "jsciclpcompany5864.cloud.thingworx.com";
IPAddress iot_address(50, 19, 39, 46);
char appKey[] = "f6a2aee7-df44-4802-b41d-cf091d5acfed";
char thingName[] = "iotik_test_thing";
char serviceName[] = "apply_iotik_data";

// ThingWorx parameters
#define sensorCount 4
char* sensorNames[] = {"air_temp", "air_hum", "soil_temp", "soil_hum"};
float sensorValues[sensorCount];
// Номера датчиков
#define air_temp     0
#define air_hum      1
#define soil_temp    2
#define soil_hum     3

WiFiClientSecure client;

#define THINGWORX_UPDATE_TIME 1000     // Update ThingWorx data server
#define DS18B20_UPDATE_TIME 1000       // Update time for DS18B20 sensor
#define DHT22_UPDATE_TIME 1000         // Update time for DHT22 sensor
#define MOISTURE_UPDATE_TIME 1000      // Update time for moisture sensor

// DHT11 sensor
#define DHT22_PIN 4
DHT dht22(DHT22_PIN, DHT22, 15);

// DS18B20 sensor
#define ONE_WIRE_BUS 5
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// Moisture sensor
#define MOISTURE_PIN A0

// Data from sensors
float h1 = 0;
float t1 = 0;
float t2 = 0;
float m1 = 0;

// Timer counters
unsigned long timer_thingworx = 0;
unsigned long timer_ds18b20 = 0;
unsigned long timer_dht22 = 0;
unsigned long timer_moisture = 0;

#define TIMEOUT 1000 // 1 second timout

// Максимальное время ожидания ответа от сервера
#define IOT_TIMEOUT1 5000
#define IOT_TIMEOUT2 100

// Таймер ожидания прихода символов с сервера
long timer_iot_timeout = 0;

// Размер приемного буффера
#define BUFF_LENGTH 64

// Приемный буфер
char buff[BUFF_LENGTH] = "";

// Main setup
void setup()
{
  // Init serial port
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Init Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Init DHT22
  dht22.begin();

  // Init DS18B20
  ds18b20.begin();

  // Init moisture
  pinMode(MOISTURE_PIN, INPUT);

  // First measurement and print data from sensors
  readDHT22();
  readDS18B20();
  readMOISTURE();
  printAllSensors();
}

// Main loop cycle
void loop()
{
  // Send data to ThingWorx server
  if (millis() > timer_thingworx + THINGWORX_UPDATE_TIME)
  {
    sendThingWorxStream();
    timer_thingworx = millis();
  }

  // DHT22 sensor timeout
  if (millis() > timer_dht22 + DHT22_UPDATE_TIME)
  {
    readDHT22();
    timer_dht22 = millis();
  }

  // DS18B20 sensor timeout
  if (millis() > timer_ds18b20 + DS18B20_UPDATE_TIME)
  {
    readDS18B20();
    timer_ds18b20 = millis();
  }

  // Moisture sensor timeout
  if (millis() > timer_moisture + MOISTURE_UPDATE_TIME)
  {
    readMOISTURE();
    timer_moisture = millis();
  }
}

// Подключение к серверу IoT ThingWorx
void sendThingWorxStream()
{
  // Подключение к серверу
  Serial.println("Connecting to IoT server...");
  if (client.connect(iot_address, 443))
  {
    // Проверка установления соединения
    if (client.connected())
    {
      // Отправка заголовка сетевого пакета
      Serial.println("Sending data to IoT server...\n");
      Serial.print("POST /Thingworx/Things/");
      client.print("POST /Thingworx/Things/");
      Serial.print(thingName);
      client.print(thingName);
      Serial.print("/Services/");
      client.print("/Services/");
      Serial.print(serviceName);
      client.print(serviceName);
      Serial.print("?appKey=");
      client.print("?appKey=");
      Serial.print(appKey);
      client.print(appKey);
      Serial.print("&method=post&x-thingworx-session=true");
      client.print("&method=post&x-thingworx-session=true");
      // Отправка данных с датчиков
      for (int idx = 0; idx < sensorCount; idx ++)
      {
        Serial.print("&");
        client.print("&");
        Serial.print(sensorNames[idx]);
        client.print(sensorNames[idx]);
        Serial.print("=");
        client.print("=");
        Serial.print(sensorValues[idx]);
        client.print(sensorValues[idx]);
      }
      // Закрываем пакет
      Serial.println(" HTTP/1.1");
      client.println(" HTTP/1.1");
      Serial.println("Accept: application/json");
      client.println("Accept: application/json");
      Serial.print("Host: ");
      client.print("Host: ");
      Serial.println(iot_server);
      client.println(iot_server);
      Serial.println("Content-Type: application/json");
      client.println("Content-Type: application/json");
      Serial.println();
      client.println();

      // Ждем ответа от сервера
      timer_iot_timeout = millis();
      while ((client.available() == 0) && (millis() < timer_iot_timeout + IOT_TIMEOUT1));

      // Выводим ответ о сервера, и, если медленное соединение, ждем выход по таймауту
      int iii = 0;
      bool currentLineIsBlank = true;
      bool flagJSON = false;
      timer_iot_timeout = millis();
      while ((millis() < timer_iot_timeout + IOT_TIMEOUT2) && (client.connected()))
      {
        while (client.available() > 0)
        {
          char symb = client.read();
          Serial.print(symb);
          if (symb == '{')
          {
            flagJSON = true;
          }
          else if (symb == '}')
          {
            flagJSON = false;
          }
          if (flagJSON == true)
          {
            buff[iii] = symb;
            iii ++;
          }
          timer_iot_timeout = millis();
        }
      }
      buff[iii] = '}';
      buff[iii + 1] = '\0';
      Serial.println(buff);
      // Закрываем соединение
      client.stop();
      Serial.println("Packet successfully sent!");
      Serial.println();
    }
  }
}

// Print sensors data to terminal
void printAllSensors()
{
  Serial.print("Air temperature: ");
  Serial.print(t1);
  Serial.println(" *C");
  Serial.print("Air humidity: ");
  Serial.print(h1);
  Serial.println(" %");
  Serial.print("Soil temperature: ");
  Serial.print(t2);
  Serial.println(" *C");
  Serial.print("Soil moisture: ");
  Serial.print(m1);
  Serial.println(" %");
}

// Read DHT22 sensor
void readDHT22()
{
  h1 = dht22.readHumidity();
  t1 = dht22.readTemperature();
  sensorValues[air_hum] = h1;
  sensorValues[air_temp] = t1;
  if (isnan(h1) || isnan(t1))
  {
    Serial.println("Failed to read from DHT22 sensor!");
  }
}

// Read DS18B20 sensor
void readDS18B20()
{
  ds18b20.requestTemperatures();
  t2 = ds18b20.getTempCByIndex(0);
  sensorValues[soil_temp] = t2;
  if (isnan(t2))
  {
    Serial.println("Failed to read from DS18B20 sensor!");
  }
}

// Read MOISTURE sensor
void readMOISTURE()
{
  m1 = analogRead(MOISTURE_PIN) / 1023.0 * 100.0;
  sensorValues[soil_hum] = m1;
}

