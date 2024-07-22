import matplotlib.pyplot as plt
import paho.mqtt.client as mqtt
import numpy as np
import os
import tracemalloc
import mysql.connector
import matplotlib.dates as mdates
import threading

from datetime import datetime, time, timedelta
from telegram.ext import Application, CommandHandler, ContextTypes
from telegram import ForceReply, Update

# Configurazioni iniziali
broker_address = '192.168.1.12'  # Indirizzo del broker MQTT
db_config = {
    'host': '192.168.1.11',  # Indirizzo del server MySQL
    'user': 'brugia',  # Nome utente del database MySQL
    'password': 'halo3000',  # Password del database MySQL
    'database': 'esp32_data'  # Nome del database MySQL
}
topics = ["sensori"]  # Un unico topic per i dati strutturati

# Connessione al database MySQL
conn = mysql.connector.connect(**db_config)
cursor = conn.cursor()
print("Connessione al database MySQL avvenuta con successo")

# Token del bot Telegram
TOKEN = '6728633709:AAFXKIkfqvrAS2ublCwPKIJ5PIdrKqdgEps'

# Funzione di callback per la ricezione dei messaggi MQTT
def on_message(client, userdata, message):
    topic = message.topic
    payload = message.payload.decode('utf-8')
    
    # Decodifica del payload separato da virgole
    try:
        data = payload.split(',')
        if len(data) == 5:
            temperatura = float(data[0])
            pressione = int(data[1])
            luminosita = int(data[2])
            data_value = data[3]
            ora_value = data[4]
            
            query = "INSERT INTO sensor_data (temperatura, pressione, luminosità, data, ora) VALUES (%s, %s, %s, %s, %s)"
            cursor.execute(query, (temperatura, pressione, luminosita, data_value, ora_value))
            conn.commit()
            print(f"Dato inserito correttamente: {payload}")
        else:
            print(f"Payload incompleto: {payload}")
    except ValueError as e:
        print(f"Errore nel decodificare il payload: {e}")

# Funzione per configurare e avviare il client MQTT
def sniff_mqtt_messages(broker_address, topics):
    client = mqtt.Client()
    client.on_message = on_message

    # Connessione al broker MQTT
    client.connect(broker_address)
    print("Connessione al broker MQTT avvenuta con successo")
    
    # Sottoscrizione ai topic
    for topic in topics:
        client.subscribe(topic)
        print(f"Sottoscritto al topic: {topic}")

    # Avvia il loop di rete del client MQTT per ricevere i messaggi
    client.loop_forever()

