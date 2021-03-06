#ifndef _DEV_CONFIG_H_

#define _DEV_CONFIG_H_

#include <Preferences.h>

// #define DEVICE_TYPE "IOT-GW-V1"

// #define DEVICE_TYPE "IOT-GW-V1-MODBUS-WIFI"
// #define SD_CARD_ENABLED
// #define LCD_ENABLED
// #define DHT_ENABLED
// #define MODBUS_ENABLED

#define DEVICE_TYPE "IOT-GW-V1-MODBUS-GSM"
#define SD_CARD_ENABLED
#define GSM_ENABLED
#define MODBUS_ENABLED

#define HW_VERSION 1
#define SW_VERSION 1

#define HEARTBEAT_DATA_PATH "/api/heartbeat/"
#define DATA_PATH "/api/data/"
#define WEBSOCKET_PATH "/iotgw1/"

#define MAX_CONNECTING_TIME_SECONDS 300

// SD card
// #define SD_CARD_ENABLED
#define SD_CS_PIN 5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_CLK_PIN 18
#define SD_FILE_CHANGE_SECONDS 3600

// GSM
// #define GSM_ENABLED
#define GSM_POLL_TIME 40
#define TINY_GSM_MODEM_SIM800
#define GSM_BAUDRATE 115200
#define GSM_RX_PIN 26
#define GSM_TX_PIN 27
#define GSM_RST_PIN 4 // 5
#define GSM_POWERKEY_PIN 2 // 4
#define GSM_POWER_PIN 2 // 23
// Leave empty, if missing user or pass
#define GSM_APN  ""
#define GSM_USER ""
#define GSM_PASS ""

// LCD
// #define LCD_ENABLED
#define UI_UPDATE_SECONDS 0

// Digital pin connected to the DHT sensor
// #define DHT_ENABLED
#define DHT_PIN 13
#define DHT_TYPE DHT11

enum DevWorkModes {
  SIMPLE_CLIENT = 0,
  LAZY_CLIENT,
};

#define ENABLE_SERIAL 1
#define DEFAULT_HEARTBEAT_FREQ_SECONDS 60
#define DEFAULT_SERVER_IP "data.okosengineering.com"
#define DEFAULT_SERVER_PORT 80
#define DEFAULT_OTA_ADMIN_PASS "admin"

#define DEFAULT_TIMESTAMP 1652997600 // 25 May 22

// LED stuff
#define LED_PIN 2
#define BUTTON_PIN 12

// Meter pins
#define METER_1_PIN 33
#define METER_2_PIN 32
#define METER_3_PIN 35
#define METER_4_PIN 34
#define METER_5_PIN 36
#define METER_6_PIN 39

// Modbus
// #define MODBUS_ENABLED
#define MAX_MODBUS_SLAVES 3
#define MAX_ADDRESSES_PER_MODBUS_SLAVE 5
#define MODBUS_BAUDRATE 9600
#define MODBUS_RX_PIN 16
#define MODBUS_TX_PIN 17
#define MODBUS_RTS_PIN 14

typedef struct {
  uint8_t func_code;
  uint16_t address;
  uint8_t length;
} modbus_address_t;

typedef struct {
  uint8_t hw_ver;
  uint8_t sw_ver;
  uint16_t dev_id;

  uint8_t work_mode;
  uint16_t heartbeat_freq;
  uint32_t timestamp;
  char server_ip[50];
  uint16_t server_port;

  float cal_coefficients[6];
  float cal_offsets[6];

  uint8_t modbus_enabled;
  uint8_t modbus_slave_ids[MAX_MODBUS_SLAVES];
  modbus_address_t modbus_address[MAX_MODBUS_SLAVES][MAX_ADDRESSES_PER_MODBUS_SLAVE];

} config_t;

#define config_length sizeof(config_t)


class DevConfig {

  private:
  Preferences *prefs;

  public:
    config_t data;

    DevConfig() {
      this->prefs = NULL;
      data.timestamp = DEFAULT_TIMESTAMP;
    }

    DevConfig(Preferences *prf) {
      this->prefs = prf;
      get_config();

      if (data.work_mode > DevWorkModes::LAZY_CLIENT) {
        data.work_mode = DevWorkModes::SIMPLE_CLIENT;
      }

      if (data.heartbeat_freq == 0 || data.heartbeat_freq > 600) {
        data.heartbeat_freq = DEFAULT_HEARTBEAT_FREQ_SECONDS;
      }

      if (data.timestamp < DEFAULT_TIMESTAMP) {
        data.timestamp = DEFAULT_TIMESTAMP;
      }

      if (!is_valid_server_ip()) {
        Serial.println("Invalid server data. Setting to default.");
        Serial.println(data.server_ip);
        // set_default_config();
      }

    };

    boolean is_valid_server_ip() {
      boolean is_valid = 1;
      uint8_t length = strlen(data.server_ip);

      if (length < 10 || length > 50 || data.server_port == 0)
        return false;

      for(uint8_t i=0;i<length;i++) {
        uint8_t is_alphabet = (data.server_ip[i] >= 'a' && data.server_ip[i] <= 'z') ? 1 : 0;
        uint8_t is_num = (data.server_ip[i] >= '0' && data.server_ip[i] <= '9') ? 1 : 0;
        uint8_t is_valid_char = (data.server_ip[i] == '.' || data.server_ip[i] == '_')  ? 1 : 0;
        is_valid = (is_alphabet + is_num + is_valid_char) > 0 ? true : false;
        if (!is_valid) {
          break;
        }
      }

      return is_valid;
    }

