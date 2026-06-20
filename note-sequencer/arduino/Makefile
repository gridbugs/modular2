MAIN_DIR=main
MAIN_SRC=$(wildcard $(MAIN_DIR)/*.c)
MAIN_OBJ=$(MAIN_SRC:.c=.o)

SCREEN_DIR=screen
SCREEN_SRC=$(wildcard $(SCREEN_DIR)/*.c)
SCREEN_OBJ=$(SCREEN_SRC:.c=.o)

COMMON_DIR=common
COMMON_SRC=$(wildcard $(COMMON_DIR)/*.c)
COMMON_OBJ=$(COMMON_SRC:.c=.o)

HEADERS=$(wildcard $(MAIN_DIR)/*.h) $(wildcard $(SCREEN_DIR)/*.h) $(wildcard $(COMMON_DIR)/*.h)

NOTE_INDICES=$(COMMON_DIR)/note_indices.h

CC=avr-gcc
STRIP=avr-strip
MCU=atmega328p

CFLAGS=-mmcu=$(MCU) -std=c99 -Wall -O1 -I $(COMMON_DIR)

all: main.elf screen.elf

main.elf: $(MAIN_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(MAIN_OBJ) $(COMMON_OBJ) -o $@
	$(STRIP) $@

screen.elf: $(SCREEN_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(SCREEN_OBJ) $(COMMON_OBJ) -o $@
	$(STRIP) $@

%.o : %.c $(HEADERS) $(NOTE_INDICES)
	$(CC) $(CFLAGS) -c -o $@ $<

$(NOTE_INDICES): note_indices_h.py
	python $< > $@

clean:
	rm -rf $(MAIN_DIR)/*.o $(SCREEN_DIR)/*.o $(COMMON_DIR)/*.o *.elf $(NOTE_INDICES)

tags: generate_tags.sh $(wildcard $(MAIN_DIR)/*.[ch]) $(wildcard $(SCREEN_DIR)/*.[ch]) $(wildcard $(COMMON_DIR)/*.[ch])
	./$<

.PHONY: clean tags all
