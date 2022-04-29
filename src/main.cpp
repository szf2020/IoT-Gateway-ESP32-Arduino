#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
// #include <WiFiUdp.h>
#include "wifi_events.h"
#include <ArduinoWebsockets.h>

#include <Preferences.h>
#include <ArduinoOTA.h>

#include "config.cpp"
#include "meters.cpp"
#include "dht.cpp"
#include "server_sync.cpp"
// #include "lcd.cpp"

// LED stuff
#define LED_PIN 18
#define BUTTON_PIN 19
#define DHTPIN 23 // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11
uint8_t led_status = 0;
uint8_t button_status = 0;

// Serial
uint8_t serial_state = 0;
#define ENABLE_SERIAL 1

Preferences preferences;
DevConfig dev_config;
ServerSync server_sync;
Meter meter;
DHT dht(DHTPIN, DHTTYPE);
DHTMeter dht_meter;

enum WiFiState
{
  InIt = 1,
  ConnectingAp,
  SmartConfigSetup,
  ConnectedAP,
  ConnectedToServer
};

uint8_t wifi_state = InIt;
IPAddress ip;
char mac_address[20];

using namespace websockets;

WebsocketsClient web_socket_client;
// WiFiUDP udp_client;

#ifdef LCD_ENABLED
// LCD lcd;
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);
#endif

char ui_buffer[20];
uint8_t ui_state = 0;
uint8_t ui_update_counter = 0;
#define UI_UPDATE_SECONDS 1

char* prepare_data_buffer() {

  ip = WiFi.localIP();
  sprintf(
    server_sync.shared_buffer,
    "{"
    "\"config\": { \"hwVer\": %hu, \"swVer\": %hu, \"devId\": %u, \"mac\": \"%s\" },"
    "\"adc\": { \"1\": %u, \"2\": %u, \"3\": %u, \"4\": %u, \"5\": %u, \"6\": %u },"
    "\"dht\": { \"state\": %hu, \"temperature\": %.2f, \"humidity\": %.2f, \"hic\": %.2f },"
    "\"meters\": { \"1\": %.2f, \"2\": %.2f, \"3\": %.2f, \"4\": %.2f, \"5\": %.2f, \"6\": %.2f },"
    "\"network\": { \"state\": %hu, \"ip\": \"%hu.%hu.%hu.%hu\", \"tts\": %u }"
    "}",
    dev_config.data.hw_ver, dev_config.data.sw_ver, dev_config.data.dev_id, mac_address,
    meter.adc_rms_values[0], meter.adc_rms_values[1], meter.adc_rms_values[2],
    meter.adc_rms_values[3], meter.adc_rms_values[4], meter.adc_rms_values[5],
    dht_meter.dht_state, dht_meter.temperature, dht_meter.humidity, dht_meter.hic,
    meter.meter_values[0], meter.meter_values[1], meter.meter_values[2],
    meter.meter_values[3], meter.meter_values[4], meter.meter_values[5],
    wifi_state, ip[0], ip[1], ip[2], ip[3], server_sync.get_time_since_sync()
  );

  return server_sync.shared_buffer;

}

char* prepare_config_data_buffer() {

  ip = WiFi.localIP();
  sprintf(
      server_sync.shared_buffer,
      "{\"config\": {"
      "\"hwVer\": %hu, \"swVer\": %hu, \"devId\": %u, \"mac\": \"%s\","
      "\"workMode\": %hu, \"heartbeatFreq\": %u,"
      "\"serverIp\": \"%s\", \"serverPort\": %u,"
      "\"calCoefficients\": [%.2f, %.2f, %.2f, %.2f, %.2f, %.2f],"
      "\"calOffsets\": [%.2f, %.2f, %.2f, %.2f, %.2f, %.2f]"
      "}}",
      dev_config.data.hw_ver, dev_config.data.sw_ver, dev_config.data.dev_id, mac_address,
      dev_config.data.work_mode, dev_config.data.heartbeat_freq,
      dev_config.data.server_ip, dev_config.data.server_port,
      dev_config.data.cal_coefficients[0], dev_config.data.cal_coefficients[1], dev_config.data.cal_coefficients[2],
      dev_config.data.cal_coefficients[3], dev_config.data.cal_coefficients[4], dev_config.data.cal_coefficients[5],
      dev_config.data.cal_offsets[0], dev_config.data.cal_offsets[1], dev_config.data.cal_offsets[2],
      dev_config.data.cal_offsets[3], dev_config.data.cal_offsets[4], dev_config.data.cal_offsets[5]
  );

  return server_sync.shared_buffer;

}

