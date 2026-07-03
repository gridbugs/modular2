MAIN_DIR=main
MAIN_SRC=$(wildcard $(MAIN_DIR)/*.c)
MAIN_MAIN_OBJ=$(MAIN_SRC:.c=.o)
MAIN_COMMON=command state util twi uart timer note rotary_encoder mcp4725 key_matrix
MAIN_OBJS=$(MAIN_MAIN_OBJ) $(foreach f,$(MAIN_COMMON),common/$(f).o)

SCREEN_DIR=screen
SCREEN_SRC=$(wildcard $(SCREEN_DIR)/*.c)
SCREEN_MAIN_OBJ=$(SCREEN_SRC:.c=.o)
SCREEN_COMMON=command spi display font st7735 state util twi uart timer note
SCREEN_OBJS=$(SCREEN_MAIN_OBJ) $(foreach f,$(SCREEN_COMMON),common/$(f).o)

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

main.elf: $(MAIN_OBJS)
	$(CC) $(CFLAGS) $^ -o $@
	$(STRIP) $@

screen.elf: $(SCREEN_OBJS)
	$(CC) $(CFLAGS) $^ -o $@
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
