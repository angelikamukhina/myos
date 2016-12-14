CC ?= gcc
LD ?= ld

CFLAGS := -g -m64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -ffreestanding \
	-mcmodel=kernel -Wall -Wextra -Werror -pedantic -std=c11 \
	-Wframe-larger-than=1024 -Wstack-usage=1024 \
	-Wno-unknown-warning-option $(if $(DEBUG),-DDEBUG)
LFLAGS := -nostdlib -z max-page-size=0x1000

INC := ./inc
SRC := ./src

C_SOURCES := $(wildcard $(SRC)/*.c)
C_OBJECTS := $(C_SOURCES:.c=.o)
C_DEPS := $(C_SOURCES:.c=.d)
S_SOURCES := $(filter-out $(SRC)/entry.S, $(wildcard $(SRC)/*.S)) $(SRC)/entry.S
S_OBJECTS := $(S_SOURCES:.S=.o)
S_DEPS := $(S_SOURCES:.S=.d)

OBJ := $(C_OBJECTS) $(S_OBJECTS)
DEP := $(C_DEPS) $(S_DEPS)

all: kernel

kernel: $(OBJ) kernel.ld
	$(LD) $(LFLAGS) -T kernel.ld -o $@ $(OBJ)

$(S_OBJECTS): %.o: %.S
	$(CC) -D__ASM_FILE__ -I$(INC) -g -MMD -c $< -o $@

$(C_OBJECTS): %.o: %.c
	$(CC) $(CFLAGS) -I$(INC) -g -MMD -c $< -o $@

$(SRC)/entry.S: gen_entries.py
	python3 gen_entries.py > $@

-include $(DEP)

.PHONY: clean
clean:
	rm -f kernel $(SRC)/entry.S $(OBJ) $(DEP)
