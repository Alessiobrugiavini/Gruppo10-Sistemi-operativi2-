import mysql.connector
import paho.mqtt.client as mqtt




# Crea un nuovo client MQTT
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
print("Connessione al broker MQTT avvenuta con successo")


temperatura = "temperatura"
pressione = "pressione"
luminosità = "luminosità"
data = "data"
ora = "ora"


mapping = {
    "temperatura": "temperatura",
    "pressione": "pressione",
    "luminosità": "luminosità",
    "data": "data",
    "ora": "ora"
}


# Lista dei topic da sottoscrivere
topics = [temperatura, pressione, luminosità, data, ora]



# Utilizzo della funzione per sniffare i messaggi MQTT
broker_address = "192.168.1.5"


# Connessione al database MySQL
conn = mysql.connector.connect(
    host='192.168.1.11',  # Inserisci l'indirizzo del server MySQL
    user='brugia',   # Inserisci il nome utente del database MySQL
    password='halo3000',  # Inserisci la password del database MySQL
    database='esp32_data'  # Inserisci il nome del database MySQL
)

print(conn)

cursor = conn.cursor()


# Funzione di callback per la ricezione dei messaggi MQTT
def on_message(client, userdata, message):
    topic = message.topic
    payload = message.payload.decode('utf-8')
    
    # Verifica se il topic è presente nel mapping
    if topic in mapping:
        # Prendi il nome della colonna dal mapping
        column_name = mapping[topic]
        
         # Prepara la query SQL per l'inserimento
        query = f"INSERT INTO sensor_data (temperatura, pressione, luminosità, data, ora) VALUES (%s, %s, %s, %s, %s)"

        # Esegui l'inserimento dei dati
        cursor.execute(query, (payload, payload, payload, payload, payload))
        conn.commit()
        print("Dati inseriti correttamente.")
    else:
        print("Percorso del messaggio non trovato nel mapping.")

def sniff_mqtt_messages(broker_address, topics):

    # Imposta la funzione on_message come callback per gestire i messaggi MQTT ricevuti
    client.on_message = on_message

    # Connetti il client MQTT al broker MQTT
    client.connect(broker_address)

    # Sottoscrivi il client MQTT a ciascun topic specificato
    for topic in topics:
        client.subscribe(topic)
        print("Funzione di sniff&push avvenuta con successo")

    # Avvia il loop di rete del client MQTT per ricevere i messaggi
    client.loop_forever()

# Configurazione del client MQTT
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_message = on_message

# Connessione al broker MQTT
broker_address = '192.168.1.6'  # Inserisci l'indirizzo del broker MQTT
client.connect(broker_address)
client.subscribe('topic_da_sottoscrivere')  # Sostituisci con il topic che desideri sottoscrivere

sniff_mqtt_messages(broker_address, topics)
