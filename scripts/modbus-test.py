#!/usr/bin/env python3
import serial
import minimalmodbus

instrument = minimalmodbus.Instrument('/dev/tty.usbserial-120', 1)  # port name, slave address (in decimal)

instrument.serial.baudrate = 9600
instrument.serial.bytesize = 8
instrument.serial.parity   = serial.PARITY_NONE
instrument.serial.stopbits = 2
instrument.serial.timeout  = 0.5          # seconds

## Read temperature (PV = ProcessValue) ##
temperature = instrument.read_register(0, 2)  # Registernumber, number of decimals
print(temperature)

## Change temperature setpoint (SP) ##
# NEW_TEMPERATURE = 95
# instrument.write_register(24, NEW_TEMPERATURE, 1)  # Registernumber, value, number of decimals for storage
