#ifndef _DHT_H_

#define _DHT_H_

#include <DHT.h>

class DHTMeter {

  private:
    DHT *dht;

  public:
    uint8_t dht_state;
    float humidity;
    float temperature;
    float hic;

  DHTMeter() {
    dht = NULL;
  }

  DHTMeter(DHT *dh) {
    dht = dh;
    humidity = 0;
    temperature = 0;
    hic = 0;
    dht_state = 1;
  };

  void read() {
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    humidity = dht->readHumidity();
    // Read temperature as Celsius (the default)
    temperature = dht->readTemperature();

    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature))
    {
      dht_state = 2;
      humidity = 200;
      temperature = 200;
    }
    else
    {
      dht_state = 3;
    }
    // Compute heat index in Celsius (isFahreheit = false)
    hic = dht->computeHeatIndex(temperature, humidity, false);
  }
};


#endif