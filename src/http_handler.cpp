#ifndef _HTTP_HANDLER_H_

#define _HTTP_HANDLER_H_
#include "config.cpp"
#ifdef GSM_ENABLED
#include <ArduinoHttpClient.h>

class HTTPHandler {
  private:
  HttpClient *http_client;

  public:
  HTTPHandler() {}
  void init(HttpClient *http_client) {
    this->http_client = http_client;
  }

  int send_heartbeat(const char* path, const char *data) {
    Serial.print("Sending http get:");
    Serial.println(path);
    int err = this->http_client->post(path, "application/json", data);
    if (err != 0) {
      return err;
    }
    return err;
  }

  void poll() {}
};
#endif
#endif