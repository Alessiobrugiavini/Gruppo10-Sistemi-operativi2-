# Sviluppo di un sistema di acquisizione dati tramite ESP32 e ricezione tramite broker MQTT.

Questo progetto utilizza un **ESP32** per acquisire i dati dai sensori **BMP280** (sensore di temperatura e pressione), **BH1750** (sensore di luminosità) e **RTC** e per poi inviarli a un broker **MQTT**, in questo caso un Raspberry Pi 4B. 
I dati vengono successivamente salvati in un database esterno chiamato **esp32_data**. 
È possibile interrogare i dati acquisiti e visualizzare grafici tramite un bot **Telegram**.

## Caratteristiche principali

- **Sensori utilizzati**:
  - BMP280: Sensore di temperatura e pressione atmosferica.
  - BH1750: Sensore di luminosità.
  - RTC: Per la gestione del tempo.
  
- **ESP32** - microcontrollore per l'acquisizione dati.
- Invio dei dati tramite **MQTT** a un broker.
- Salvataggio dei dati su un database esterno (**esp32_data**).
- Interfaccia con bot **Telegram** per interrogare e visualizzare i dati storici e i grafici.

## Architettura del progetto

1. **Acquisizione Dati**:
   - L'ESP32 acquisisce periodicamente i dati dai sensori BMP280, BH1750 e RTC tramite lo script Arduino_nobot.ino 
   
2. **Comunicazione MQTT**:
   - I dati acquisiti vengono inviati a un broker MQTT tramite lo script Arduino_nobot.ino e vengono ricevuti e gestiti tramite lo script mqtt_server.py
   
3. **Database esterno**:
   - I dati dei sensori vengono salvati in un database chiamato in qesto caso **esp32_data**.

4. **Bot Telegram**:
   - L'utente può interagire con un bot Telegram per richiedere i dati memorizzati e visualizzare grafici basati sui dati raccolti. 
     Per maggiori dettagli consultare lo script telegram_bot.py

## Requisiti

- **ESP32** con supporto WiFi.
- Sensori:
  - **BMP280** (sensore di pressione e temperatura).
  - **BH1750** (sensore di luminosità).
  - **RTC** (modulo per la gestione del tempo).
- Broker **MQTT** configurato per ricevere i dati.
- Database (MySQL, PostgreSQL o simile) chiamato **esp32_data**.
- Bot **Telegram** configurato per ricevere comandi dall'utente.
- **Librerie necessarie**:
    - Script Arduino - per ESP32:
        1. **Adafruit Sensor:** libreria base necessaria per tutti i sensori
        2. **Adafruit BMP280:** libreria per il sensore della temperatura e pressione BMP280
        3. **BH1750:** libreria per il sensore di luminosità BH1750
        4. **RTClib:** libreria per il sensore RTC
        5. **WiFi:** libreria per la connessione internet
        6. **PubSubClient:** libreria per lo scambio dei messaggi MQTT
        7. **NTPClient:** libreria utilizzata per creare il client NTP per la sincronizzazione della   data e dell’ora
        8. **WiFiUDP:** libreria per la gestione dei pacchetti
        
    - Script Python: consultabili e installabili dal file requirements.txt. Per gestire diverse versioni di Python all'interno della stessa macchina si può utilizzare Pyenv e Poetry.

## Installazione

### 1. Configurazione del Raspberry Pi e dell'ESP32

- Scaricare e configurare il Raspberry Pi, installare le librerie per script Python da requirements.txt
- Scaricare e installare le librerie necessarie per la gestione dei sensori (BH1750, BMP280 e RTC) e la comunicazione con MQTT.
- Caricare lo sketch sull'ESP32, assicurandosi di configurare i parametri WiFi e MQTT.

### 2. Configurazione del Broker MQTT

- Configurare un broker MQTT (ad esempio Mosquitto) per ricevere i dati dall'ESP32.

### 3. Database

- Creare un database chiamato **esp32_data**, con all'interno due tabelle: **sensor_data** e **chatids**. In questo caso è stato utilizzato XAMMP con servizio Apache e MySQL.
- Assicurarsi che il database sia configurato correttamente per memorizzare i dati dei sensori.

### 4. Configurazione del Bot Telegram

- Creare un bot su Telegram utilizzando **@BotFather**. Il bot utilizzato in questo progetto si chiama esp32_data
- Collegare il bot al server che interroga il database e genera i grafici con i dati.

### 5. Avvio automatico script all'accensione del Raspberry

- Creare due file service all'interno della cartella systemd del Raspberry
- Abilitare ed avviare i servizi

## Utilizzo

1. Accendere l'ESP32 e assicurarsi che sia connesso alla rete WiFi.
2. L'ESP32 inizierà a raccogliere i dati dai sensori e ad inviarli al broker MQTT.
3. I dati verranno salvati automaticamente nel database esterno.
4. Interagire con il bot Telegram per richiedere i dati e visualizzare i grafici.

## Esempi di Comandi Telegram

- `/start` - Introduzione al bot e salvataggio dei CHAT ID
- `/leggi` - Mostra un messaggio con i dati statistici calcolati delle grandezze e grafici con i dati storici indicati dall'utente.


