#include "arduino_secrets.h"

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <BH1750.h>
#include <RTClib.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

// Dichiarazioni delle costanti e delle variabili globali
TaskHandle_t sensorTaskHandle, mqttTaskHandle, syncTimeTaskHandle;
QueueHandle_t dataQueue;
WiFiClient espClient;
PubSubClient client(espClient);
const char *ssid = "TIM-63619464";
const char *password = "uc4DfzHJHd36CZFJ";
const char *mqttBroker = "192.168.1.12";
const int mqttPort = 1883;

// Costanti per i topic MQTT
const char *temperatureTopic = "temperatura";
const char *pressureTopic = "pressione";
const char *lightTopic = "luminositÃ ";
const char *dateTopic = "data";
const char *timeTopic = "ora";

// Costanti per il bot Telegram
const char *telegramBotToken = "6728633709:AAFXKIkfqvrAS2ublCwPKIJ5PIdrKqdgEps";
std::vector<String> chatIDs; // Vettore per memorizzare gli ID chat

// Inizializzazione dei sensori
Adafruit_BMP280 bmp;
BH1750 lightSensor;
RTC_PCF8523 rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);

struct SensorData {
  float temperature;
  float pressure;
  float light;
  char date[11];
  char time[9];
};

// Prototipi delle funzioni
void connectToWiFi();
void connectToMQTT();
void subscribeToTopics();
void sensorTask(void *parameter);
void mqttTask(void *parameter);
void syncTimeTask(void *parameter);
void sendTelegramMessage(const char *message);
void syncTimeWithNTP();
void handleTelegramMessages();
void addChatID(const String& chatID);

// Variabili e costanti per il controllo della soglia e l'isteresi
bool temperatureThresholdEnabled = true; // Variabile per tenere traccia dello stato del monitoraggio della soglia
const float temperatureThresholdHigh = 5.5; // Soglia alta della temperatura in C
const float temperatureThresholdLow = 4.5; // Soglia bassa della temperatura in C

bool lightThresholdEnabled = true; // Variabile per tenere traccia dello stato del monitoraggio della soglia
const float lightThresholdHigh = 530.0; // Soglia alta della luminositÃ  in Lumen
const float lightThresholdLow = 450.0; // Soglia bassa della luminositÃ  in Lumen

bool pressureThresholdEnabled = true; // Variabile per tenere traccia dello stato del monitoraggio della soglia
const float pressureThresholdHigh = 1022.0; // Soglia alta della pressione in hPa
const float pressureThresholdLow = 1015.0; // Soglia bassa della pressione in hPa

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

  syncTimeWithNTP();

  // Creazione della coda
  dataQueue = xQueueCreate(10, sizeof(SensorData));

  xTaskCreatePinnedToCore(sensorTask, "SensorTask", 4096, NULL, 1, &sensorTaskHandle, 0);
  xTaskCreatePinnedToCore(mqttTask, "MQTTTask", 4096, NULL, 1, &mqttTaskHandle, 1);
  xTaskCreatePinnedToCore(syncTimeTask, "SyncTimeTask", 4096, NULL, 1, &syncTimeTaskHandle, 1);

  subscribeToTopics();
}

void loop() {
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();
  handleTelegramMessages(); // Gestisci i messaggi in entrata di Telegram
}

void connectToWiFi() {
  Serial.println("Connessione al WiFi..");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connettendosi al WiFi...");
  }
  Serial.println("Connesso al WiFi");
  timeClient.begin();
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
  if (client.connected()) {
    client.subscribe(temperatureTopic);
    client.subscribe(pressureTopic);
    client.subscribe(lightTopic);
    client.subscribe(dateTopic);
    client.subscribe(timeTopic);
  }
}

