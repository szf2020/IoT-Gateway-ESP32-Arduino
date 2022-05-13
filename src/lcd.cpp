#ifndef _LCH_H_

#define _LCH_H_

#include "config.cpp"
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
    char ui_buffer[20];
    DevConfig *dev_config;
    LiquidCrystal_I2C *lcd;
  public:
    LCD() {}
    LCD(DevConfig *dev_config) {
      this->dev_config = dev_config;
      this->lcd = new LiquidCrystal_I2C(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);
      this->lcd_init();
    };

    void lcd_init(uint8_t display_message=0) {
      vTaskDelay(10);
      this->lcd->init();
      vTaskDelay(10);
      if (display_message == 1) {
        this->lcd->noBacklight();
        vTaskDelay(10);
        this->display_message_lcd(0, "IoT Gateway", 1);
        snprintf(
          this->ui_buffer, 16, "ID:%u M:%hu V:%hu.%hu",
          this->dev_config->data.dev_id, this->dev_config->data.work_mode, this->dev_config->data.hw_ver, this->dev_config->data.sw_ver
        );
        this->display_message_lcd(1, ui_buffer);
      }
    }

    void display_message_lcd(uint8_t lineno, char *message, uint8_t clear_lcd=0) {
      uint8_t end_detected = 0;
      for (uint8_t i=0; i<16; i++) {
        if (message[i] == '\0') {
          end_detected = 1;
          break;
        }
      }
      vTaskDelay(10);
      if (clear_lcd == 1) {
        this->lcd->clear();
        vTaskDelay(10);
        this->lcd->noBacklight();
        vTaskDelay(10);
      }
      if (end_detected == 0 || lineno > 1) {
        this->lcd->setCursor(0, 0);
        vTaskDelay(10);
        this->lcd->printstr("msg invld");
      } else {
        this->lcd->setCursor(0, lineno);
        vTaskDelay(10);
        this->lcd->printstr(message);
      }
    }

    void clear_lcd() {
      this->lcd->clear();
      vTaskDelay(10);
      this->lcd->noBacklight();
      vTaskDelay(10);
    }
};

#endif