# Main Makefile for BACnet-stack project with GCC

PWD = $(shell pwd)
HEADER_DIR = $(PWD)/include
LIB_DIR = $(PWD)/lib

# Default compiler settings
CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)g++
AR = $(CROSS_COMPILE)ar
CPP = $(CROSS_COMPILE)g++
READELF = $(CROSS_COMPILE)readelf

CFLAGS ?=
CXXFLAGS ?=
CPPFLAGS ?=

CFLAGS =-std=gnu99 -Wmissing-prototypes
CXXFLAGS =-std=gnu++98 
# CPPFLAGS += -fdata-sections -ffunction-sections -Wall -Wmissing-prototypes -W -Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings -fshort-enums -fno-common -fno-strict-aliasing
CPPFLAGS += -fdata-sections -ffunction-sections -Wall
INCLUDES = -I$(HEADER_DIR)

LDFLAGS ?=
LDFLAGS += -lrt -lpthread -Wl,--gc-sections -lusb-1.0

ifeq ($(DEBUG),y)
	CPPFLAGS += -g -DDEBUG
else
	CPPFLAGS += -Os
	LDFLAGS += -Wl,-s
	MAKE += -s
endif

# Export the variables defined here to all subprocesses
.EXPORT_ALL_VARIABLES:

all: lib demo
.PHONY : all lib clean demo

lib:
	$(MAKE) -C src all

demo: lib
	$(MAKE) -C demo all

clean:
	-$(MAKE) -C src clean
	-$(MAKE) -C demo clean

demo-clean:
	$(MAKE) -C demo clean
