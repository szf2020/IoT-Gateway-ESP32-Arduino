#ifndef _HTTP_HANDLER_H_

#define _HTTP_HANDLER_H_
#include "config.cpp"
#ifdef GSM_ENABLED
#include <ArduinoHttpClient.h>

#define CONTENT_TYPE "application/json"

class HTTPHandler {
  private:
  char buffer[500];
  String resp_string;
  HttpClient *http_client;

  public:
  HTTPHandler() {}
  void init(HttpClient *http_client) {
    this->http_client = http_client;
  }

  int send_heartbeat(const char* path, uint16_t dev_id, char *dev_mac) {
    Serial.print("Sending http post:");
    snprintf(
      this->buffer,
      500,
      "{\"device\":%u,\"mac\":\"%s\"}",
      dev_id,
      dev_mac
    );
    Serial.println(this->buffer);
    int err = this->http_client->post(path, CONTENT_TYPE, this->buffer);
    Serial.print("Resp:");
    Serial.println(err);
    if (err != 0) {
      return err;
    }

    err = this->http_client->responseStatusCode();
    Serial.print("resp: ");
    Serial.print(err);
    if (err == 200 || err == 201) {
      this->resp_string = this->http_client->responseBody();
      Serial.print(" ");
      Serial.println(this->resp_string);
    }

    this->http_client->stop();
    return err;
  }

  int send_data(const char* path, char *data) {
    Serial.print("Sending data post:");
    Serial.println(path);
    Serial.println(data);
    int err = this->http_client->post(path, CONTENT_TYPE, data);
    Serial.print("Resp:");
    Serial.println(err);
    if (err != 0) {
      return err;
    }

    err = this->http_client->responseStatusCode();
    Serial.print("resp: ");
    Serial.print(err);
    if (err == 200 || err == 201) {
      this->resp_string = this->http_client->responseBody();
      Serial.print(" ");
      Serial.println(this->resp_string);
    }

    this->http_client->stop();
    return err;
  }

  String get_response() {
    return this->resp_string;
  }

  void poll() {}
};
#endif
#endif