#ifndef _METERS_H_

#define _METERS_H_

#include "config.cpp"

// ADC
#define ADC_AVG_COUNT_MAX 100

const uint8_t ADC_PINS[6] = {
  METER_1_PIN,
  METER_2_PIN,
  METER_3_PIN,
  METER_4_PIN,
  METER_5_PIN,
  METER_6_PIN
};

class Meter {

  private:
    DevConfig *config;
    uint16_t adc_avg_count_voltage;
    uint16_t adc_avg_count_current;
    unsigned long adc_sum_values[6];

  public:
    uint16_t adc_rms_values[6];
    float meter_values[6];

    Meter() {
    };

    Meter(DevConfig *cfg)
    {
      config = cfg;
      adc_avg_count_voltage = 0;
      adc_avg_count_current = 0;

      for (uint8_t i=0; i < 6; i += 1) {
        pinMode(ADC_PINS[i], INPUT);
        adc_sum_values[i] = 0;
        adc_rms_values[i] = 0;
        meter_values[i] = 0;
        digitalWrite(ADC_PINS[i], LOW);
      }
    };

    void read_voltage() {
      for (uint8_t i = 0; i < 3; i++) {
        uint16_t sensor_val = analogRead(ADC_PINS[i]);
        adc_sum_values[i] = adc_sum_values[i] +  sensor_val * sensor_val;
      }
      adc_avg_count_voltage += 1;

      if (adc_avg_count_voltage > ADC_AVG_COUNT_MAX)
      {
        for (uint8_t i = 0; i < 3; i++)
        {
          adc_rms_values[i] = sqrt(adc_sum_values[i] / ADC_AVG_COUNT_MAX);
          meter_values[i] = config->data.cal_coefficients[i] * adc_rms_values[i] + config->data.cal_offsets[i];
          adc_sum_values[i] = 0;
        }
        adc_avg_count_voltage = 0;
      }
    }

     void read_current() {
      for (uint8_t i = 3; i < 6; i++) {
        uint16_t sample_val1 = analogRead(ADC_PINS[i]);
        vTaskDelay(10);
        uint16_t sample_val2 = analogRead(ADC_PINS[i]);
        int16_t diff = (sample_val1 - sample_val2);
        adc_sum_values[i] = adc_sum_values[i] + (diff * diff);
      }
      adc_avg_count_current += 1;

      if (adc_avg_count_current > ADC_AVG_COUNT_MAX)
      {
        for (uint8_t i = 3; i < 6; i++)
        {
          // adc_rms_values[i] = adc_rms_values[i] + 0.1 * (sqrt(adc_sum_values[i] / ADC_AVG_COUNT_MAX) - adc_rms_values[i]);
          adc_rms_values[i] = sqrt(adc_sum_values[i] / ADC_AVG_COUNT_MAX);
          meter_values[i] = config->data.cal_coefficients[i] * adc_rms_values[i] + config->data.cal_offsets[i];
          adc_sum_values[i] = 0;
        }
        adc_avg_count_current = 0;

      }
    }

};

#endif