### toolchain ###
CC = arm-none-eabi-gcc
CSIZE = arm-none-eabi-size
CDUMP = arm-none-eabi-objdump

### device and env ###
PWD = $(shell pwd)
PORT = /dev/ttyACM0

TARGET = STM32F103C6
# TARGET = STM32F103C8

# USE_ASSERT = true

### files ###
NAME = MiniRTOS
ELF = build/$(NAME).elf
MAP = build/$(NAME).map
DUMP = build/$(NAME)_dump

ifeq ($(TARGET),STM32F103C6)
	LINK = lib/stm32f10xc6.ld
	MACRO = -D USE_STDPERIPH_DRIVER -D STM32F10X_LD
	STARTUP = lib/startup_stm32f10x_ld.s
endif

ifeq ($(TARGET),STM32F103C8)
	LINK = lib/stm32f10xc8.ld
	MACRO = -D USE_STDPERIPH_DRIVER -D STM32F10X_MD
	STARTUP = lib/startup_stm32f10x_md.s
endif

ifeq ($(USE_ASSERT),true)
	MACRO += -D USE_FULL_ASSERT
endif

SRC = lib/*.c src/*.c src/**/*.c
HEAD = lib/*.h src/*.h src/**/*.h
INC = -I lib/ -I src/ $(addprefix -I , $(wildcard src/*/))

### flags ###
CFLAG = -mcpu=cortex-m3 -mthumb -std=gnu17 -Wall -Wextra -O2 -g $(MACRO) $(INC)
LFLAG = --specs=nano.specs --specs=nosys.specs -Wl,-Map=$(MAP) -Wl,--gc-sections

### script ###
.PHONY: build size clean flash flash-st flash-isp

build : $(ELF) $(DUMP) size

$(ELF) : $(SRC) $(HEAD) $(STARTUP) $(LINK) makefile
	$(CC) $(CFLAG) $(LFLAG) $(SRC) $(STARTUP) -T $(LINK) -o $@

size :
	$(CSIZE) $(ELF) -G

clean :
	rm build/*

$(DUMP) : $(ELF)
	$(CDUMP) -D $(ELF) > $(DUMP)

flash :
	openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg -c init -c halt -c "flash write_image erase $(PWD)/$(ELF)" -c reset -c shutdown

flash-st :
	openocd -f interface/stlink-v2.cfg -f target/stm32f1x.cfg -c init -c halt -c "flash write_image erase $(PWD)/$(ELF)" -c reset -c shutdown

flash-isp :
	stm32flash -w $(ELF) -v -g 0x0 $(PORT)