void display_message_lcd(uint8_t lineno, char *message, uint8_t clear_lcd=0) {
  #ifdef LCD_ENABLED
  uint8_t end_detected = 0;
  for (uint8_t i=0; i<16; i++) {
    if (message[i] == '\0') {
      end_detected = 1;
      break;
    }
  }
  vTaskDelay(10);
  if (clear_lcd == 1) {
    lcd.clear();
    vTaskDelay(10);
    lcd.noBacklight();
    vTaskDelay(10);
  }
  if (end_detected == 0 || lineno > 1) {
    lcd.setCursor(0, 0);
    vTaskDelay(10);
    lcd.printstr("msg invld");
  } else {
    lcd.setCursor(0, lineno);
    vTaskDelay(10);
    lcd.printstr(message);
  }
  #endif
}

void lcd_init(uint8_t display_message=0) {
  #ifdef LCD_ENABLED
  vTaskDelay(10);
  lcd.init();
  vTaskDelay(10);
  if (display_message == 1) {
    lcd.noBacklight();
    vTaskDelay(10);
    display_message_lcd(0, "IoT Gateway", 1);
  }
  #endif
}

void clear_lcd() {
  #ifdef LCD_ENABLED
  lcd.clear();
  vTaskDelay(10);
  lcd.noBacklight();
  vTaskDelay(10);
  Serial.println("LCD clear");
  #endif
}

void update_user_interface(void *params)
{
  char *buffer;
  if (serial_state == 0)
  {
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);
    digitalWrite(LED_PIN, LOW);
  }

  if (ENABLE_SERIAL > 0 && serial_state == 0)
  {
    Serial.begin(115200);
    serial_state = 1;
  }

  while (1)
  {
    if (serial_state == 1)
    {
      buffer = prepare_config_data_buffer();
      Serial.println(buffer);
      buffer = prepare_data_buffer();
      Serial.println(buffer);
    }

    for (uint8_t k = 0; k < wifi_state; k++)
    {
      digitalWrite(LED_PIN, HIGH);
      vTaskDelay(100);
      digitalWrite(LED_PIN, LOW);
      vTaskDelay(100);
    }

    if (digitalRead(BUTTON_PIN) == HIGH) {
      Serial.println("Button pressed");
      button_status += 1;
      if (button_status > 5) {
        Serial.println("SmartConfigSetup");
        web_socket_client.close();
        WiFi.disconnect();
        wifi_state = SmartConfigSetup;
        button_status = 0;
      }
    }

    #ifdef LCD_ENABLED
    if (ui_update_counter <= 0) {
      ui_update_counter = UI_UPDATE_SECONDS;
      if (ui_state == 0) {
        lcd_init(1);
        ui_state = 1;
      } else if (ui_state == 1) {
        snprintf(ui_buffer, 16, "ID:%u", dev_config.data.dev_id);
        display_message_lcd(0, ui_buffer, 1);
        Serial.println(ui_buffer);
        // lcd.display_message(0, ui_buffer);
        snprintf(ui_buffer, 16, "Ver:%hu.%hu", dev_config.data.hw_ver, dev_config.data.sw_ver);
        // lcd.display_message(1, ui_buffer);
        display_message_lcd(1, ui_buffer);
        Serial.println(ui_buffer);
        ui_state = 2;
      } else if (ui_state == 2) {
        snprintf(ui_buffer, 16, "WiFi:%hu %u", wifi_state, server_sync.get_time_since_sync());
        // lcd.display_message(0, ui_buffer);
        display_message_lcd(0, ui_buffer, 1);
        Serial.println(ui_buffer);
        snprintf(ui_buffer, 16, "%hu.%hu.%hu.%hu", ip[0], ip[1], ip[2], ip[3]);
        // lcd.display_message(1, ui_buffer);
        display_message_lcd(1, ui_buffer);
        Serial.println(ui_buffer);
        ui_state = 3;
      } else if (ui_state == 3) {
        snprintf(ui_buffer, 16, "DHT:%hu",  dht_meter.dht_state);
        // lcd.display_message(0, ui_buffer);
        display_message_lcd(0, ui_buffer, 1);
        Serial.println(ui_buffer);
        snprintf(ui_buffer, 16, "T:%.1f H:%.1f", dht_meter.temperature, dht_meter.humidity);
        // lcd.display_message(1, ui_buffer);
        display_message_lcd(1, ui_buffer);
        Serial.println(ui_buffer);
        ui_state = 4;
      } else if (ui_state == 4) {
        snprintf(ui_buffer, 16, "M1:%u", meter.adc_rms_values[0]);
        // lcd.display_message(0, ui_buffer);
        display_message_lcd(0, ui_buffer, 1);
        Serial.println(ui_buffer);
        snprintf(ui_buffer, 16, "M2:%u", meter.adc_rms_values[1]);
        // lcd.display_message(1, ui_buffer);
        display_message_lcd(1, ui_buffer);
        Serial.println(ui_buffer);
        ui_state = 5;
      } else if (ui_state == 5) {
        snprintf(ui_buffer, 16, "M3:%u", meter.adc_rms_values[2]);
        // lcd.display_message(0, ui_buffer);
        display_message_lcd(0, ui_buffer, 1);
        Serial.println(ui_buffer);
        snprintf(ui_buffer, 16, "M4:%u", meter.adc_rms_values[3]);
        // lcd.display_message(1, ui_buffer);
        display_message_lcd(1, ui_buffer);
        Serial.println(ui_buffer);
        ui_state = 6;
      } else if (ui_state == 6) {
        snprintf(ui_buffer, 16, "M5:%u", meter.adc_rms_values[4]);
        // lcd.display_message(0, ui_buffer);
        display_message_lcd(0, ui_buffer, 1);
        Serial.println(ui_buffer);
        snprintf(ui_buffer, 16, "M6:%u", meter.adc_rms_values[5]);
        // lcd.display_message(1, ui_buffer);
        display_message_lcd(1, ui_buffer);
        Serial.println(ui_buffer);
        ui_state = 0;
      }
    } else {
      ui_update_counter -= 1;
    }
    #endif

    vTaskDelay(3000);
  }
  /* delete a task when finish,
  this will never happen because this is infinity loop */
  vTaskDelete(NULL);
}

