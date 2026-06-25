#define BLYNK_TEMPLATE_ID "TMPL2MFgtw7OB"
#define BLYNK_TEMPLATE_NAME "Yakuyura"
#define BLYNK_AUTH_TOKEN "wPPOgT2GeVWP4oJ6vy2Lx7gAwq5AeAku"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_BME280.h>

// ==========================================
// Ruteo de Hardware (Arquitectura MECRO 3.3V)
// ==========================================
#define SDA_ADS 21
#define SCL_ADS 22
#define SDA_BMP 32
#define SCL_BMP 33

const char* ssid = "esp";
const char* pass = "12345678";

Adafruit_ADS1115 ads;         
Adafruit_BME280 bme;          
BlynkTimer timer;

// ==========================================
// CONSTANTES DE CALIBRACIÓN INDEPENDIENTE
// ==========================================
float offset_pH = 0.0; 

// CONFIGURACIÓN DE DIVISORES DE VOLTAJE POR CANAL
// Si el sensor tiene divisor de 5V a 3.3V, pon 1.5
// Si el sensor NO tiene divisor (va directo), pon 1.0
const float FACTOR_TURBIDEZ = 1.5; 
const float FACTOR_PH       = 1.0; // Cambiado a 1.0 asumiendo que no le pusiste divisor y por eso leía tan alto
const float FACTOR_TDS      = 1.5; 
const float FACTOR_MQ135    = 1.5; 

void procesarYEnviar() {
  // 1. ADQUISICIÓN DE VOLTAJES CRUDOS (DIRECTOS DEL ADC)
  float v_adc[4];
  for(int i = 0; i < 4; i++) {
    v_adc[i] = ads.computeVolts(ads.readADC_SingleEnded(i));
  }

  // 2. RECONSTRUCCIÓN DEL VOLTAJE REAL DEL SENSOR
  float v_turbidez = v_adc[0] * FACTOR_TURBIDEZ;
  float v_pH       = v_adc[1] * FACTOR_PH;
  float v_tds      = v_adc[2] * FACTOR_TDS;
  float v_mq135    = v_adc[3] * FACTOR_MQ135;

  // 3. PROCESAMIENTO DE SEÑALES ANALÓGICAS
  // A0: Turbidez 
  float turbidez = 0;
  if (v_turbidez < 2.5) {
    turbidez = 3000; 
  } else if (v_turbidez > 4.2) {
    turbidez = 0;    
  } else {
    turbidez = -1120.4 * v_turbidez * v_turbidez + 5742.3 * v_turbidez - 4352.9;
    if(turbidez < 0) turbidez = 0;
  }

  // A1: pH 
  float pH = 7.0 + ((2.5 - v_pH) / 0.18) + offset_pH;

  // A2: TDS 
  float tds = (133.42 * v_tds * v_tds * v_tds - 255.86 * v_tds * v_tds + 857.39 * v_tds) * 0.5;
  if(tds < 0) tds = 0;

  // 4. VARIABLES ATMOSFÉRICAS (BME280)
  float temp = bme.readTemperature();
  float presion_atm = bme.readPressure() / 101325.0F; 
  float humedad = bme.readHumidity();

  // 5. TRANSMISIÓN IOT BLYNK
  if (Blynk.connected()) {
    Blynk.virtualWrite(V0, v_mq135);     
    Blynk.virtualWrite(V1, presion_atm); 
    Blynk.virtualWrite(V2, pH);        
    Blynk.virtualWrite(V3, turbidez);  
    Blynk.virtualWrite(V4, tds);       
    Blynk.virtualWrite(V6, temp);      
    Blynk.virtualWrite(V7, humedad);   
  }
  
  // 6. TELEMETRÍA (Modo Ingeniería - Aislamiento de Fallas)
  Serial.println("==================================================================");
  Serial.printf("[ADC_PURO] A0:%.2fV | A1:%.2fV | A2:%.2fV | A3:%.2fV\n", v_adc[0], v_adc[1], v_adc[2], v_adc[3]);
  Serial.printf("[SENSOR_V] Turb:%.2fV | pH:%.2fV | TDS:%.2fV | MQ:%.2fV\n", v_turbidez, v_pH, v_tds, v_mq135);
  Serial.printf("[PROCESADO] pH:%.2f | Turb:%.1f | TDS:%.0f ppm | MQ135:%.2f V\n", pH, turbidez, tds, v_mq135);
  Serial.printf("[ATMOSFERA] Presion:%.4f atm | Temp:%.1f C | Hum:%.1f %%\n", presion_atm, temp, humedad);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Wire.begin(SDA_ADS, SCL_ADS);
  Wire1.begin(SDA_BMP, SCL_BMP);
  
  // GAIN_ONE permite leer máximo 4.096V (OJO: No superes los 3.6V físicos si el ADS está a 3.3V)
  ads.setGain(GAIN_ONE);
  
  if (!ads.begin(0x48, &Wire)) Serial.println("[CRÍTICO] Error ADC.");
  if (!bme.begin(0x76, &Wire1)) Serial.println("[CRÍTICO] Error BME280.");

  WiFi.begin(ssid, pass);
  Blynk.config(BLYNK_AUTH_TOKEN);
  
  timer.setInterval(1000L, procesarYEnviar);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!Blynk.connected()) Blynk.connect(5000); 
    Blynk.run();
  }
  timer.run();
}