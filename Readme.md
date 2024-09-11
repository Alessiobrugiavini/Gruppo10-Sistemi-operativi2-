# Sviluppo di un sistema di acquisizione dati tramite ESP32 e ricezione tramite broker MQTT.

Questo progetto utilizza un **ESP32** per acquisire i dati dai sensori **RTC**, **BH1750** (sensore di luminosità) e 
**BMP280** (sensore di pressione e temperatura), inviando poi i dati a un broker **MQTT**, in questo caso sarà un raspberry PI4. 
I dati vengono successivamente salvati in un database esterno chiamato **esp32_data**. 
È possibile interrogare i dati acquisiti e visualizzare grafici tramite un bot **Telegram**.

## Caratteristiche principali

- **Sensori utilizzati**:
  - RTC: Per la gestione del tempo.
  - BH1750: Sensore di luminosità.
  - BMP280: Sensore di pressione atmosferica e temperatura.
- **ESP32** come microcontrollore per l'acquisizione dati.
- Invio dei dati tramite **MQTT** a un broker.
- Salvataggio dei dati su un database esterno (**esp32_data**).
- Interfaccia con bot **Telegram** per interrogare e visualizzare i dati storici e i grafici.

## Architettura del progetto

1. **Acquisizione Dati**:
   - L'ESP32 acquisisce periodicamente i dati dai sensori RTC, BH1750 e BMP280 tramite lo script Arduinoo-nanobot.ino
   
2. **Comunicazione MQTT**:
   - I dati acquisiti vengono inviati a un broker MQTT, che gestisce l'inoltro dei messaggi tramite lo script Mqtt_server.py
   
3. **Database esterno**:
   - I dati dei sensori vengono salvati in un database chiamato in qesto caso **esp32_data**.

4. **Bot Telegram**:
   - L'utente può interagire con un bot Telegram per richiedere i dati memorizzati e visualizzare grafici basati sui dati raccolti. 
     Per maggiori dettagli consultare lo script Telegram_bot.py

## Requisiti

- **ESP32** con supporto WiFi.
- Sensori:
  - **BH1750** (sensore di luminosità).
  - **BMP280** (sensore di pressione e temperatura).
  - **RTC** (modulo per la gestione del tempo).
- Broker **MQTT** configurato per ricevere i dati.
- Database (MySQL, PostgreSQL o simile) chiamato **esp32_data**.
- Bot **Telegram** configurato per ricevere comandi dall'utente.
- **Librerie necessarie**: INSERIRE LA LISTA DELLE LIBRERIE NECESSARIE, BASTA FARE COPIA INCOLLA DA CODICE
  - Librerie per l'ESP32.
  - Libreria MQTT.
  - Librerie per l'interfacciamento con i sensori BH1750, BMP280 e RTC.
  - Librerie per l'integrazione con il bot Telegram.

## Installazione

### 1. Configurazione dell'ESP32

- Scaricare e installare le librerie necessarie per la gestione dei sensori (BH1750, BMP280 e RTC) e la comunicazione MQTT.
- Caricare lo sketch sull'ESP32, assicurandosi di configurare i parametri WiFi e MQTT.

### 2. Configurazione del Broker MQTT

- Configurare un broker MQTT (ad esempio Mosquitto) per ricevere i dati dall'ESP32.

### 3. Database

- Creare un database chiamato **esp32_data**. In questo caso è stato utilizzato XAMMP
- Assicurarsi che il database sia configurato correttamente per memorizzare i dati dei sensori.

### 4. Configurazione del Bot Telegram

- Creare un bot su Telegram utilizzando **@BotFather**. Il bot utilizzato in questo progetto si chiama esp32_data
- Collegare il bot al server che interroga il database e genera i grafici con i dati.

## Utilizzo

1. Accendere l'ESP32 e assicurarsi che sia connesso alla rete WiFi.
2. L'ESP32 inizierà a raccogliere i dati dai sensori e a inviarli al broker MQTT.
3. I dati verranno salvati automaticamente nel database esterno.
4. Interagire con il bot Telegram per richiedere i dati e visualizzare i grafici.

## Esempi di Comandi Telegram

- `/start` - Introduzione al bot e salvataggio dei CHAT ID
- `/leggi` - Mostra un grafico con i dati storici indicati dall utente.


