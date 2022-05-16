#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>
#include <Adafruit_Sensor.h>
// #include <WiFiUdp.h>
// #include "wifi_events.h"

#include <Preferences.h>
#include <ArduinoOTA.h>

#include "config.cpp"
#include "meters.cpp"
#include "server_sync.cpp"

#ifdef DHT_ENABLED
#include <DHT.h>
#include "dht.cpp"
#endif

#ifdef LCD_ENABLED
#include "lcd.cpp"
#endif

#ifdef MODBUS_ENABLED
#include "modbushandler.cpp"
#endif

#ifdef GSM_ENABLED
#include <TinyGsmClient.h>
#include "gsm.cpp"
#endif

uint8_t ota_in_progress = 0;
uint8_t led_status = 0;
uint8_t button_status = 0;

// Serial
uint8_t serial_state = 0;

Preferences preferences;
DevConfig dev_config;
ServerSync server_sync;
Meter meter;

#ifdef DHT_ENABLED
DHT dht(DHT_PIN, DHT_TYPE);
DHTMeter dht_meter;
#endif

#ifdef MODBUS_ENABLED
ModbusHandler modbus_handler;
#endif

#ifdef GSM_ENABLED
GSM gsm;
TinyGsm gsm_modem(Serial2);
TinyGsmClient gsm_client(gsm_modem);
uint16_t gsm_update_counter = 0;
#endif

enum WiFiState
{
  InIt = 1,
  ConnectingAp,
  SmartConfigSetup,
  ConnectedAP
};

uint8_t wifi_state = InIt;
IPAddress ip;
char mac_address[20];

using namespace websockets;

WebsocketsClient web_socket_client;
// WiFiUDP udp_client;

#ifdef LCD_ENABLED
LCD lcd;
#endif

char ui_buffer[20];
uint8_t ui_state = 0;
uint8_t ui_update_counter = 0;

void update_ip_address() {
#ifdef GSM_ENABLED
  if (gsm.state == GSMState::GPRSConnected) {
    ip = gsm_modem.localIP();
  } else {
    ip = WiFi.localIP();
  }
#else
  ip = WiFi.localIP();
#endif
}

char* prepare_data_buffer() {

  update_ip_address();

#ifdef DHT_ENABLED
  sprintf(
    server_sync.shared_buffer,
    "{"
    "\"config\": { \"hwVer\": %hu, \"swVer\": %hu, \"devId\": %u, \"workmode\": %hu, \"mac\": \"%s\" },"
    "\"adc\": { \"1\": %u, \"2\": %u, \"3\": %u, \"4\": %u, \"5\": %u, \"6\": %u },"
    "\"dht\": { \"state\": %hu, \"temperature\": %.2f, \"humidity\": %.2f, \"hic\": %.2f },"
    "\"meters\": { \"1\": %.2f, \"2\": %.2f, \"3\": %.2f, \"4\": %.2f, \"5\": %.2f, \"6\": %.2f },"
    "\"network\": { \"state\": %hu, \"ip\": \"%hu.%hu.%hu.%hu\", \"tts\": %u }"
    "}",
    dev_config.data.hw_ver, dev_config.data.sw_ver, dev_config.data.dev_id, dev_config.data.work_mode, mac_address,
    meter.adc_rms_values[0], meter.adc_rms_values[1], meter.adc_rms_values[2],
    meter.adc_rms_values[3], meter.adc_rms_values[4], meter.adc_rms_values[5],
    dht_meter.dht_state, dht_meter.temperature, dht_meter.humidity, dht_meter.hic,
    meter.meter_values[0], meter.meter_values[1], meter.meter_values[2],
    meter.meter_values[3], meter.meter_values[4], meter.meter_values[5],
    wifi_state, ip[0], ip[1], ip[2], ip[3], server_sync.get_time_since_sync()
  );
#else
  sprintf(
    server_sync.shared_buffer,
    "{"
    "\"config\": { \"hwVer\": %hu, \"swVer\": %hu, \"devId\": %u, \"workmode\": %hu, \"mac\": \"%s\" },"
    "\"adc\": { \"1\": %u, \"2\": %u, \"3\": %u, \"4\": %u, \"5\": %u, \"6\": %u },"
    "\"meters\": { \"1\": %.2f, \"2\": %.2f, \"3\": %.2f, \"4\": %.2f, \"5\": %.2f, \"6\": %.2f },"
    "\"network\": { \"state\": %hu, %hu, %hu, \"ip\": \"%hu.%hu.%hu.%hu\", \"tts\": %u }"
    "}",
    dev_config.data.hw_ver, dev_config.data.sw_ver, dev_config.data.dev_id, dev_config.data.work_mode, mac_address,
    meter.adc_rms_values[0], meter.adc_rms_values[1], meter.adc_rms_values[2],
    meter.adc_rms_values[3], meter.adc_rms_values[4], meter.adc_rms_values[5],
    meter.meter_values[0], meter.meter_values[1], meter.meter_values[2],
    meter.meter_values[3], meter.meter_values[4], meter.meter_values[5],
    wifi_state, gsm.state, server_sync.state, ip[0], ip[1], ip[2], ip[3], server_sync.get_time_since_sync()
  );
#endif
  return server_sync.shared_buffer;
}

