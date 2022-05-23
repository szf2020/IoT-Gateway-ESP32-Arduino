#ifndef _SERVER_SYNC_H_

#define _SERVER_SYNC_H_

#include "config.cpp"

#define HEARTBEAT_CONTENT "HEARTBEAT"
#define HEARTBEAT_ACK "HEARTBEAT_ACK"
#define MAX_MESSAGE_LENGTH 200


enum ServerConnectionState
{
  Disconnected = 1,
  Connected = 2
};

class ServerSync
{

private:
  DevConfig *config;
  uint16_t seconds_since_last_sent;

public:
  uint8_t state;
  char shared_buffer[1000];

  ServerSync() {
    config = NULL;
  }

  ServerSync(DevConfig *cfg)
  {
    state = ServerConnectionState::Disconnected;
    config = cfg;
    seconds_since_last_sent = 9999;
  };

  char* get_heartbeat(char *device_mac)
  {
    sprintf(
      shared_buffer,
      HEARTBEAT_CONTENT
      " [%u] [%s]",
      config->data.dev_id,
      device_mac
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

  uint8_t process_response(String message_str) {
    char message_data[200];
    String message_str_temp;
    uint8_t str_length, idx;
    message_str.toCharArray(message_data, 200, 0);
    if (message_str.startsWith(HEARTBEAT_ACK)) {
      Serial.println("process response heartbeat");
      str_length = strlen(message_data);
      message_str = message_str.substring(14, str_length - 1);
      message_str.toCharArray(message_data, 200, 0);
      sscanf(message_data, "[%u]", &config->data.timestamp);
      config->set_timestamp();
      if (config->data.work_mode == SIMPLE_CLIENT) {
        return 1;
      }
    } else if (message_str.startsWith("CALIBRATE[")) {
      // CALIBRATE[0] {23,34}
      Serial.println("process response calibrate");
      idx = int(message_data[10] - '0');
      str_length = strlen(message_data);
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
      Serial.println("process response config");
      // Serial.println(message_str);
      idx = int(message_data[7] - '0');
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
        // Enable/disable modbus
        case 7:
          config->data.modbus_enabled = val;
          break;
      }

      config->set_config();
      return 3;
    } else if (message_str.startsWith("MODBUS[")) {
      // MODBUS[slave_idx,slave_id] [{function_code,address,length},{function_code,address,length}...]
      Serial.println("process response modbus config");
      uint8_t idx = int(message_data[7] - '0');
      uint8_t slave_id = int(message_data[9] - '0');
      uint8_t str_length = strlen(message_data);
      message_str = message_str.substring(13, str_length - 1);
      // this leaves message_str with {function_code,address,length},{function_code,address,length}...
      Serial.println(idx);
      Serial.println(slave_id);
      Serial.println(message_str);

      if (idx >=0 && idx < MAX_MODBUS_SLAVES) {
        uint8_t message_addr_idx = 0;
        str_length = message_str.length();
        config->data.modbus_slave_ids[idx] = slave_id;
        while (str_length > 0 && message_addr_idx < MAX_ADDRESSES_PER_MODBUS_SLAVE) {
          int8_t end_idx = message_str.indexOf('}');
          if (end_idx < 0) {
            break;
          }
          message_str_temp = message_str.substring(0, end_idx+1);
          // Serial.print("processing str: ");
          // Serial.println(message_str_temp);
          // Serial.println(message_str);
          message_str_temp.toCharArray(message_data, 200, 0);
          sscanf(
            message_data,
            "{%hu,%u,%hu}",
            &(config->data.modbus_address[idx][message_addr_idx].func_code),
            &(config->data.modbus_address[idx][message_addr_idx].address),
            &(config->data.modbus_address[idx][message_addr_idx].length)
          );
          message_addr_idx += 1;
          message_str = message_str.substring(end_idx, str_length);
          str_length = message_str.length();
          end_idx = message_str.indexOf('{');
          if (end_idx < 0) {
            break;
          }
          message_str = message_str.substring(end_idx, str_length);
          str_length = message_str.length();
        }
        config->set_config();
        return 5;
      }
      return 0;
    } else if (message_str.startsWith("SYNC")) {
      return 4;
    }
    return 0;
  }

};

#endif