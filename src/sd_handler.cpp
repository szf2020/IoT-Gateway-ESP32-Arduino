#ifndef _SD_HANDLER_H_
#define _SD_HANDLER_H_

#include "config.cpp"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

class SDHandler {
  private:
  uint32_t current_file_timestamp;
  char current_file[100];
  fs::SDFS *sd;
  fs::FS *fs;

  public:
  SDHandler() {}

  void init(fs::SDFS *sd, fs::FS *fs) {
    this->sd = sd;
    this->fs = fs;
    this->start_new_file();
    this->current_file_timestamp = DEFAULT_TIMESTAMP;
    sprintf(this->current_file, "%u.txt", this->current_file_timestamp);
  }

  void start_new_file() {
    sprintf(current_file, "test-file.txt");
  }

  char* get_card_info() {
    uint8_t cardType = this->sd->cardType();
    if(cardType == CARD_NONE){
      return "None";
    }
    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC){
      return "MMC";
    } else if(cardType == CARD_SD){
      return "SDSC";
    } else if(cardType == CARD_SDHC){
      return "SDHC";
    } else {
      return "UNKNOWN";
    }
  }

  void add_data(uint32_t timestamp, const char * message){
    int diff = timestamp - current_file_timestamp;
    if (diff > SD_FILE_CHANGE_SECONDS) {
      current_file_timestamp = timestamp;
      sprintf(this->current_file, "%u.txt", this->current_file_timestamp);
    }
    Serial.printf("Appending to file: %s\n", this->current_file);
    File file = this->fs->open(this->current_file, FILE_APPEND);
    if(!file){
      Serial.println("Failed to open file for appending");
      return;
    }
    if(file.print(message)){
      Serial.println("Message appended");
    } else {
      Serial.println("Append failed");
    }
    file.close();
  }
};

#endif
