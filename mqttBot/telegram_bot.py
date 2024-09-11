import telegram
from telegram.ext import (
    Application,
    CommandHandler,
    ContextTypes,
)
from telegram import Update
import matplotlib.pyplot as plt
import numpy as np
import os

import mysql.connector
from datetime import datetime
import json


async def start(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Send a message when the command /start is issued."""
    user = update.effective_user
    user_id = user.id

    query_chat = "SELECT * FROM chatids WHERE chat_id=%s"
    cursor.execute(query_chat, (user_id,))
    results = cursor.fetchall()

    if results:
        print(f"L'utente {user.full_name} [{user_id}] ha inviato il comando start.")
    else:
        query = "INSERT INTO chatids (Nome, chat_id) VALUES (%s, %s)"
        cursor.execute(query, (user.full_name, user_id))
        conn.commit()
        print("Utente inserito correttamente:")

    await update.message.reply_text(
        f"Ciao {user.full_name}, inserisci /leggi+data+ora+topic, per ottenere i dati!",
    )


async def leggi(update, context):
    user = update.effective_user

    if len(context.args) < 2:
        await context.bot.send_message(
            chat_id=user.id,
            text="Devi specificare una data e un'ora nel formato 'YYYY-MM-DD HH:MM:SS' e un topic.",
        )
        return

    print("Valore di context.args[0]:", context.args[0])
    print("Valore di context.args[1]:", context.args[1])

    try:
        datetime_str = context.args[0] + " " + context.args[1]
        datetime_filter = datetime.strptime(datetime_str, "%Y-%m-%d %H:%M:%S")

        query = "SELECT * FROM sensor_data WHERE DATE(data) >= %s AND TIME(ora) >= %s"
        cursor.execute(query, (datetime_filter.date(), datetime_filter.time()))
        results = cursor.fetchall()

        if results:

            timestamps = [
                datetime.strptime(
                    row[4].strftime("%Y-%m-%d")
                    + " "
                    + (datetime.min + row[5]).strftime("%H:%M:%S"),
                    "%Y-%m-%d %H:%M:%S",
                )
                for row in results
            ]
            pressione = [row[2] for row in results]
            luminosita = [row[3] for row in results]
            temperatura = [row[1] for row in results]

            valori = len(pressione)

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

            message = f"Temperatura media [C]: {temperatura_media}, Pressione media [Pa]: {pressione_media}, Luminosità media [Lumen]: {luminosita_media}, numero valori: {valori}"
            await context.bot.send_message(chat_id=user.id, text=message)

            # Funzione per creare il grafico e salvarlo
            def crea_grafico(
                timestamps, valori, parametro, media, minimo, massimo, nome_file
            ):
                plt.figure(figsize=(10, 6))
                plt.plot(timestamps, valori, label=parametro, marker="o")
                plt.axhline(
                    y=media, color="blue", linestyle="--", label=f"Media: {media:.2f}"
                )
                plt.axhline(
                    y=minimo,
                    color="green",
                    linestyle="--",
                    label=f"Minimo: {minimo:.2f}",
                )
                plt.axhline(
                    y=massimo,
                    color="red",
                    linestyle="--",
                    label=f"Massimo: {massimo:.2f}",
                )

                plt.legend(loc="upper left")
                plt.xlabel("Data e Ora")
                plt.ylabel("Valore")
                plt.title(f"{parametro} nel Tempo")

                # Aggiungi le statistiche sul lato del grafico
                plt.annotate(
                    f"Media: {media:.2f}\nMinimo: {minimo:.2f}\nMassimo: {massimo:.2f}",
                    xy=(1.05, 0.5),
                    xycoords="axes fraction",
                    fontsize=12,
                    ha="left",
                    va="center",
                )

                # Nascondi l'asse X
                plt.gca().xaxis.set_visible(False)

                plt.tight_layout()
                plt.savefig(nome_file)
                plt.close()

            # Creare e salvare i grafici
            grafici = [
                (
                    timestamps,
                    temperatura,
                    "Temperatura [°C]",
                    temperatura_media,
                    temperatura_minimo,
                    temperatura_massimo,
                    "/tmp/grafico_temperatura.png",
                ),
                (
                    timestamps,
                    pressione,
                    "Pressione [Pa]",
                    pressione_media,
                    pressione_minimo,
                    pressione_massimo,
                    "/tmp/grafico_pressione.png",
                ),
                (
                    timestamps,
                    luminosita,
                    "Luminosità [Lumen]",
                    luminosita_media,
                    luminosita_minimo,
                    luminosita_massimo,
                    "/tmp/grafico_luminosita.png",
                ),
            ]

            for (
                timestamps,
                valori,
                parametro,
                media,
                minimo,
                massimo,
                nome_file,
            ) in grafici:
                crea_grafico(
                    timestamps, valori, parametro, media, minimo, massimo, nome_file
                )

            # Invia i grafici al bot Telegram
            for _, _, _, _, _, _, nome_file in grafici:
                await context.bot.send_photo(
                    chat_id=user.id, photo=open(nome_file, "rb")
                )
                os.remove(nome_file)

        else:
            await context.bot.send_message(
                chat_id=user.id,
                text="Nessun valore trovato per la data e l'ora specificate.",
            )
    except ValueError:
        await context.bot.send_message(
            chat_id=user.id,
            text="Formato data e ora non valido. Utilizza 'YYYY-MM-DD HH:MM:SS'.",
        )


# Definisci la funzione per inviare un messaggio Telegram
async def invia_messaggio_telegram(messaggio, ids, token):
    # bot = telegram.Bot(token=data_config["TELEGRAM_TOKEN"])
    bot = telegram.Bot(token=token)

    async with bot:
        print("Prova invio messaggio bot.")
        id = str(ids).replace(",", "")
        id_f = str(id).replace("(", "")
        id_s = str(id_f).replace(")", "")
        await bot.send_message(chat_id=id_s, text=messaggio)


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

    topics = ["sensori"]  # Un unico topic per i dati strutturati
    # Connessione al database MySQL
    conn = mysql.connector.connect(**db_config)
    cursor = conn.cursor()
    print("Connessione al database MySQL avvenuta con successo")

    # Crea un'applicazione Telegram
    application = Application.builder().token(data_config["TELEGRAM_TOKEN"]).build()
    application.add_handler(CommandHandler("start", start))
    application.add_handler(CommandHandler("leggi", leggi))

    application.run_polling(allowed_updates=Update.ALL_TYPES)
