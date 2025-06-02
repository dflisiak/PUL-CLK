#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_LPS35HW.h>
#include <U8g2lib.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_SHT4x.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2591.h>

#define SPI_CS D2
#define SPI_DC D3
#define SPI_CLK D8
#define SPI_MOSI D10
#define I2C_SDA D4
#define S2C_SCL D5

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;
const char* ssid = "CNNC2";
const char* password = "KoziolekMatolek5";
const char* mqtt_server = "192.168.1.109";
const char HOME_SYMBOL[] = { 0x44, '\0' };
const char CLOUD_SYMBOL[] = { 0x40, '\0' };
const int night_start = 0;
const int night_end = 5;
unsigned long previousMillis;
unsigned long currentMillis;
unsigned long previousMillisWifi;
unsigned long previousMillisMQ;
unsigned long previousMillisSend;
unsigned long previousMillisRefresh;
unsigned long interval_reconnect = 30000;
unsigned long interval_send = 600000;
unsigned long interval_refresh = 1000;


WiFiClient espClient;
PubSubClient mqttClient(espClient);
Adafruit_LPS35HW lps35hw = Adafruit_LPS35HW();
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

U8G2_SSD1327_WS_128X128_F_4W_HW_SPI u8g2 (U8G2_R0,SPI_CS,SPI_DC);
//U8G2_SSD1327_WS_128X128_F_4W_HW_SPI u8g2 (U8G2_R3,SPI_CS,SPI_DC);

uint8_t contrast;
uint8_t vcomh;
uint16_t luminosity;
float temp;
float temp2;
float temp3;
float humid;
float humid2;
float humid3;
float press;
struct tm timeinfo;
float ow_temp;
float ow_humid;
float ow_press;
sensors_event_t humidity_w, temp_w;

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  JsonDocument doc;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  if (String(topic) == "R1/zigbee2mqtt/TePokoj") {
    deserializeJson(doc, messageTemp);
    temp = doc["temperature"];
    humid = doc["humidity"];
  }

  if (String(topic) == "R1/zigbee2mqtt/TeZewnatrz") {
    deserializeJson(doc, messageTemp);
    temp2 = doc["temperature"];
    humid2 = doc["humidity"];
  }

}

void print_screen_1(){

  char f_buf[50]; 
  char f_buf2[50]; 
  char s_buf[50];
 
  sprintf(f_buf, "%.1f", temp);
  sprintf(f_buf2, "%.1f", temp2);
  strftime(s_buf,sizeof(s_buf),"%H:%M",&timeinfo);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso42_tr);
  u8g2.drawStr(0,42,s_buf);
  u8g2.setFont(u8g2_font_open_iconic_embedded_2x_t);
  u8g2.drawUTF8(0,84, HOME_SYMBOL);
  u8g2.setFont(u8g2_font_logisoso34_tr);
  u8g2.drawStr(25,84,f_buf);
  u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);  
  u8g2.drawUTF8(0,128, CLOUD_SYMBOL);
  u8g2.setFont(u8g2_font_logisoso34_tr);  
  u8g2.drawStr(25,128,f_buf2);
  u8g2.sendBuffer();
}

void clear_screen(){
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

void setup_wifi() {
  delay(10);
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("RSSI: ");
  Serial.println(WiFi.RSSI());
}

void setup_time(){
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1); 
  tzset();

  currentMillis=millis();
  previousMillisWifi=currentMillis;
  previousMillisMQ=currentMillis;
  previousMillisSend=currentMillis;
  previousMillisRefresh=currentMillis;
}

void setup_serial(){
  Serial.begin(9600);
  while (!Serial) { delay(1); }
}

void setup_mqtt_subscribe(){
  mqttClient.subscribe("R1/zigbee2mqtt/TePokoj"); 
  mqttClient.subscribe("R1/zigbee2mqtt/TeZewnatrz");
}

void setup_mqtt(){
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);

  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("mqtts","mqtt","mqtt")) {
      Serial.println("connected");
      setup_mqtt_subscribe();
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}

