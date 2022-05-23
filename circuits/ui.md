
Push Button:
    
    3V3-----1m----button----1m----Gnd
                |
                -----> Pin 19

LED Indicator:

    Pin 18 <-----1k----LED---Gnd


RS-485:

    Pin 5  <-----RS-3485 DE,RE
    pin 16 <-----RS-3485 RO
    pin 17 <-----RS-3485 DI


LCD:
    Pin 22 <-----LCD SCL
    Pin 21 <-----LCD SDA


Meters:
                           3V3
                            |
                           10k
                            |    
    Pin33 <-----1k----|-----|------>3.5mm jack ref-v
                      |     |
                    zener  10k
                      |     |
                      -----Gnd

3.5 mm jack:

    Current Sensor:

        Sleeve | Ring | Ring | Tip
          |       |      |      |
         1k      3V3    ref-v  NC
          |       |      |
          |       |-10k--|--10k------>Gnd
          |
          |--zener---Gnd
          |
          uC

GSM:

SD Card:
