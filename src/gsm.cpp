#ifndef _GSM_H_

#define _GSM_H_
#include "config.cpp"
#include <TinyGsmClient.h>

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

enum GSMState
{
  GSMInIt = 1,
  SIMReady = 2,
  NetworkRegistered = 3,
  GPRSConnected = 4
};

class GSM {
  private:
    DevConfig *config;
    TinyGsm *modem;
    TinyGsmClient *client;
    uint16_t buffer_index;
  public:
    uint8_t state;
    GSM() {}
    void init(DevConfig *cfg, TinyGsm *modem, TinyGsmClient *client) {
      this->state = GSMState::GSMInIt;
      this->config = cfg;
      Serial2.begin(GSM_BAUDRATE, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);

      this->modem = modem;
      this->client = client;
      pinMode(GSM_POWERKEY_PIN, OUTPUT);
      pinMode(GSM_RST_PIN, OUTPUT);
      pinMode(GSM_POWER_PIN, OUTPUT);
      digitalWrite(GSM_POWERKEY_PIN, LOW);
      digitalWrite(GSM_RST_PIN, HIGH);
      digitalWrite(GSM_POWER_PIN, HIGH);

      this->modem->restart();
    }

    bool check_gprs() {

      Serial.print("GSM state: ");
      Serial.println(this->state);
      if (this->state == GSMState::GSMInIt) {
        if (this->modem->getSimStatus() != SIM_READY) {
          Serial.println("Waiting for SIM");
          return false;
        }
        this->state = GSMState::SIMReady;
      }

      if (this->state == GSMState::SIMReady) {
        if (!this->modem->isNetworkConnected()) {
          Serial.println("Waiting for network");
          return false;
        }
        this->state = GSMState::NetworkRegistered;
      }

      if (this->state == GSMState::NetworkRegistered) {
        if (!this->modem->gprsConnect(GSM_APN, GSM_USER, GSM_PASS)) {
          Serial.println("gprs connection fail"); 
          this->state = GSMState::GSMInIt;
          return false;
        }
        this->state = GSMState::GPRSConnected;
      }

      if (this->state == GSMState::GPRSConnected) {
        if (!this->modem->isGprsConnected()) {
          this->state = GSMState::GSMInIt;
          return false;
        }
      }

      return true;
    }
};

#endif