void sendTelegramMessage(const char *message) {
  HTTPClient http;
  for (const auto& chatID : chatIDs) {
    String url = "https://api.telegram.org/bot" + String(telegramBotToken) + "/sendMessage?chat_id=" + chatID + "&text=" + String(message);
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.print("Messaggio Telegram inviato a ");
      Serial.print(chatID);
      Serial.print(". Response code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Errore nell'inviare il messaggio Telegram a ");
      Serial.print(chatID);
      Serial.print(". Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}

void syncTimeWithNTP() {
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  unsigned long epochTime = timeClient.getEpochTime();
  rtc.adjust(DateTime(epochTime));
}

void sensorTask(void *parameter) {
  (void)parameter;
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1) {
    // Leggi dati dai sensori
    SensorData data;
    data.temperature = bmp.readTemperature();
    data.pressure = bmp.readPressure() / 100.0; // Pressione in hPa
    data.light = lightSensor.readLightLevel();

    // Leggi l'ora corrente dal RTC
    DateTime now = rtc.now();
    snprintf(data.date, sizeof(data.date), "%04d/%02d/%02d", now.year(), now.month(), now.day());
    snprintf(data.time, sizeof(data.time), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

    // Controllo delle soglie
    if (temperatureThresholdEnabled && data.temperature > temperatureThresholdHigh) {
      sendTelegramMessage("La temperatura Ã¨ superiore a 5 gradi Celsius!");
      temperatureThresholdEnabled = false; // Disattiva il monitoraggio della soglia
    }
    if (!temperatureThresholdEnabled && data.temperature <= temperatureThresholdLow) {
      temperatureThresholdEnabled = true;
    }

    if (pressureThresholdEnabled && data.pressure > pressureThresholdHigh) {
      sendTelegramMessage("La pressione Ã¨ superiore a 1020 hPa!");
      pressureThresholdEnabled = false; // Disattiva il monitoraggio della soglia
    }
    if (!pressureThresholdEnabled && data.pressure <= pressureThresholdLow) {
      pressureThresholdEnabled = true;
    }

    if (lightThresholdEnabled && data.light > lightThresholdHigh) {
      sendTelegramMessage("La luminositÃ  Ã¨ superiore a 500 lumen!");
      lightThresholdEnabled = false; // Disattiva il monitoraggio della soglia
    }
    if (!lightThresholdEnabled && data.light <= lightThresholdLow) {
      lightThresholdEnabled = true;
    }

    // Invia i dati alla coda
    if (xQueueSend(dataQueue, &data, portMAX_DELAY) != pdPASS) {
      Serial.println("Coda piena, impossibile inviare i dati dei sensori");
    }

    vTaskDelayUntil(&xLastWakeTime, 10000 / portTICK_PERIOD_MS); // Ritardo di 10 secondi
  }
}

void mqttTask(void *parameter) {
  (void)parameter;
  SensorData data;

  while (1) {
    // Ricevi i dati dalla coda
    if (xQueueReceive(dataQueue, &data, portMAX_DELAY) == pdPASS) {
      // Pubblica i dati su MQTT
      if (client.publish(temperatureTopic, String(data.temperature).c_str())) {
        Serial.print("Temperatura inviata al Broker MQTT: ");
        Serial.println(data.temperature);
      } else {
        Serial.println("Fallito nell'inviare la temperatura al Broker MQTT");
      }

      if (client.publish(pressureTopic, String(data.pressure).c_str())) {
        Serial.print("Pressione inviata al Broker MQTT: ");
        Serial.println(data.pressure);
      } else {
        Serial.println("Fallito nell'inviare la pressione al Broker MQTT");
      }

      if (client.publish(lightTopic, String(data.light).c_str())) {
        Serial.print("LuminositÃ  inviata al Broker MQTT: ");
        Serial.println(data.light);
      } else {
        Serial.println("Fallito nell'inviare la luminositÃ  al Broker MQTT");
      }

      if (client.publish(dateTopic, data.date)) {
        Serial.print("Data inviata al Broker MQTT: ");
        Serial.println(data.date);
      } else {
        Serial.println("Fallito nell'inviare la data al Broker MQTT");
      }

      if (client.publish(timeTopic, data.time)) {
        Serial.print("Ora inviata al Broker MQTT: ");
        Serial.println(data.time);
      } else {
        Serial.println("Fallito nell'inviare l'ora al Broker MQTT");
      }
    }
  }
}

void syncTimeTask(void *parameter) {
  (void)parameter;
  while (1) {
    syncTimeWithNTP();
    vTaskDelay(3600000 / portTICK_PERIOD_MS); // Sincronizza ogni ora
  }
}

void handleTelegramMessages() {
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(telegramBotToken) + "/getUpdates";
  http.begin(url);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println(response);

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    JsonArray updates = doc["result"].as<JsonArray>();

    for (JsonObject update : updates) {
      String chatID = update["message"]["chat"]["id"].as<String>();
      addChatID(chatID); // Aggiungi l'ID chat all'elenco
    }
  } else {
    Serial.print("Errore nell'ottenere gli aggiornamenti da Telegram. Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

void addChatID(const String& chatID) {
  if (std::find(chatIDs.begin(), chatIDs.end(), chatID) == std::end(chatIDs)) {
    chatIDs.push_back(chatID);
    Serial.print("Aggiunto nuovo chatID: ");
    Serial.println(chatID);
  }
}