char* prepare_modbus_data_buffer(char *modbus_data) {
  sprintf(
    server_sync.shared_buffer,
    "{\"config\":{\"hwVer\":%hu,\"swVer\":%hu,\"devId\":%u,\"workmode\":%hu,\"mac\":\"%s\"},\"modbus_data\":%s}",
    dev_config.data.hw_ver, dev_config.data.sw_ver, dev_config.data.dev_id, dev_config.data.work_mode, mac_address,
    modbus_data
  );
  return server_sync.shared_buffer;
}

char* prepare_config_data_buffer() {
  update_ip_address();
  sprintf(
      server_sync.shared_buffer,
      "{\"config\": {"
      "\"hwVer\": %hu, \"swVer\": %hu, \"devId\": %u, \"mac\": \"%s\","
      "\"workMode\": %hu, \"heartbeatFreq\": %u,"
      "\"serverIp\": \"%s\", \"serverPort\": %u,"
      "\"calCoefficients\": [%.2f, %.2f, %.2f, %.2f, %.2f, %.2f],"
      "\"calOffsets\": [%.2f, %.2f, %.2f, %.2f, %.2f, %.2f],"
      "\"modbus\": {\"enabled\": %hu, \"slaves\": [%hu, %hu, %hu],"
      "\"slave_1\": [[%hu,%u,%hu],[%hu,%u,%hu],[%hu,%u,%hu],[%hu,%u,%hu],[%hu,%u,%hu]],"
      "\"slave_2\": [[%hu,%u,%hu],[%hu,%u,%hu],[%hu,%u,%hu],[%hu,%u,%hu],[%hu,%u,%hu]],"
      "\"slave_3\": [[%hu,%u,%hu],[%hu,%u,%hu],[%hu,%u,%hu],[%hu,%u,%hu],[%hu,%u,%hu]]"
      "}}}",
      dev_config.data.hw_ver, dev_config.data.sw_ver, dev_config.data.dev_id, mac_address,
      dev_config.data.work_mode, dev_config.data.heartbeat_freq,
      dev_config.data.server_ip, dev_config.data.server_port,
      dev_config.data.cal_coefficients[0], dev_config.data.cal_coefficients[1], dev_config.data.cal_coefficients[2],
      dev_config.data.cal_coefficients[3], dev_config.data.cal_coefficients[4], dev_config.data.cal_coefficients[5],
      dev_config.data.cal_offsets[0], dev_config.data.cal_offsets[1], dev_config.data.cal_offsets[2],
      dev_config.data.cal_offsets[3], dev_config.data.cal_offsets[4], dev_config.data.cal_offsets[5],
      dev_config.data.modbus_enabled, dev_config.data.modbus_slave_ids[0], dev_config.data.modbus_slave_ids[1], dev_config.data.modbus_slave_ids[2],
      dev_config.data.modbus_address[0][0].func_code, dev_config.data.modbus_address[0][0].address, dev_config.data.modbus_address[0][0].length,
      dev_config.data.modbus_address[0][1].func_code, dev_config.data.modbus_address[0][1].address, dev_config.data.modbus_address[0][1].length,
      dev_config.data.modbus_address[0][2].func_code, dev_config.data.modbus_address[0][2].address, dev_config.data.modbus_address[0][2].length,
      dev_config.data.modbus_address[0][3].func_code, dev_config.data.modbus_address[0][3].address, dev_config.data.modbus_address[0][3].length,
      dev_config.data.modbus_address[0][4].func_code, dev_config.data.modbus_address[0][4].address, dev_config.data.modbus_address[0][4].length,
      dev_config.data.modbus_address[1][0].func_code, dev_config.data.modbus_address[1][0].address, dev_config.data.modbus_address[1][0].length,
      dev_config.data.modbus_address[1][1].func_code, dev_config.data.modbus_address[1][1].address, dev_config.data.modbus_address[1][1].length,
      dev_config.data.modbus_address[1][2].func_code, dev_config.data.modbus_address[1][2].address, dev_config.data.modbus_address[1][2].length,
      dev_config.data.modbus_address[1][3].func_code, dev_config.data.modbus_address[1][3].address, dev_config.data.modbus_address[1][3].length,
      dev_config.data.modbus_address[1][4].func_code, dev_config.data.modbus_address[1][4].address, dev_config.data.modbus_address[1][4].length,
      dev_config.data.modbus_address[2][0].func_code, dev_config.data.modbus_address[2][0].address, dev_config.data.modbus_address[2][0].length,
      dev_config.data.modbus_address[2][1].func_code, dev_config.data.modbus_address[2][1].address, dev_config.data.modbus_address[2][1].length,
      dev_config.data.modbus_address[2][2].func_code, dev_config.data.modbus_address[2][2].address, dev_config.data.modbus_address[2][2].length,
      dev_config.data.modbus_address[2][3].func_code, dev_config.data.modbus_address[2][3].address, dev_config.data.modbus_address[2][3].length,
      dev_config.data.modbus_address[2][4].func_code, dev_config.data.modbus_address[2][4].address, dev_config.data.modbus_address[2][4].length
  );

  return server_sync.shared_buffer;

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
    if (ota_in_progress > 0) break;
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
        lcd.lcd_init(1);
        ui_state = 1;
      } else if (ui_state == 1) {
        snprintf(ui_buffer, 16, "WiFi:%hu %u", wifi_state, server_sync.get_time_since_sync());
        // lcd.display_message(0, ui_buffer);
        lcd.display_message_lcd(0, ui_buffer, 1);
        Serial.println(ui_buffer);
        snprintf(ui_buffer, 16, "%hu.%hu.%hu.%hu", ip[0], ip[1], ip[2], ip[3]);
        // lcd.display_message(1, ui_buffer);
        lcd.display_message_lcd(1, ui_buffer);
        Serial.println(ui_buffer);
        ui_state = 2;
      } else if (ui_state == 2) {
#ifdef DHT_ENABLED
        snprintf(ui_buffer, 16, "DHT:%hu",  dht_meter.dht_state);
        lcd.display_message_lcd(0, ui_buffer, 1);
        Serial.println(ui_buffer);
        snprintf(ui_buffer, 16, "T:%.1f H:%.1f", dht_meter.temperature, dht_meter.humidity);
        lcd.display_message_lcd(1, ui_buffer);
        Serial.println(ui_buffer);
#endif
        ui_state = 3;
      } else if (ui_state == 3) {
        snprintf(ui_buffer, 16, "A1:%u M1:%.1f", meter.adc_rms_values[0], meter.meter_values[0]);
        lcd.display_message_lcd(0, ui_buffer, 1);
        Serial.println(ui_buffer);
        snprintf(ui_buffer, 16, "A2:%u M2:%.1f", meter.adc_rms_values[1], meter.meter_values[1]);
        lcd.display_message_lcd(1, ui_buffer);
        Serial.println(ui_buffer);
        ui_state = 4;
      } else if (ui_state == 4) {
        snprintf(ui_buffer, 16, "A3:%u M3:%.1f", meter.adc_rms_values[2], meter.meter_values[2]);
        lcd.display_message_lcd(0, ui_buffer, 1);
        Serial.println(ui_buffer);
        snprintf(ui_buffer, 16, "A4:%u M4:%.1f", meter.adc_rms_values[3], meter.meter_values[3]);
        lcd.display_message_lcd(1, ui_buffer);
        Serial.println(ui_buffer);
        ui_state = 5;
      } else if (ui_state == 5) {
        snprintf(ui_buffer, 16, "A5:%u M5:%.1f", meter.adc_rms_values[4], meter.meter_values[4]);
        lcd.display_message_lcd(0, ui_buffer, 1);
        Serial.println(ui_buffer);
        snprintf(ui_buffer, 16, "A6:%u M6:%.1f", meter.adc_rms_values[5], meter.meter_values[4]);
        lcd.display_message_lcd(1, ui_buffer);
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
    if (ota_in_progress > 0) break;
    meter.read();
    vTaskDelay(10);
  }
  /* delete a task when finish,
  this will never happen because this is infinity loop */
  vTaskDelete(NULL);
}

