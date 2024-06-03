import matplotlib.pyplot as plt
import paho.mqtt.client as mqtt
import numpy as np
import os
import tracemalloc 
import mysql.connector
import matplotlib.dates as mdates

from datetime import datetime, time, timedelta
from telegram.ext import Application, Updater, CommandHandler, MessageHandler, ContextTypes, filters
from telegram import ForceReply, Update, Bot




TOKEN = '6728633709:AAFXKIkfqvrAS2ublCwPKIJ5PIdrKqdgEps'

# Connessione al database MySQL
conn = mysql.connector.connect(
    host='192.168.1.13',  # Inserisci l'indirizzo del server MySQL
    user='brugia',   # Inserisci il nome utente del database MySQL
    password='halo3000',  # Inserisci la password del database MySQL
    database='esp32_data'  # Inserisci il nome del database MySQL
)

cursor = conn.cursor()


# Funzione per gestire i messaggi di testo inviati dagli utenti
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
            # Verifica il tipo di dato del primo elemento per determinare il formato
            if isinstance(results[0][0], int):
                # Se il timestamp è un intero (UNIX timestamp)
                timestamps = [datetime.fromtimestamp(row[0]) for row in results]
            elif isinstance(results[0][0], str):
                # Se il timestamp è una stringa
                timestamps = [datetime.strptime(row[0], '%Y-%m-%d %H:%M:%S') for row in results]
            else:
                raise TypeError("Il tipo di dato del timestamp non è né un intero né una stringa")

            pressione = [row[1] for row in results]
            luminosita = [row[2] for row in results]
            temperatura = [row[3] for row in results]
            
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
                (timestamps, temperatura, 'Temperatura', temperatura_media, temperatura_minimo, temperatura_massimo, 'grafico_temperatura.png'),
                (timestamps, pressione, 'Pressione', pressione_media, pressione_minimo, pressione_massimo, 'grafico_pressione.png'),
                (timestamps, luminosita, 'Luminosità', luminosita_media, luminosita_minimo, luminosita_massimo, 'grafico_luminosita.png')
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
                    message = f"Pressione: {row[1]}, Luminosità: {row[2]}, Temperatura: {row[3]}, Data: {row[4]}, ora: {row[5]}"
                    await context.bot.send_message(chat_id=user.id, text=message)
            else:
                await context.bot.send_message(chat_id=user.id, text="Nessun valore trovato per la data e l'ora specificate.")
    except ValueError:
        await context.bot.send_message(chat_id=user.id, text="Formato data e ora non valido. Utilizza 'YYYY-MM-DD HH:MM:SS'.")




async def start(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Send a message when the command /start is issued."""
    user = update.effective_user
    await update.message.reply_html(
        rf"Ciao {user.mention_html()},  insert read+data+topic to obtain data!",
        reply_markup=ForceReply(selective=True),
    )




def main():
  
    tracemalloc.start()
    
    application  = Application.builder().token("6728633709:AAFXKIkfqvrAS2ublCwPKIJ5PIdrKqdgEps").build()

    # manage start command
    application.add_handler(CommandHandler("start", start))

     # manage read command
    application.add_handler(CommandHandler("leggi", leggi))

    # Run the bot until the user presses Ctrl-C
    application.run_polling(allowed_updates=Update.ALL_TYPES)

   
if __name__ == '__main__':
    main()