void setup_sensors(){
  if (!lps35hw.begin_I2C(93U)) {
    Serial.println("Couldn't find LPS35HW chip");
    while (1);
  }

  lps35hw.setDataRate(LPS35HW_RATE_ONE_SHOT);

  if (!sht4.begin()) {
    Serial.println("Couldn't find SHT4x");
    while (1) delay(1);
  }

  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);

  if (!tsl.begin())  
  {
    Serial.println("Couldn't find TSL");
    while (1);
  }

  tsl.setGain(TSL2591_GAIN_MED);

  tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);

  if (!u8g2.begin())  
  {
    Serial.println("Couldn't find u8g2");
    while (1);
  }
}

void set_brighteness(){
  if(luminosity>30){
    contrast=255;
    vcomh = 7;
  } else{
    contrast=0;
    vcomh = 0;
  }
  u8g2.setContrast(contrast);
  u8x8_cad_StartTransfer(u8g2.getU8x8());
  u8x8_cad_SendCmd( u8g2.getU8x8(), 0xbe);
  u8x8_cad_SendArg( u8g2.getU8x8(), vcomh);    // vcomh value from 0 to 7
  u8x8_cad_EndTransfer(u8g2.getU8x8()); 
}

void reconnect_WiFi(){
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
}

void reconnect_MQTT() {
    Serial.print("Reconnecting to MQTT...");
    if (mqttClient.connect("mqtts","mqtt","mqtt")) {
      Serial.println("connected");
      setup_mqtt_subscribe();
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
    }
}

void refreshLocalTime(){
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
}

void refreshLocalTempHumid()
{
  sht4.getEvent(&humidity_w, &temp_w);
  temp3 = temp_w.temperature;
  humid3 = humidity_w.relative_humidity;
  return;
}

void refreshLocalPressure()
{
  lps35hw.takeMeasurement();
  press = lps35hw.readPressure();
  return;
}

void refreshLuminosity(void)
{
  luminosity = tsl.getLuminosity(TSL2591_VISIBLE);
}

void submitLocalTempHumid(){
  JsonDocument doc;
  char buffer[256];
  doc["temperature"] = serialized(String(temp3,1));
  doc["humidity"] = serialized(String(humid3,1));
  size_t n = serializeJson(doc, buffer);
  mqttClient.publish("P2/temphumid", buffer, n);
}

void submitLocalPressure(){
  JsonDocument doc;
  char buffer[256];
  doc["pressure"] = serialized(String(press,1));;
  size_t n = serializeJson(doc, buffer);
  mqttClient.publish("P2/press", buffer, n);
}

void submitLocalLuminosity(){
  JsonDocument doc;
  char buffer[256];
  doc["luminosity"] = serialized(String(luminosity));
  size_t n = serializeJson(doc, buffer);
  mqttClient.publish("P2/lumin", buffer, n);
}

void refreshDisplay(){
  if ((timeinfo.tm_hour>=night_start) && (timeinfo.tm_hour<=night_end))
    clear_screen();
  else {
    set_brighteness();
    print_screen_1();  
  }
}

void setup() {

  //setup_serial();
  
  setup_sensors();

  setup_wifi();

  setup_time();

  setup_mqtt();
}

void loop() {
  currentMillis = millis();  

  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillisWifi >= interval_reconnect)) {
    previousMillisWifi = currentMillis;
    reconnect_WiFi();
  }

  if ((!mqttClient.connected()) && (currentMillis - previousMillisMQ >= interval_reconnect)) {
    previousMillisMQ = currentMillis;
    reconnect_MQTT();
  }

  mqttClient.loop();


  if (currentMillis - previousMillisRefresh >= interval_refresh) {
    previousMillisRefresh = currentMillis;
    refreshLocalTime();
    refreshLocalTempHumid();
    refreshLocalPressure();
    refreshLuminosity();
    refreshDisplay();

  }


  if (currentMillis - previousMillisSend >= interval_send) {
    previousMillisSend = currentMillis;
    submitLocalTempHumid();
    submitLocalPressure();
    submitLocalLuminosity();
  }

}