#ifdef DHT_ENABLED
void update_dht(void *params)
{

  while (1)
  {
    if (ota_in_progress > 0) break;
    dht_meter.read();
    vTaskDelay(10000);
  }
  /* delete a task when finish,
  this will never happen because this is infinity loop */
  vTaskDelete(NULL);
}
#endif

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
    if (ota_in_progress > 0) break;
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
  } else if (cmd == 4) {
    char *buffer = prepare_data_buffer();
    web_socket_client.send(buffer);
    #ifdef MODBUS_ENABLED
    buffer = modbus_handler.update();
    buffer = prepare_modbus_data_buffer(buffer);
    Serial.print("Modbus: ");
    Serial.println(buffer);
    web_socket_client.send(buffer);
    #endif
  } else if (cmd == 2 || cmd == 3 || cmd == 5) {
    char *buffer = prepare_config_data_buffer();
    web_socket_client.send(buffer);
    // Config is updated. Reinitialize the web socket.
    web_socket_client.close();
    server_sync.state = ServerConnectionState::Disconnected;
  }
}

void onEventsCallback(WebsocketsEvent event, String data)
{
  if (event == WebsocketsEvent::ConnectionOpened)
  {
    Serial.println("Connection Opened");
    server_sync.state = ServerConnectionState::Connected;
  }
  else if (event == WebsocketsEvent::ConnectionClosed)
  {
    Serial.println("Connection Closed");
    server_sync.state = ServerConnectionState::Disconnected;
  }
  else if (event == WebsocketsEvent::GotPing)
  {
    Serial.println("Got a Ping!");
    web_socket_client.pong();
    server_sync.state = ServerConnectionState::Connected;
  }
  else if (event == WebsocketsEvent::GotPong)
  {
    Serial.println("Got a Pong!");
    web_socket_client.ping();
    server_sync.state = ServerConnectionState::Connected;
  }
}