void update_meters(void *params)
{
  while (1)
  {
    meter.read_voltage();
    meter.read_current();
    vTaskDelay(10);
  }
  /* delete a task when finish,
  this will never happen because this is infinity loop */
  vTaskDelete(NULL);
}

void update_dht(void *params)
{

  while (1)
  {
    dht_meter.read();
    vTaskDelay(10000);
  }
  /* delete a task when finish,
  this will never happen because this is infinity loop */
  vTaskDelete(NULL);
}

// wifi event handler
void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    //When connected set
    //initializes the UDP state
    //This initializes the transfer buffer
    // udp.begin(WiFi.localIP(), udpPort);
    wifi_state = ConnectedAP;
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    if (wifi_state != SmartConfigSetup) {
      wifi_state = InIt;
    }
    break;
  default:
    break;
  }
}

void initialise_wifi(void *params)
{

  while (1)
  {

    if (wifi_state == InIt)
    {
      WiFi.setSleep(false);
      //register event handler
      WiFi.onEvent(WiFiEvent);
      WiFi.begin();
      WiFi.macAddress().toCharArray(mac_address, 20, 0);
      wifi_state = ConnectingAp;
    }

    if (wifi_state == SmartConfigSetup)
    {
      WiFi.mode(WIFI_AP_STA);
      WiFi.beginSmartConfig();
      //Wait for SmartConfig packet from mobile
      while (!WiFi.smartConfigDone())
      {
        vTaskDelay(500);
      }
      wifi_state = InIt;
    }

    if (wifi_state == ConnectedAP)
    {
      vTaskDelay(60000);
      if (WiFi.status() != WL_CONNECTED)
      {
        wifi_state = ConnectingAp;
      }
    }
  }

  vTaskDelete(NULL);
}

void onMessageCallback(WebsocketsMessage message)
{
  Serial.print("Got Message: ");
  Serial.println(message.data());
  uint8_t cmd = server_sync.process_response(message);

  if (cmd == 0) {

  } else if (cmd == 1 || cmd == 4) {
    char *buffer = prepare_data_buffer();
    web_socket_client.send(buffer);
  } else if (cmd == 2 || cmd == 3) {
    char *buffer = prepare_config_data_buffer();
    web_socket_client.send(buffer);
    // Config is updated. Reinitialize the web socket.
    web_socket_client.close();
    wifi_state = ConnectedAP;
  }
}

