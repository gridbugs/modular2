# Arduino Delay

Delay using an Arduino and a MCP4725 DAC without using the Arduino SDK.

## Useful Commands

### Flash an Arduino Nano
```
make && avrdude -P /dev/ttyUSB0 -c arduino -p m328p -U flash:w:main.elf
```

### Connect USB serial console
```
picocom -b300 /dev/ttyUSB0
```