void update_websocket(void *params)
{
  char *buffer;
  while (1)
  {

    if (ota_in_progress > 0) break;

    web_socket_client.poll();

#ifdef GSM_ENABLED
    if (server_sync.state == ServerConnectionState::Disconnected && (wifi_state == ConnectedAP || gsm.state == GSMState::GPRSConnected))
#else
    if (server_sync.state == ServerConnectionState::Disconnected && wifi_state == ConnectedAP )
#endif
    {
      Serial.println("Connecting to server...");
      web_socket_client.connect(
        dev_config.get_server_ip(),
        dev_config.get_server_port(),
        "/iotgw1/"
      );
    }

    if (server_sync.time_to_send() && server_sync.state == ServerConnectionState::Connected)
    {
      buffer = server_sync.get_heartbeat();
      web_socket_client.send(buffer);
      server_sync.send_success();
      if (dev_config.data.work_mode == SIMPLE_CLIENT) {
        char *buffer = prepare_data_buffer();
        web_socket_client.send(buffer);
        #ifdef MODBUS_ENABLED
        buffer = modbus_handler.update();
        buffer = prepare_modbus_data_buffer(buffer);
        Serial.print("Modbus: ");
        Serial.println(buffer);
        web_socket_client.send(buffer);
        #endif
      }
    }
    vTaskDelay(1000);
  }
  /* delete a task when finish,
  this will never happen because this is infinity loop */
  vTaskDelete(NULL);
}