# Funzione per gestire i messaggi di testo inviati dagli utenti Telegram
async def leggi(update, context):
    user = update.effective_user

    if len(context.args) < 2:
        await context.bot.send_message(chat_id=user.id, text="Devi specificare una data e un'ora nel formato 'YYYY-MM-DD HH:MM:SS' e un topic.")
        return

    print("Valore di context.args[0]:", context.args[0])
    print("Valore di context.args[1]:", context.args[1])

    try:
        datetime_str = context.args[0] + ' ' + context.args[1]
        datetime_filter = datetime.strptime(datetime_str, '%Y-%m-%d %H:%M:%S')

        query = "SELECT * FROM sensor_data WHERE DATE(data) >= %s AND TIME(ora) >= %s"
        cursor.execute(query, (datetime_filter.date(), datetime_filter.time()))
        results = cursor.fetchall()

        if results:
            timestamps = [datetime.strptime(row[4] + ' ' + row[5], '%Y-%m-%d %H:%M:%S') for row in results]
            pressione = [row[1] for row in results]
            luminosita = [row[2] for row in results]
            temperatura = [row[0] for row in results]

            # Calcola media, minimo e massimo per ciascun parametro
            pressione_media = np.mean(pressione)
            pressione_minimo = np.min(pressione)
            pressione_massimo = np.max(pressione)

            luminosita_media = np.mean(luminosita)
            luminosita_minimo = np.min(luminosita)
            luminosita_massimo = np.max(luminosita)

            temperatura_media = np.mean(temperatura)
            temperatura_minimo = np.min(temperatura)
            temperatura_massimo = np.max(temperatura)

            # Funzione per creare il grafico e salvarlo
            def crea_grafico(timestamps, valori, parametro, media, minimo, massimo, nome_file):
                plt.figure(figsize=(10, 6))
                plt.plot(timestamps, valori, label=parametro, marker='o')
                plt.axhline(y=media, color='blue', linestyle='--', label=f'Media: {media:.2f}')
                plt.axhline(y=minimo, color='green', linestyle='--', label=f'Minimo: {minimo:.2f}')
                plt.axhline(y=massimo, color='red', linestyle='--', label=f'Massimo: {massimo:.2f}')
                
                plt.legend(loc='upper left')
                plt.xlabel('Data e Ora')
                plt.ylabel('Valore')
                plt.title(f'{parametro} nel Tempo')
                
                # Aggiungi le statistiche sul lato del grafico
                plt.annotate(f'Media: {media:.2f}\nMinimo: {minimo:.2f}\nMassimo: {massimo:.2f}', 
                            xy=(1.05, 0.5), xycoords='axes fraction', fontsize=12, ha='left', va='center')
                
                # Nascondi l'asse X
                plt.gca().xaxis.set_visible(False)
                
                plt.tight_layout()
                plt.savefig(nome_file)
                plt.close()
            
            # Creare e salvare i grafici
            grafici = [
                (timestamps, temperatura, 'Temperatura °C', temperatura_media, temperatura_minimo, temperatura_massimo, 'grafico_temperatura.png'),
                (timestamps, pressione, 'Pressione [Pa]', pressione_media, pressione_minimo, pressione_massimo, 'grafico_pressione.png'),
                (timestamps, luminosita, 'Luminosità [Lumen]', luminosita_media, luminosita_minimo, luminosita_massimo, 'grafico_luminosita.png')
            ]

            for timestamps, valori, parametro, media, minimo, massimo, nome_file in grafici:
                crea_grafico(timestamps, valori, parametro, media, minimo, massimo, nome_file)
            
            # Invia i grafici al bot Telegram
            for _, _, _, _, _, _, nome_file in grafici:
                await context.bot.send_photo(chat_id=user.id, photo=open(nome_file, 'rb'))
                os.remove(nome_file)

            # Invia i valori filtrati al bot
            if results:
                for row in results:
                    message = f"Pressione [Pa]: {row[1]}, Luminosità [Lumen]: {row[2]}, Temperatura[C]: {row[0]}, Data: {row[4]}, ora: {row[5]}"
                    await context.bot.send_message(chat_id=user.id, text=message)
            else:
                await context.bot.send_message(chat_id=user.id, text="Nessun valore trovato per la data e l'ora specificate.")
    except ValueError:
        await context.bot.send_message(chat_id=user.id, text="Formato data e ora non valido. Utilizza 'YYYY-MM-DD HH:MM:SS'.")

# Funzione per il comando /start
async def start(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Send a message when the command /start is issued."""
    user = update.effective_user
    await update.message.reply_html(
        rf"Ciao {user.mention_html()}, inserisci read+data+topic per ottenere i dati!",
        reply_markup=ForceReply(selective=True),
    )

# Funzione principale
def main():
    tracemalloc.start()
    
    application = Application.builder().token(TOKEN).build()

    # Gestione del comando /start
    application.add_handler(CommandHandler("start", start))

    # Gestione del comando read
    application.add_handler(CommandHandler("leggi", leggi))

    # Avvia il bot
    application.run_polling(allowed_updates=Update.ALL_TYPES)

# Esegui il client MQTT in un thread separato
def run_mqtt_client():
    sniff_mqtt_messages(broker_address, topics)

if __name__ == '__main__':
    mqtt_thread = threading.Thread(target=run_mqtt_client)
    mqtt_thread.start()
    main()
