#ifndef _SERVER_SYNC_H_

#define _SERVER_SYNC_H_

#include <ArduinoWebsockets.h>
#include "config.cpp"

#define HEARTBEAT_CONTENT "HEARTBEAT"
#define HEARTBEAT_ACK "HEARTBEAT_ACK"
#define MAX_MESSAGE_LENGTH 200

using namespace websockets;

class ServerSync
{

private:
  DevConfig *config;
  uint16_t seconds_since_last_sent;

public:
  char shared_buffer[1000];

  ServerSync() {
    config = NULL;
  }

  ServerSync(DevConfig *cfg)
  {
    config = cfg;
    seconds_since_last_sent = 9999;
  };

  char* get_heartbeat()
  {
    sprintf(
      shared_buffer,
      HEARTBEAT_CONTENT
      " [%u]",
      config->data.dev_id
    );
    return shared_buffer;
  }

  boolean time_to_send() {
    boolean send_now = false;
    if (seconds_since_last_sent >= config->data.heartbeat_freq) {
      send_now = true;
      seconds_since_last_sent = config->data.heartbeat_freq;
    }
    seconds_since_last_sent += 1;

    return send_now;
  }

  void send_success() {
    seconds_since_last_sent = 0;
  }

  uint16_t get_time_since_sync() {
    return seconds_since_last_sent;
  }

  uint8_t process_response(WebsocketsMessage message) {

    char message_data[200];
    String message_str;
    if (message.isText()) {
      message_str = message.data();
      message_str.toCharArray(message_data, 200, 0);
      if (message_str.startsWith(HEARTBEAT_ACK)) {
        Serial.println("process reponse heartbeat");
        if (config->data.work_mode == SIMPLE_CLIENT) {
          return 1;
        }
      } else if (message_str.startsWith("CALIBRATE[")) {
        // CALIBRATE[0] {23,34}
        Serial.println("process reponse calibrate");
        uint8_t idx = int(message_data[10] - '0');
        uint8_t str_length = strlen(message_data);
        message_str = message_str.substring(14, str_length - 1);

        Serial.println(idx);
        Serial.println(message_str);

        int8_t comma_idx = message_str.indexOf(',');
        if (comma_idx < 0) {
          return 0;
        }

        config->data.cal_coefficients[idx] = message_str.substring(0, comma_idx).toFloat();
        config->data.cal_offsets[idx] = message_str.substring(comma_idx+1, str_length-1).toFloat();;
        config->set_config();
        return 2;
      } else if (message_str.startsWith("CONFIG[")) {
        // CONFIG[0] {23}
        Serial.println("process reponse config");
        // Serial.println(message_str);
        uint8_t idx = int(message_data[7] - '0');
        message_str = message_str.substring(11, strlen(message_data) - 1);
        message_str.toCharArray(message_data, 200, 0);

        // Serial.println(idx);
        // Serial.println(message_data);

        int16_t val = (idx != 5) ? message_str.toInt() : 0;

        if (idx == 5) {
          int8_t cidx = message_str.indexOf(':');
          uint8_t str_length = strlen(message_data);
          message_str = message_str.substring(cidx+1, str_length);
          val = message_str.toInt();
          message_str = String(message_data);
          message_str = message_str.substring(0, cidx);
          message_str.toCharArray(message_data, 200, 0);
        }

        switch (idx) {
          case 0:
            config->data.hw_ver = val;
            break;
          case 1:
            config->data.sw_ver = val;
            break;
          case 2:
            config->data.dev_id = val;
            break;
          case 3:
            config->data.work_mode = val;
            break;
          case 4:
            config->data.heartbeat_freq = val;
            break;
          case 5:
            strcpy(config->data.server_ip, message_data);
            config->data.server_port = val;
            break;
          case 6:
            config->data.server_port = val;
            break;
        }

        config->set_config();
        return 3;
      } else if (message_str.startsWith("SYNC")) {
        return 4;
      }
      return 0;
    }
    return 0;
  }

};

#endif