void(* reset_device) (void) = 0;//declare reset function at address 0

void setup()
{
  dev_config = DevConfig(&preferences);
  server_sync = ServerSync(&dev_config);
  meter = Meter(&dev_config);
  ota_in_progress = 0;

#ifdef DHT_ENABLED
  dht.begin();
  dht_meter = DHTMeter(&dht);
#endif

#ifdef MODBUS_ENABLED
  modbus_handler.init(&dev_config);
#endif

#ifdef LCD_ENABLED
  lcd = LCD(&dev_config);
  lcd.lcd_init(1);
#endif

#ifdef GSM_ENABLED
  gsm.init(&dev_config, &gsm_modem, &gsm_client);
#endif

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

#ifdef DHT_ENABLED
  xTaskCreate(
      update_dht,        /* Task function. */
      "update_dht",      /* name of task. */
      2000,              /* Stack size of task */
      NULL,              /* parameter of the task */
      7,                 /* priority of the task */
      NULL);             /* Task handle to keep track of created task */
#endif

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
      .onStart([]() {
        ota_in_progress = 1;
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";
        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        // Serial.println("Start updating " + type);
#ifdef LCD_ENABLED
      lcd.display_message_lcd(0, "Starting OTA", 1);
#endif
      })
      .onEnd([]() { 
        ota_in_progress = 0;
        // Serial.println("\nEnd");
#ifdef LCD_ENABLED
      lcd.display_message_lcd(0, "Finished OTA", 1);
#endif
      reset_device();
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        ota_in_progress = (progress / (total / 100));
        // Serial.printf("Progress: %u%%\r", ota_in_progress);
#ifdef LCD_ENABLED
      if (ota_in_progress % 5 == 0) {
        sprintf(ui_buffer, "OTA: %hu%", ota_in_progress);
        lcd.display_message_lcd(0, ui_buffer, 1);
      }
#endif
      })
      .onError([](ota_error_t error) {
        ota_in_progress = 0;
        // Serial.printf("Error[%u]: ", error);
        // if (error == OTA_AUTH_ERROR)
        //   Serial.println("Auth Failed");
        // else if (error == OTA_BEGIN_ERROR)
        //   Serial.println("Begin Failed");
        // else if (error == OTA_CONNECT_ERROR)
        //   Serial.println("Connect Failed");
        // else if (error == OTA_RECEIVE_ERROR)
        //   Serial.println("Receive Failed");
        // else if (error == OTA_END_ERROR)
        //   Serial.println("End Failed");
#ifdef LCD_ENABLED
      sprintf(ui_buffer, "Failed: %u", error);
      lcd.display_message_lcd(0, ui_buffer, 1);
#endif
      reset_device();
      });

      ArduinoOTA.begin();
}

void loop()
{
  ArduinoOTA.handle();

  #ifdef GSM_ENABLED
    if (ota_in_progress == 0 && gsm_update_counter > 65000) {
      gsm_update_counter = 0;
      gsm.check_gprs();
    }
    gsm_update_counter += 1;
    delay(1);
  #endif
}