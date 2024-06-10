#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

#define DHT_PIN 8   
#define DHTTYPE DHT11          
#define DS18B20_PIN 4        
#define soilMoisturePIN A0  
#define photoresistorPIN A2 
#define RX_PIN 10
#define TX_PIN 11
#define DEBUG true   

DHT dht(DHT_PIN, DHTTYPE);
OneWire oneWire(DS18B20_PIN);
DallasTemperature DS18B20(&oneWire);
SoftwareSerial esp8266(RX_PIN, TX_PIN);

String dht11TandH = "";
String ds18b20Temp = "";
String soilMoist = "";
String photoResistor = "";

void setup() {
  Serial.begin(9600);
  dht.begin();
  DS18B20.begin();
  esp8266.begin(115200);
  delay(1000);
  InitWifiModule();
  delay(2000);
}

void loop() {
  if (esp8266.available()) {    
    if (esp8266.find("+IPD,")) {
      delay(1000);
      int connectionId = esp8266.read() - 48;  
      String request = esp8266.readStringUntil('\r');
      
      dht11TandH = dht11Read();
      ds18b20Temp = ds18b20Read();
      soilMoist = soilMoistureRead();
      photoResistor = photoResistorRead();

        if (request.indexOf("/") != -1) {
    StaticJsonDocument<500> doc;
    doc["temperature"] = getValue(dht11TandH, ':', 0);
    doc["humidity"] = getValue(dht11TandH, ':', 1);
    doc["ds18b20Temperature"] = ds18b20Temp;
    doc["soilMoisture"] = soilMoist;
    doc["photoresistorValue"] = photoResistor;
    String response;
    serializeJson(doc, response);
    sendDataToClient(connectionId, response);
  }
      
      String closeCommand = "AT+CIPCLOSE="; 
      closeCommand += connectionId; 
      closeCommand += "\r\n";    
      sendData(closeCommand, 3000, DEBUG);
    }
  }
}

void sendDataToClient(int connectionId, String data) {
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: application/json\r\n";
  response += "Connection: close\r\n\r\n";
  response += data;
  
  String cipSend = "AT+CIPSEND=";
  cipSend += connectionId;
  cipSend += ",";
  cipSend += response.length();
  cipSend += "\r\n";
  sendData(cipSend, 1000, DEBUG);
  
  sendData(response, 1000, DEBUG);
}

String sendData(String command, const int timeout, bool debug) {
  String response = "";                                             
  esp8266.print(command);                                          
  long int time = millis();                                      
  while ((time + timeout) > millis()) {      
    while (esp8266.available()) {                                      
      char c = esp8266.read();                                     
      response += c;                                                  
    }  
  }    
  if (debug) {                                                      
    Serial.print(response);
  }    
  return response;                                                  
}

void InitWifiModule() {
  sendData("AT+RST\r\n", 2000, DEBUG);                                                  
  sendData("AT+CWJAP=\"SSID\",\"PASSWORD\"\r\n", 2000, DEBUG);        
  delay(3000);
  sendData("AT+CWMODE=1\r\n", 1500, DEBUG);                                             
  delay(1500);
  sendData("AT+CIFSR\r\n", 1500, DEBUG);                                             
  delay(1500);
  sendData("AT+CIPMUX=1\r\n", 1500, DEBUG);                                             
  delay(1500);
  sendData("AT+CIPSERVER=1,80\r\n", 1500, DEBUG);                                     
}


// Функція для зчитування температури і вологості з датчика DHT11
String dht11Read() {
  char temperature_c[6];
  char humidity_c[6];
  float h = dht.readHumidity();   // Зчитуємо вологість з датчика
  float t = dht.readTemperature();  // Зчитуємо температуру з датчика
  if (isnan(h) || isnan(t)) {
    return ""; 
  }
  dtostrf(t, 0, 1, temperature_c); 
  dtostrf(h, 0, 1, humidity_c);    
  return (String)temperature_c + ":" + (String)humidity_c;
}

// Функція для зчитування температури з датчика DS18B20
String ds18b20Read() {
  DS18B20.requestTemperatures();
  float temperature = DS18B20.getTempCByIndex(0); // Отримуємо температуру
  if (temperature == -127.0) {
    return ""; 
  }
  char temperature_c[6];
  dtostrf(temperature, 0, 1, temperature_c); 
  return (String)temperature_c;
}

// Функція для зчитування вологості ґрунту
String soilMoistureRead() {
    float moisture = analogRead(soilMoisturePIN); // Зчитуємо вологість з датчика
    if (isnan(moisture)) {
      return ""; 
    }
    moisture = 100*(1-(moisture)/1023);  // Конвертуємо отримане значення у відсоткове значення
    char moisture_c[6];
    dtostrf(moisture, 0, 1, moisture_c); 
    return (String) moisture_c;
}

// Функція для зчитування рівня освітленості
String photoResistorRead() {
    int sensorValue = analogRead(photoresistorPIN); // Зчитуємо значення фоторезистора
    if (sensorValue == -1) {
      return ""; 
    }
    char percentage_c[4]; 
    int percentage = map(sensorValue, 0, 1023, 0, 100); 
    sprintf(percentage_c, "%d", percentage); 
    return (String)percentage_c;
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
