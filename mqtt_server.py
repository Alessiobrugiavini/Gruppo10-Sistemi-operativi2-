import matplotlib.pyplot as plt
import paho.mqtt.client as mqtt
import numpy as np
import os
import tracemalloc
import mysql.connector
import matplotlib.dates as mdates
import threading
import asyncio
from datetime import datetime, time, timedelta
import json

import telegram_bot

with open("Configurazione.json") as f:
    data=json.load(f)

db_config = {
     'host': '192.168.155.51',  # Indirizzo del server MySQL
     'user': 'brugia',  # Nome utente del database MySQL
     'password': 'halo3000',  # Password del database MySQL
     'database': 'esp32_data'  # Nome del database MySQL
}

temperatureThresholdEnabled = True
temperatureThresholdHigh = 5.5 
temperatureHysteresis = 1.0

lightThresholdEnabled = True
lightThresholdHigh = 530.0
lightHysteresis = 200.0

pressureThresholdEnabled = True
pressureThresholdHigh = 1022.0
pressureHysteresis = 200.0

broker_address = "192.168.155.229"
topics = ["sensori"]  # Un unico topic per i dati strutturati

def send_alert(message):
        
    query="SELECT chat_id FROM chatids"
    cursor.execute(query)
    results = cursor.fetchall()
    for result in results:
        print(result)
        asyncio.set_event_loop(asyncio.new_event_loop())
        loop = asyncio.get_event_loop()
        loop.run_until_complete(telegram_bot.invia_messaggio_telegram(message, result))
        loop.close()


def on_message(client, userdata, message):
    topic = message.topic
    payload = message.payload.decode("utf-8")
    print(f"Ricevuto il messaggio '{payload}' nel topic '{topic}'")

    global temperatureThresholdEnabled, pressureThresholdEnabled, lightThresholdEnabled
    
    # Decodifica del payload separato da virgole
    try:
        data = payload.split(',')
        if len(data) == 5:
            temperatura = float(data[0])
            pressione = float(data[1])
            luminosita = float(data[2])
            data_value = data[3]
            ora_value = data[4]
            
            query = "INSERT INTO sensor_data (temperatura, pressione, luminosita, data, ora) VALUES (%s, %s, %s, %s, %s)"
            cursor.execute(query, (temperatura, pressione, luminosita, data_value, ora_value))
            conn.commit()
            print(f"Dato inserito correttamente: {payload}")
        else:
            print(f"Payload incompleto: {payload}")
    except ValueError as e:
        print(f"Errore nel decodificare il payload: {e}")

    if temperatureThresholdEnabled and temperatura> temperatureThresholdHigh:
        send_alert("Attenzione: temperatura alta")      
        temperatureThresholdEnabled = False
            
    if temperatureThresholdEnabled == False and temperatura<= temperatureThresholdHigh-temperatureHysteresis:
        temperatureThresholdEnabled = True
                
    if pressureThresholdEnabled and pressione  > pressureThresholdHigh:
        send_alert("Attenzione: pressione alta")         
        pressureThresholdEnabled = False 
      
    if pressureThresholdEnabled== False and pressione <= pressureThresholdHigh - pressureHysteresis:
        pressureThresholdEnabled = True
     
    if lightThresholdEnabled and luminosita  > lightThresholdHigh:
        send_alert("Attenzione: luminosità alta")
        lightThresholdEnabled = False
                 
    if lightThresholdEnabled== False and luminosita <= lightThresholdHigh - lightHysteresis:
        lightThresholdEnabled = True

def main():
    print("Esecuzione main...") 
    
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

    client.on_message = on_message

    client.connect(broker_address)
    print("Connessione al broker MQTT avvenuta con successo")

    # Sottoscrizione ai topic
    client.subscribe("sensori")
    print("Sottoscritto al topic: sensori")

    client.loop_forever()
    

if __name__ == '__main__':

# Connessione al database MySQL
    conn = mysql.connector.connect(**db_config)
    cursor = conn.cursor()
    print("Connessione al database MySQL avvenuta con successo")
    
main()
