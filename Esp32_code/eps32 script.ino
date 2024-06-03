#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <BH1750.h>
#include <RTClib.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h> // Aggiunta libreria per richieste HTTP

// Dichiarazioni delle costanti e delle variabili globali

TaskHandle_t bmpTaskHandle, bh1750TaskHandle, rtcTaskHandle;
WiFiClient espClient;
PubSubClient client(espClient);
const char *ssid = "TIM-63619464";
const char *password = "uc4DfzHJHd36CZFJ";
const char *mqttBroker = "192.168.1.12";
const int mqttPort = 1883;



//Costanti per il bot Telegram
const char *telegramBotToken = "6728633709:AAFXKIkfqvrAS2ublCwPKIJ5PIdrKqdgEps";
const char *telegramChatID = "364476654";

// Inizializzazione dei sensori
Adafruit_BMP280 bmp;
BH1750 lightSensor;
RTC_PCF8523 rtc;

// Prototipi delle funzioni
void connectToWiFi();
void connectToMQTT();
void subscribeToTopics();
void readBMP280Task(void *parameter);
void readBH1750Task(void *parameter);
void readRTCTask(void *parameter);
void sendTelegramMessage(const char *message);

void setup() {
  Serial.begin(115200);
  connectToWiFi();
  client.setServer(mqttBroker, mqttPort);

  if (!bmp.begin()) {
    Serial.println("Impossibile trovare un sensore valido BMP280, controlla il cablaggio!");
    while (1);
  }

  if (!lightSensor.begin()) {
    Serial.println("Impossibile trovare un sensore valido BH1750, controlla il cablaggio!");
    while (1);
  }

  if (!rtc.begin()) {
    Serial.println("Impossibile trovare un sensore valido RTC, controlla il cablaggio!");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  xTaskCreatePinnedToCore(readBMP280Task, "BMP280Task", 4096, NULL, 1, &bmpTaskHandle, 0);
  xTaskCreatePinnedToCore(readBH1750Task, "BH1750Task", 4096, NULL, 1, &bh1750TaskHandle, 0);
  xTaskCreatePinnedToCore(readRTCTask, "RTCTask", 4096, NULL, 1, &rtcTaskHandle, 0);

  subscribeToTopics();
}

void loop() {
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();
}

void connectToWiFi() {
  Serial.println("Connessione al WiFi..");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connettendosi al WiFi...");
  }
  Serial.println("Connesso al WiFi");
}

void connectToMQTT() {
  Serial.println("Connessione all MQTT");
  while (!client.connected()) {
    if (client.connect("ESP32Client")) {
      Serial.println("Connesso all MQTT Broker");
    } else {
      Serial.print("Fallita la connessione all MQTT, rc=");
      Serial.print(client.state());
      Serial.println(" Riprova in 5 secondi");
      delay(5000);
    }
  }
}

void subscribeToTopics() {
  char temperatureTopic[] = "temperatura";
  char pressureTopic[] = "pressione";
  char lightTopic[] = "luminositÃ ";
  char dateTopic[] = "data";
  char timeTopic[] = "ora";

  if (client.connected()) {
    client.subscribe(temperatureTopic);
    client.subscribe(pressureTopic);
    client.subscribe(lightTopic);
    client.subscribe(dateTopic);
    client.subscribe(timeTopic);
  }
}

