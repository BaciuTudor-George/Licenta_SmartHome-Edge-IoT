#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// CONFIGURARE RETEA ȘI SERVER 
const char* ssid = "Tudor";
const char* password = "12345678";
const char* mqtt_server = "10.22.72.160";


// DEFINIRE PINI NOD DORMITOR

#define DHTPIN 25     
#define DHTTYPE DHT22 
#define PIRPIN 26     
#define LEDPIN 27     

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// VARIABILE PENTRU TEMPORIZATOARE (TIMERS)
unsigned long lastMsg = 0;         
unsigned long timpOprireLED = 0;   

// VARIABILE DE STARE
bool controlManual = false;        
int ultimaStarePIR = LOW;          


// FUNCTIE RECEPTIE MQTT

void callback(char* topic, byte* payload, unsigned long length) {
  char comanda = (char)payload[0];

  if (strcmp(topic, "dormitor/led") == 0) {
    if (comanda == '1') {
      controlManual = true;       
      digitalWrite(LEDPIN, HIGH); 
    } 
    else if (comanda == '0') {
      controlManual = true;       
      digitalWrite(LEDPIN, LOW);  
    }
    else if (comanda == 'A') {    
      controlManual = false;      
    }
  }
}

// CONECTARE WI-FI
void setup_wifi() {
  delay(10);
  Serial.println("\nConectare la Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi de la Dormitor s-a conectat!");
}

// CONECTARE / RECONECTARE BROKER MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Se incearca conectarea la MQTT...");
    if (client.connect("ESP32_Dormitor_Client")) { 
      Serial.println("CONECTAT!");
      client.subscribe("dormitor/led");
    } else {
      delay(5000);
    }
  }
}

// INITIALIZARE (SETUP)
void setup() {
  Serial.begin(115200);
  Serial.println("\n INITIALIZARE NOD DORMITOR ");
  
  dht.begin();
  pinMode(PIRPIN, INPUT);
  pinMode(LEDPIN, OUTPUT);
  
  digitalWrite(LEDPIN, LOW); 

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

 // LOGICA CONTROL LED SI ALERTA INSTANT PIR 
if (controlManual == false) {
    int starePIR = digitalRead(PIRPIN);
    unsigned long timpCurent = millis();

    // 1. Detectare schimbare PIR
    if (starePIR != ultimaStarePIR) {
        ultimaStarePIR = starePIR; 
        
        if (starePIR == HIGH) {
            digitalWrite(LEDPIN, HIGH);    
            Serial.println("MIȘCARE: ON");    
            timpOprireLED = timpCurent + 3000; 
            
            // Trimite mesajul de "DETECTAT" 
            client.publish("dormitor/miscare", "1");
        } 
    }

    // 2. Stingere automata + Trimitere mesaj "Liniște" după 3 secunde
    if (starePIR == LOW && timpCurent >= timpOprireLED && digitalRead(LEDPIN) == HIGH) {
        digitalWrite(LEDPIN, LOW);
        client.publish("dormitor/miscare", "0");
        Serial.println("MIȘCARE: OFF (Timeout)");
    }
}

  // TRIMITERE SI AFISARE DATE (La fiecare 5 secunde) 
  unsigned long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    char tempString[8] = "0.0";
    char humString[8] = "0.0";

    if (!isnan(temperature) && !isnan(humidity)) {
      dtostrf(temperature, 1, 1, tempString);
      dtostrf(humidity, 1, 1, humString);
    } else {
      Serial.print("![Eroare citire DHT22] ");
    }

    // Afiseaza senzorii si modul curent (AUTOMAT / MANUAL)
    Serial.printf("Temperatura: %s°C | Umiditate: %s%% | Mod: %s\n", 
                  tempString, 
                  humString, 
                  (controlManual ? "MANUAL" : "AUTOMAT"));

    // Trimite datele catre Node-RED
    client.publish("dormitor/temperatura", tempString);
    client.publish("dormitor/umiditate", humString);
  }
}