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
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600*2, 60000);

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

  handleTelegramMessages(); // Controlla nuovi messaggi Telegram
  
  delay(10000); // Attendi 10 secondi prima di controllare di nuovo
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
      // Crea una stringa formattata con i dati dei sensori separati da virgole
      char payload[100];
      snprintf(payload, sizeof(payload), "%.2f,%.2f,%.2f,%s,%s", data.temperature, data.pressure, data.light, data.date, data.time);

      // Pubblica i dati su MQTT
      if (client.publish("sensori", payload)) {
        Serial.print("Dati inviati al Broker MQTT: ");
        Serial.println(payload);
      } else {
        Serial.println("Fallito nell'inviare i dati al Broker MQTT");
      }




    // Controllo della soglia solo se il monitoraggio della soglia Ã¨ abilitato
      if (temperatureThresholdEnabled && data.temperature  > temperatureThresholdHigh) {
          sendTelegramMessage("Attenzione: Temperatura troppo alta[C]!", String(data.temperature).c_str());
          temperatureThresholdEnabled = false; // Disattiva il monitoraggio della soglia
        }else if(temperatureThresholdEnabled&& data.temperature < temperatureThresholdLow){
          sendTelegramMessage("Attenzione: Temperatura troppo bassa [C]!", String(data.temperature).c_str());
          temperatureThresholdEnabled = false; // Disattiva il monitoraggio della soglia
        }
      // Riattiva il monitoraggio della soglia se la temperatura scende sotto la soglia
      if (!temperatureThresholdEnabled && data.temperature <= temperatureThresholdHigh) {
          temperatureThresholdEnabled = true;

      }

        // Controllo della soglia solo se il monitoraggio della soglia Ã¨ abilitato
      if (pressureThresholdEnabled && data.pressure  > pressureThresholdHigh) {
          sendTelegramMessage("Attenzione: Pressione troppo alta[Pa]!", String(data.pressure).c_str());
          pressureThresholdEnabled = false; // Disattiva il monitoraggio della soglia
        }else if(pressureThresholdEnabled&& data.pressure < pressureThresholdLow){
          sendTelegramMessage("Attenzione: Pressione troppo bassa[Pa]!", String(data.pressure).c_str());
          pressureThresholdEnabled = false; // Disattiva il monitoraggio della soglia
        }
      // Riattiva il monitoraggio della soglia se la temperatura scende sotto la soglia
      if (!pressureThresholdEnabled && data.pressure <= pressureThresholdHigh) {
          pressureThresholdEnabled = true;

      }
 
    

      // Controlla le soglie di luminositÃ 
      if (lightThresholdEnabled) {
        if (data.light > lightThresholdHigh) {
          sendTelegramMessage("Attenzione: LuminositÃ  troppo alta!", String(data.light).c_str());
          lightThresholdEnabled=false;
        } else if (data.light < lightThresholdLow) {
          sendTelegramMessage("Attenzione: LuminositÃ  troppo bassa!", String(data.light).c_str());
          lightThresholdEnabled=false;
        } else if (data.light >= lightThresholdLow && data.light <= lightThresholdHigh) {
          lightThresholdEnabled = true; // Reset alert quando la luminositÃ  rientra nei limiti
        }
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

void sendTelegramMessage(const char *message, const char *measurement) {
  // Attendi finchÃ© non ci sono chat ID disponibili
  while (chatIDs.empty()) {
    Serial.println("Nessun chat ID disponibile. In attesa di ottenere i chat ID...");
    handleTelegramMessages(); // Controlla i nuovi messaggi per ottenere i chat ID
    delay(5000); // Attendi 5 secondi prima di controllare di nuovo
  }

  if (WiFi.status() == WL_CONNECTED && !chatIDs.empty()) {  // Verifica che chatIDs non sia vuoto
    HTTPClient http;
    for (const String& chatID : chatIDs) {
      String url = String("https://api.telegram.org/bot") + telegramBotToken + "/sendMessage?chat_id=" + chatID + "&text=" + message + " Misurazione corrente: " + measurement;
      http.begin(url);
      int httpCode = http.GET();
      if (httpCode > 0) {
        Serial.println("Messaggio Telegram inviato correttamente a chatID: " + chatID);
      } else {
        Serial.println("Errore nell'inviare il messaggio Telegram a chatID: " + chatID);
      }
      http.end();
    }
  } else {
    Serial.println("Errore: chatIDs Ã¨ vuoto o WiFi non Ã¨ connesso.");
  }
}




void handleTelegramMessages() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("https://api.telegram.org/bot") + telegramBotToken + "/getUpdates";
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        for (JsonObject update : doc["result"].as<JsonArray>()) {
          String chat_id = update["message"]["chat"]["id"].as<String>();
          if (std::find(chatIDs.begin(), chatIDs.end(), chat_id) == chatIDs.end()) {
            chatIDs.push_back(chat_id); // Aggiungi il chat_id se non Ã¨ giÃ  presente
            Serial.print("Nuovo chat ID aggiunto: ");
            Serial.println(chat_id);
          }
        }
      } else {
        Serial.println("Errore nella deserializzazione del payload JSON");
      }
    } else {
      Serial.println("Errore HTTP nella richiesta getUpdates");
    }
    http.end();
  }
}