// Funzione per inviare un messaggio a un bot Telegram
void sendTelegramMessage(const char *message) {
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(telegramBotToken) + "/sendMessage?chat_id=" + String(telegramChatID) + "&text=" + String(message);
  http.begin(url);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    Serial.print("Messaggio Telegram inviato. Response code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("Errore nell'inviare il messaggio Telegram. Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}


bool temperatureThresholdEnabled = true; // Variabile per tenere traccia dello stato del monitoraggio della soglia

void readBMP280Task(void *parameter) {
  (void)parameter;
  char temperatureTopic[] = "temperatura";
  char pressureTopic[] = "pressione";
  
  float temperatureThreshold = 5.0; //temperatura in C
  
  
  while (1) {
    // Leggi temperatura e pressione dal BMP280
    float temperature = bmp.readTemperature();
    float pressure = bmp.readPressure() / 100.0; // Pressione in hPa
    
    // Pubblica i dati del BMP280 su MQTT
    if (client.publish(temperatureTopic, String(temperature).c_str())) {
      Serial.print("Temperatura inviata all MQTT Broker: ");
      Serial.println(temperature);
      
      // Controllo della soglia solo se il monitoraggio della soglia Ã¨ abilitato
      if (temperatureThresholdEnabled && temperature > temperatureThreshold) {
        sendTelegramMessage("La temperatura Ã¨ superiore a 5 gradi Celsius!");
        temperatureThresholdEnabled = false; // Disattiva il monitoraggio della soglia
      }
      
      // Riattiva il monitoraggio della soglia se la temperatura scende sotto la soglia
      if (!temperatureThresholdEnabled && temperature <= temperatureThreshold) {
        temperatureThresholdEnabled = true;
      }
    } else {
      Serial.println("Fallito nell'inviare la temperatura al Broker MQTT");
    }
    
    if (client.publish(pressureTopic, String(pressure).c_str())) {
      Serial.print("Pressione inviata al Broke MQTT: ");
      Serial.println(pressure);
    } else {
      Serial.println("Fallito nell'inviare la pressione al Broker MQTT");
    }
    
    vTaskDelay(10000 / portTICK_PERIOD_MS); // Ritardo di 10 secondo
  }
}


bool lightThresholdEnabled = true; // Variabile per tenere traccia dello stato del monitoraggio della soglia


void readBH1750Task(void *parameter) {
  (void)parameter;
  char lightTopic[] = "luminositÃ ";
  
  float lightThreshold = 500; //luminositÃ  in Lumen

  while (1) {
    // Leggi l'intensitÃ  luminosa dal BH1750
    float lux = lightSensor.readLightLevel();
    // Pubblica i dati del BMP280 su MQTT
    if (client.publish(lightTopic, String(lux).c_str())) {
      Serial.print("LuminositÃ  inviata al Broke MQTT: ");
      Serial.println(lux);
      
      // Controllo della soglia solo se il monitoraggio della soglia Ã¨ abilitato
      if (lightThresholdEnabled && lux > lightThreshold) {
        sendTelegramMessage("La luminositÃ  Ã¨ superiore a 500 lumen!");
        lightThresholdEnabled = false; // Disattiva il monitoraggio della soglia
      }
      
      // Riattiva il monitoraggio della soglia se la temperatura scende sotto la soglia
      if (!lightThresholdEnabled && lux <= lightThreshold) {
        lightThresholdEnabled = true;
      }
    } else {
      Serial.println("Fallito nell'inviare la luminositÃ  al Broker MQTT");
    }
    
    vTaskDelay(10000 / portTICK_PERIOD_MS); // Ritardo di 10 secondo
  }
}

void readRTCTask(void *parameter) {
  (void)parameter;
  char dateTopic[] = "data";
  char timeTopic[] = "ora";
  while (1) {
    // Leggi l'ora corrente dal RTC
    DateTime now = rtc.now();
    // Pubblica i dati del RTC su MQTT
    if (client.publish(dateTopic, (String(now.year()) + "/" + String(now.month()) + "/" + String(now.day())).c_str())) {
      Serial.print("Data inviata al Broker MQTT: ");
      Serial.print(now.year());
      Serial.print("/");
      Serial.print(now.month());
      Serial.print("/");
      Serial.println(now.day());
    } else {
      Serial.println("Fallito nell'inviare la data al Broker MQTT");
    }
    if (client.publish(timeTopic, (String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second())).c_str())) {
      Serial.print("Ora inviata al Broker MQTT");
      Serial.print(now.hour());
      Serial.print(":");
      Serial.print(now.minute());
      Serial.print(":");
      Serial.println(now.second());
    } else {
      Serial.println("Fallito nell'inviare l'ora al Broker MQTT");
    }
    vTaskDelay(10000 / portTICK_PERIOD_MS); // Ritardo di 10 secondi
  }
}
