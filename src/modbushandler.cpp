#ifndef _MODBUS_H_

#define _MODBUS_H_
#include "config.cpp"
#include <ModbusMaster.h>

#define MODBUS_HANDLER_BUFFER_SIZE 1000

class ModbusHandler {

  private:
    DevConfig *config;
    ModbusMaster *node;
    char data_buffer[MODBUS_HANDLER_BUFFER_SIZE];
    uint16_t buffer_index;
  public:
    ModbusHandler() {
      this->node = new ModbusMaster();
      this->buffer_index = 0;
    }
    static void preTransmission()
    {
      digitalWrite(MODBUS_RTS_PIN, 1);
    }

    static void postTransmission()
    {
      digitalWrite(MODBUS_RTS_PIN, 0);
    }

    void init(DevConfig *cfg) {
      pinMode(MODBUS_RTS_PIN, OUTPUT);
      digitalWrite(MODBUS_RTS_PIN, 0);
      this->config = cfg;
      Serial2.begin(MODBUS_BAUDRATE, SERIAL_8N2, MODBUS_RX_PIN, MODBUS_TX_PIN, false, 2000);

      // Modbus slave ID 1
      this->node->begin(1, Serial2);
      // Callbacks allow us to configure the RS485 transceiver correctly
      this->node->preTransmission(preTransmission);
      this->node->postTransmission(postTransmission);
    }

    char* update() {
      uint8_t result;
      uint8_t is_first_address = 1;
      bool valid_func = 0;
      this->buffer_index = 0;
      this->buffer_index += sprintf(this->data_buffer, "{\"modbus\":{");
      if (this->config->data.modbus_enabled == 1) {
        for (uint8_t i=0; i < MAX_MODBUS_SLAVES; i++) {
          if (this->config->data.modbus_slave_ids[i] != 0) {
            this->node->begin(this->config->data.modbus_slave_ids[i], Serial2);
            for (uint8_t k = 0; k < MAX_ADDRESSES_PER_MODBUS_SLAVE; k++) {
              result = this->node->ku8MBResponseTimedOut;
              valid_func = 0;
              if (this->config->data.modbus_address[i][k].func_code == 0x01) {
                valid_func = 1;
                result = this->node->readCoils(
                  this->config->data.modbus_address[i][k].address,
                  this->config->data.modbus_address[i][k].length
                );
              } else if (this->config->data.modbus_address[i][k].func_code == 0x03) {
                valid_func = 1;
                result = this->node->readHoldingRegisters(
                  this->config->data.modbus_address[i][k].address,
                  this->config->data.modbus_address[i][k].length
                );
              } else if (this->config->data.modbus_address[i][k].func_code == 0x04) {
                valid_func = 1;
                result = this->node->readInputRegisters(
                  this->config->data.modbus_address[i][k].address,
                  this->config->data.modbus_address[i][k].length
                );
              }

              if (valid_func == 1) {
                // Serial.printf(
                //   "Modbus cfg: %hu:%hu:%u:%hu:%hu:",
                //   this->config->data.modbus_slave_ids[i],
                //   this->config->data.modbus_address[i][k].func_code,
                //   this->config->data.modbus_address[i][k].address,
                //   this->config->data.modbus_address[i][k].length,
                //   result
                // );
                if (is_first_address == 1) {
                  is_first_address = 0;
                } else {
                  this->buffer_index += sprintf(&this->data_buffer[this->buffer_index], ",");
                }
                this->buffer_index += sprintf(
                  &this->data_buffer[this->buffer_index],
                  "\"%hu:%hu:%u:%hu\":\"%hu",
                  this->config->data.modbus_slave_ids[i],
                  this->config->data.modbus_address[i][k].func_code,
                  this->config->data.modbus_address[i][k].address,
                  this->config->data.modbus_address[i][k].length,
                  result
                );
                if (result == this->node->ku8MBSuccess)
                {
                  for(uint8_t j = 0; j < this->config->data.modbus_address[i][k].length; j++) {
                    this->buffer_index += sprintf(
                      &this->data_buffer[this->buffer_index],
                      ",%d",
                      this->node->getResponseBuffer(j)
                    );
                  }
                }
                this->buffer_index += sprintf(&this->data_buffer[this->buffer_index], "\"");
              }
            }
          }
        }
      }
      this->buffer_index += sprintf(&this->data_buffer[this->buffer_index], "}}");
      return this->data_buffer;
    }
};

#endif
