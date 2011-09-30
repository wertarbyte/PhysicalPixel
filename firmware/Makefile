DEVICE  = attiny85
F_CPU   = 16500000
FUSE_L  = 0xe1
FUSE_H  = 0xdd
AVRDUDE = avrdude -p $(DEVICE)

CFLAGS  = -Iusbdrv -I. -DDEBUG_LEVEL=0
OBJECTS = usbdrv/usbdrv.o usbdrv/usbdrvasm.o usbdrv/oddebug.o rgbled.o

COMPILE = avr-gcc -Wall -Os -DF_CPU=$(F_CPU) $(CFLAGS) -mmcu=$(DEVICE)
COMPILEPP = avr-g++ -Wall -Os -DF_CPU=$(F_CPU) $(CFLAGS) -mmcu=$(DEVICE)

# symbolic targets:
help:
	@echo "This Makefile has no default rule. Use one of the following:"
	@echo "make hex ....... to build rgbled.hex"
	@echo "make program ... to flash fuses and firmware"
	@echo "make fuse ...... to flash the fuses"
	@echo "make flash ..... to flash the firmware (use this on metaboard)"
	@echo "make clean ..... to delete objects and hex file"

hex: rgbled.hex

program: flash fuse

# rule for programming fuse bits:
fuse:
	@[ "$(FUSE_H)" != "" -a "$(FUSE_L)" != "" ] || \
		{ echo "*** Edit Makefile and choose values for FUSE_L and FUSE_H!"; exit 1; }
	$(AVRDUDE) -U hfuse:w:$(FUSE_H):m -U lfuse:w:$(FUSE_L):m

# rule for uploading firmware:
flash: rgbled.hex
	$(AVRDUDE) -U flash:w:rgbled.hex:i

# rule for deleting dependent files (those which can be built by Make):
clean:
	rm -f rgbled.hex rgbled.lst rgbled.obj rgbled.cof rgbled.list rgbled.map rgbled.eep.hex rgbled.elf *.o usbdrv/*.o rgbled.s usbdrv/oddebug.s usbdrv/usbdrv.s

.cpp.o:
	$(COMPILEPP) -c $< -o $@


# Generic rule for compiling C files:
.c.o:
	$(COMPILE) -c $< -o $@

# Generic rule for assembling Assembler source files:
.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

# Generic rule for compiling C to assembler, used for debugging only.
.c.s:
	$(COMPILE) -S $< -o $@

# file targets:

# Since we don't want to ship the driver multipe times, we copy it into this project:
usbdrv:
	cp -r ../../../usbdrv .

rgbled.elf: usbdrv $(OBJECTS)	# usbdrv dependency only needed because we copy it
	$(COMPILE) -o rgbled.elf $(OBJECTS)

rgbled.hex: rgbled.elf
	rm -f rgbled.hex rgbled.eep.hex
	avr-objcopy -j .text -j .data -O ihex rgbled.elf rgbled.hex
	avr-size rgbled.hex

