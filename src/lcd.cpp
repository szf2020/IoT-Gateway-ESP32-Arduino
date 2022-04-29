#ifndef _LCH_H_

#define _LCH_H_

#include "config.cpp"

#ifdef LCD_ENABLED
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD stuff
#define LCD_SCL_PIN 22
#define LCD_SDA_PIN 21

#define LCD_I2C_ADDRESS 0x27
#define LCD_ROWS 2
#define LCD_COLUMNS 16


class LCD {
  private:
    uint8_t init = 0;
    LiquidCrystal_I2C *lcd;
  public:
    LCD() {}
    LCD(LiquidCrystal_I2C *lcd) {
      this->lcd = lcd;
      this->lcd->init();
      this->lcd->noBacklight();
      this->lcd->clear();
      this->init = 1;
      this->lcd->setCursor(0, 0);
      this->lcd->print("IoT Gateway");
    };

    void display_message(uint8_t lineno, char *message) {
      uint8_t end_detected = 0;
      if (this->init == 0) {
        return;
      }
      for (uint8_t i=0; i<16; i++) {
        if (message[i] == '\0') {
          end_detected = 1;
          break;
        }
      }
      if (end_detected == 0 || lineno > 1) {
        this->lcd->setCursor(0, 0);
        this->lcd->printstr("msg invld");
      } else {
        this->lcd->setCursor(0, lineno);
        this->lcd->printstr(message);
      }
    }

    void clear() {
      if (this->init == 0) {
        return;
      }
      this->lcd->clear();
    }
};

#endif
#endif
