#ifndef _METERS_H_

#define _METERS_H_

#include "config.cpp"

// ADC
#define ADC_AVG_COUNT_MAX 100
#define ADC_HALF_VAL 4096/2

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
    uint32_t adc_avg_count_voltage;
    uint32_t adc_avg_count_current;
    unsigned long adc_sum_values[6];

  public:
    uint32_t adc_rms_values[6];
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

    void read() {
      for (uint8_t i = 0; i < 6; i++) {
        int32_t sensor_val = analogRead(ADC_PINS[i]) - ADC_HALF_VAL;
        adc_sum_values[i] = adc_sum_values[i] +  sensor_val * sensor_val;
      }
      adc_avg_count_voltage += 1;

      if (adc_avg_count_voltage >= ADC_AVG_COUNT_MAX-1)
      {
        for (uint8_t i = 0; i < 6; i++)
        {
          adc_rms_values[i] = sqrt(adc_sum_values[i] / ADC_AVG_COUNT_MAX);
          meter_values[i] = config->data.cal_coefficients[i] * adc_rms_values[i] + config->data.cal_offsets[i];
          adc_sum_values[i] = 0;
        }
        adc_avg_count_voltage = 0;
      }
    }
};

#endif