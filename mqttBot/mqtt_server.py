import paho.mqtt.client as mqtt
import os
import mysql.connector
import asyncio
import json

import telegram_bot


def send_alert(message):

    query = "SELECT chat_id FROM chatids"
    cursor.execute(query)
    results = cursor.fetchall()
    for result in results:
        print(result)
        asyncio.set_event_loop(asyncio.new_event_loop())
        loop = asyncio.get_event_loop()
        loop.run_until_complete(telegram_bot.invia_messaggio_telegram(message, result, data_config["TELEGRAM_TOKEN"]))
        loop.close()


def on_message(client, userdata, message):
    topic = message.topic
    payload = message.payload.decode("utf-8")
    print(f"Ricevuto il messaggio '{payload}' nel topic '{topic}'")

    global temperatureThresholdEnabled, pressureThresholdEnabled, lightThresholdEnabled

    # Decodifica del payload separato da virgole
    try:
        data = payload.split(",")
        if len(data) == 5:
            temperatura = float(data[0])
            pressione = float(data[1])
            luminosita = float(data[2])
            data_value = data[3]
            ora_value = data[4]

            query = "INSERT INTO sensor_data (temperatura, pressione, luminosita, data, ora) VALUES (%s, %s, %s, %s, %s)"
            cursor.execute(
                query, (temperatura, pressione, luminosita, data_value, ora_value)
            )
            conn.commit()
            print(f"Dato inserito correttamente: {payload}")
        else:
            print(f"Payload incompleto: {payload}")
    except ValueError as e:
        print(f"Errore nel decodificare il payload: {e}")

    if temperatureThresholdEnabled and temperatura > temperatureActiveTh:
        send_alert("Attenzione: temperatura alta")
        temperatureThresholdEnabled = False

    if not temperatureThresholdEnabled and temperatura <= temperatureResetTh:
        temperatureThresholdEnabled = True

    if pressureThresholdEnabled and pressione > pressureActiveTh:
        send_alert("Attenzione: pressione alta")
        pressureThresholdEnabled = False

    if not pressureThresholdEnabled and pressione <= pressureResetTh:
        pressureThresholdEnabled = True

    if lightThresholdEnabled and luminosita > lightActiveTh:
        send_alert("Attenzione: luminosità alta")
        lightThresholdEnabled = False

    if not lightThresholdEnabled and luminosita <= lightResetTh:
        lightThresholdEnabled = True


def main():
    print("Esecuzione main...")

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

    client.on_message = on_message

    client.connect(data_config["broker_address"])
    print("Connessione al broker MQTT avvenuta con successo")

    # Sottoscrizione ai topic
    client.subscribe("sensori")
    print("Sottoscritto al topic: sensori")

    client.loop_forever()


if __name__ == "__main__":

    main_dir = os.path.dirname(__file__)
    config_path = os.path.join(main_dir, "Configurazione.json")

    with open(config_path) as f:
        data_config = json.load(f)

    db_config = {
        "host": data_config["db_config"]["host"],  # Indirizzo del server MySQL
        "user": data_config["db_config"]["user"],  # Nome utente del database MySQL
        "password": data_config["db_config"]["password"],  # Password del database MySQL
        "database": data_config["db_config"]["database"],  # Nome del database MySQL
    }

    temperatureThresholdEnabled = True
    temperatureActiveTh = data_config["temperatureThresholdHigh"]
    temperatureResetTh = temperatureActiveTh - data_config["temperatureHysteresis"]

    lightThresholdEnabled = True
    lightActiveTh = data_config["lightThresholdHigh"]
    lightResetTh = lightActiveTh - data_config["lightHysteresis"]

    pressureThresholdEnabled = True
    pressureActiveTh = data_config["pressureThresholdHigh"]
    pressureResetTh = pressureActiveTh - data_config["pressureHysteresis"]

    topics = ["sensori"]  # Un unico topic per i dati strutturati
    # Connessione al database MySQL
    conn = mysql.connector.connect(**db_config)
    cursor = conn.cursor()
    print("Connessione al database MySQL avvenuta con successo")

    main()
