# Arduino Dual DAC Experiment

Controlling a pair of MCP4725 DACs from an Arduino without using the Arduino SDK.

## Useful Commands

### Flash an Arduino Nano
```
make && avrdude -P /dev/ttyUSB0 -c arduino -p m328p -U flash:w:main.elf
```

### Connect USB serial console
```
picocom -b300 /dev/ttyUSB0
```
