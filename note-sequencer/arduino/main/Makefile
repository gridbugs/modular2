TARGET=main

SRC_DIR=src
SRC=$(wildcard $(SRC_DIR)/*.c)
OBJ=$(SRC:.c=.o)

NOTES=$(SRC_DIR)/notes.h

CC=avr-gcc
STRIP=avr-strip
MCU=atmega328p

CFLAGS=-mmcu=$(MCU) -std=c99 -Wall -O1

$(TARGET).elf: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@
	$(STRIP) $@

%.o : %.c $(HEADERS) $(NOTES)
	$(CC) $(CFLAGS) -c -o $@ $<

$(SRC_DIR)/notes.h: notes_h.py
	python $< > $@

clean:
	rm -rf $(SRC_DIR)/*.o *.elf $(NOTES)

tags: generate_tags.sh $(wildcard $(SRC_DIR)/*.[ch])
	./$<

.PHONY: clean tags
