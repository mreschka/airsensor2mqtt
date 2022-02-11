# Global target; when 'make' is run without arguments, this is what it should do
all: airsensor2mqtt
 
# These variables hold the name of the compilation tool, the compilation flags and the link flags
# We make use of these variables in the package manifest
CC = gcc
CFLAGS = -Wall
LDFLAGS = 
 
cmdline.c: cmdline.ggo
	gengetopt --input=cmdline.ggo --include-getopt --conf-parser

# This rule builds individual object files, and depends on the corresponding C source files and the header files
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) -lmosquitto -lusb-1.0
 
airsensor2mqtt: cmdline.o a2m_mqtt.o a2m_usb.o airsensor2mqtt.o
	$(CC) -o $@ $^ $(LDFLAGS) -lmosquitto -lusb-1.0
 
# To clean build artifacts, we specify a 'clean' rule, and use PHONY to indicate that this rule never matches with a potential file in the directory
.PHONY: clean
 
clean:
	rm -f airsensor2mqtt *.o
	rm -f cmdline.c cmdline.h
