TARGET=main

SRC_DIR=src
HEADERS=$(wildcard $(SRC_DIR)/*.h)
SRC=$(wildcard $(SRC_DIR)/*.c)
OBJ=$(SRC:.c=.o)

CC=avr-gcc
STRIP=avr-strip
MCU=atmega328p

CFLAGS=-mmcu=$(MCU) -std=c99 -Wall -O1

$(TARGET).elf: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@
	$(STRIP) $@

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(SRC_DIR)/*.o *.elf

tags: generate_tags.sh $(wildcard $(SRC_DIR)/*.[ch])
	./$<

.PHONY: clean tags