void onEventsCallback(WebsocketsEvent event, String data)
{
  if (event == WebsocketsEvent::ConnectionOpened)
  {
    Serial.println("Connection Opened");
    wifi_state = ConnectedToServer;
  }
  else if (event == WebsocketsEvent::ConnectionClosed)
  {
    Serial.println("Connection Closed");
    wifi_state = ConnectedAP;
  }
  else if (event == WebsocketsEvent::GotPing)
  {
    Serial.println("Got a Ping!");
    web_socket_client.pong();
    wifi_state = ConnectedToServer;
  }
  else if (event == WebsocketsEvent::GotPong)
  {
    Serial.println("Got a Pong!");
    web_socket_client.ping();
    wifi_state = ConnectedToServer;
  }
}

void update_websocket(void *params)
{
  char *buffer;
  while (1)
  {
    web_socket_client.poll();
    if (wifi_state == ConnectedAP) {
      web_socket_client.connect(
        dev_config.get_server_ip(),
        dev_config.get_server_port(),
        "/iotgw1/"
      );
    }

    if (server_sync.time_to_send() && wifi_state == ConnectedToServer)
    {
      buffer = server_sync.get_heartbeat();
      web_socket_client.send(buffer);
      server_sync.send_success();
    }
    vTaskDelay(1000);
  }
  /* delete a task when finish,
  this will never happen because this is infinity loop */
  vTaskDelete(NULL);
}

void setup()
{
  dev_config = DevConfig(&preferences);
  server_sync = ServerSync(&dev_config);
  meter = Meter(&dev_config);

  dht.begin();
  dht_meter = DHTMeter(&dht);

  lcd_init(1);

  xTaskCreate(
      initialise_wifi,   /* Task function. */
      "initialise_wifi", /* name of task. */
      10000,              /* Stack size of task */
      NULL,              /* parameter of the task */
      10,                /* priority of the task */
      NULL);             /* Task handle to keep track of created task */

  xTaskCreate(
      update_meters,     /* Task function. */
      "update_meters",   /* name of task. */
      3000,              /* Stack size of task */
      NULL,              /* parameter of the task */
      9,                 /* priority of the task */
      NULL);             /* Task handle to keep track of created task */

  xTaskCreate(
      update_dht,        /* Task function. */
      "update_dht",      /* name of task. */
      2000,              /* Stack size of task */
      NULL,              /* parameter of the task */
      7,                 /* priority of the task */
      NULL);             /* Task handle to keep track of created task */

  xTaskCreate(
      update_user_interface,     /* Task function. */
      "update_user_interface",   /* name of task. */
      3000,              /* Stack size of task */
      NULL,              /* parameter of the task */
      6,                 /* priority of the task */
      NULL);             /* Task handle to keep track of created task */

  // run callback when messages are received
  web_socket_client.onMessage(onMessageCallback);
  // run callback when events are occuring
  web_socket_client.onEvent(onEventsCallback);

  xTaskCreate(
      update_websocket,   /* Task function. */
      "update_websocket", /* name of task. */
      80000,              /* Stack size of task */
      NULL,               /* parameter of the task */
      8,                  /* priority of the task */
      NULL);              /* Task handle to keep track of created task */

  ArduinoOTA.setPassword(dev_config.get_ota_admin_pass());

  ArduinoOTA
      .onStart([]()
               {
                 String type;
                 if (ArduinoOTA.getCommand() == U_FLASH)
                   type = "sketch";
                 else // U_SPIFFS
                   type = "filesystem";

                 // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                 Serial.println("Start updating " + type);
               })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
                 Serial.printf("Error[%u]: ", error);
                 if (error == OTA_AUTH_ERROR)
                   Serial.println("Auth Failed");
                 else if (error == OTA_BEGIN_ERROR)
                   Serial.println("Begin Failed");
                 else if (error == OTA_CONNECT_ERROR)
                   Serial.println("Connect Failed");
                 else if (error == OTA_RECEIVE_ERROR)
                   Serial.println("Receive Failed");
                 else if (error == OTA_END_ERROR)
                   Serial.println("End Failed");
               });

  ArduinoOTA.begin();
}

void loop()
{
  if (wifi_state >= ConnectedAP) {
    ArduinoOTA.handle();
  }
}