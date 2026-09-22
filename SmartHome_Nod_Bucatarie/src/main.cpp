#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h> 

// CONFIGURARE RETEA SI SERVER 
const char* ssid = "Tudor";
const char* password = "12345678";
const char* mqtt_server = "10.22.72.160"; 

// DEFINIRE PINI 
#define DHTPIN 25     
#define DHTTYPE DHT22
#define PIRPIN 26     
#define MQ6PIN 34     

DHT dht(DHTPIN, DHTTYPE); // Initializare obiect Adafruit
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;

void setup_wifi() {
  delay(10);
  Serial.println("\nConectare la Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectat cu succes!");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Se incearca conectarea la Brokerul MQTT...");
    if (client.connect("ESP32_Bucatarie_Client")) {
      Serial.println("CONECTAT!");
    } else {
      Serial.print("Esuat, rc=");
      Serial.print(client.state());
      Serial.println(" Reincercam in 5 secunde...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("INITIALIZARE SISTEM");
  
  dht.begin(); // Pornire senzor DHT Adafruit
  pinMode(PIRPIN, INPUT);
  pinMode(MQ6PIN, INPUT);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  // Citim la 5 secunde
  if (now - lastMsg > 5000) {
    lastMsg = now;

    // Citire stabila cu Adafruit
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    int miscare = digitalRead(PIRPIN);
    int nivelGaz = analogRead(MQ6PIN);

    char tempString[8] = "0.0";
    char humString[8] = "0.0";
    char pirString[3];
    char gazString[7];

    // Verificam daca datele sunt valide
    if (!isnan(temperature) && !isnan(humidity)) {
      dtostrf(temperature, 1, 1, tempString);
      dtostrf(humidity, 1, 1, humString);
    } else {
      Serial.print("![Senzorul DHT22 nu a putut fi citit de data asta] ");
    }

    itoa(miscare, pirString, 10);
    itoa(nivelGaz, gazString, 10);

    // Afisare în Serial Monitor
    Serial.println("\nPACHET DATE TRANSMIS");
    Serial.printf("PIR: %s | GAZ: %s | Temp: %s°C | Hum: %s%%\n", 
                  (miscare == HIGH ? " MIȘCARE" : " Liniște"), 
                  gazString, tempString, humString);

    //TRANSMITERE CATRE RASPBERRY PI
    client.publish("bucatarie/temperatura", tempString);
    client.publish("bucatarie/umiditate", humString);
    client.publish("bucatarie/miscare", pirString);
    client.publish("bucatarie/gaz", gazString);
  }
}