    char* get_server_ip() {
      if (is_valid_server_ip()) {
        return data.server_ip;
      }
      return DEFAULT_SERVER_IP;
    }

    uint16_t get_server_port() {
      if (is_valid_server_ip()) {
        return data.server_port;
      }
      return DEFAULT_SERVER_PORT;
    }

    char* get_ota_admin_pass() {
      return DEFAULT_OTA_ADMIN_PASS;
    }

    void set_default_config() {
      strcpy(data.server_ip, DEFAULT_SERVER_IP);
      data.server_port = DEFAULT_SERVER_PORT;

      data.hw_ver = HW_VERSION;
      data.sw_ver = SW_VERSION;
      data.dev_id = 0;
      data.work_mode = DevWorkModes::SIMPLE_CLIENT;
      data.heartbeat_freq = DEFAULT_HEARTBEAT_FREQ_SECONDS;
      data.timestamp = DEFAULT_TIMESTAMP;

      for (uint8_t i=0; i < 6; i++) {
        data.cal_coefficients[i] = 0;
        data.cal_offsets[i] = 0;
      }

      set_config();
    }

    void get_config(void) {
      Serial.println("get config");

      this->prefs->begin("config");
      data.hw_ver = this->prefs->getChar("hwVer");
      data.sw_ver = this->prefs->getChar("swVer");
      data.dev_id = this->prefs->getUShort("devId");
      data.work_mode = this->prefs->getChar("workMode");
      data.heartbeat_freq = this->prefs->getUShort("heartbeatFreq");
      data.timestamp = this->prefs->getUInt("timestamp");

      String st = this->prefs->getString("serverIp", "");
      st.toCharArray(data.server_ip, 50);

      data.server_port = this->prefs->getUShort("serverPort");

      char key[10];
      strcpy(key, "cof0");
      for (uint8_t i=0; i < 6; i++) {
        key[3] = '0' + i;
        data.cal_coefficients[i] = this->prefs->getFloat(key, 0);
      }
      strcpy(key, "off0");
      for (uint8_t i=0; i < 6; i++) {
        key[3] = '0' + i;
        data.cal_offsets[i] = this->prefs->getFloat(key, 0);
      }

      data.modbus_enabled = this->prefs->getChar("modbusEnabled");

      strcpy(key, "mdb0");
      for (uint8_t i=0; i < MAX_MODBUS_SLAVES; i++) {
        key[3] = '0' + i;
        key[4] = '\0';
        data.modbus_slave_ids[i] = this->prefs->getChar(key);

        for (uint8_t k=0; k < MAX_ADDRESSES_PER_MODBUS_SLAVE; k++) {
          key[4] = '0' + k;
          key[6] = '\0';
          key[5] = 'f';
          data.modbus_address[i][k].func_code = this->prefs->getChar(key);
          key[5] = 'a';
          data.modbus_address[i][k].address = this->prefs->getUShort(key);
          key[5] = 'l';
          data.modbus_address[i][k].length = this->prefs->getChar(key);
        }
      }
      this->prefs->end();
    }

    void set_timestamp() {
      this->prefs->putUInt("timestamp", data.timestamp);
    }

    void set_config(void) {
      Serial.println("set config");
      this->prefs->begin("config", false);

      if (!is_valid_server_ip()) {
        Serial.println("Invalid server data. Setting to default.");
        Serial.println(data.server_ip);
        strcpy(data.server_ip, DEFAULT_SERVER_IP); 
        data.server_port = DEFAULT_SERVER_PORT;
      }
      this->prefs->clear();
      this->prefs->putChar("hwVer", data.hw_ver);
      this->prefs->putChar("swVer", data.sw_ver);
      this->prefs->putUShort("devId", data.dev_id);
      this->prefs->putChar("workMode", data.work_mode);
      this->prefs->putUShort("heartbeatFreq", data.heartbeat_freq);

      String st = String(data.server_ip);
      this->prefs->putString("serverIp", st);
      this->prefs->putUShort("serverPort", data.server_port);

      char key[10];
      strcpy(key, "cof0");
      for (uint8_t i=0; i < 6; i++) {
        key[3] = '0' + i;
        this->prefs->putFloat(key, data.cal_coefficients[i]);
      }
      strcpy(key, "off0");
      for (uint8_t i=0; i < 6; i++) {
        key[3] = '0' + i;
        this->prefs->putFloat(key, data.cal_offsets[i]);
      }

      this->prefs->putChar("modbusEnabled", data.modbus_enabled);
      strcpy(key, "mdb0");
      for (uint8_t i=0; i < MAX_MODBUS_SLAVES; i++) {
        key[3] = '0' + i;
        key[4] = '\0';
        this->prefs->putChar(key, data.modbus_slave_ids[i]);

        for (uint8_t k=0; k < MAX_ADDRESSES_PER_MODBUS_SLAVE; k++) {
          key[4] = '0' + k;
          key[6] = '\0';
          key[5] = 'f';
          this->prefs->putChar(key, data.modbus_address[i][k].func_code);
          key[5] = 'a';
          this->prefs->putUShort(key, data.modbus_address[i][k].address);
          key[5] = 'l';
          this->prefs->putChar(key, data.modbus_address[i][k].length);
        }
      }
      this->prefs->end();
    };
};

#endif