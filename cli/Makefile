CC = gcc
CXX = g++
LD = gcc

UNAME = $(shell uname)

CFLAGS = -I../lib/lz4 -I../lib/hidapi/hidapi -I../lib/fdsemu -I../lib
LIBS = -lsetupapi

TARGET = fdsemu-cli

CPPSOURCE = main.cpp ../lib/firmware.cpp ../lib/crc32.cpp
CPPSOURCE += ../lib/fdsemu/Device.cpp ../lib/fdsemu/Flash.cpp ../lib/fdsemu/FlashUtil.cpp ../lib/fdsemu/Sram.cpp ../lib/fdsemu/System.cpp

CSOURCE += ../lib/lz4/lz4.c ../lib/lz4/lz4frame.c ../lib/lz4/lz4hc.c ../lib/lz4/xxhash.c
HIDSRC = ../lib/hidapi/windows/hid.c

ifeq ($(UNAME),Linux)
	HIDSRC = ../lib/hidapi/linux/hid.c
	LIBS = `pkg-config libusb-1.0 --libs` -lpthread -ludev
endif

ifeq ($(UNAME),Darwin)
	HIDSRC = ../lib/hidapi/mac/hid.c
	LIBS = -framework IOKit -framework CoreFoundation -liconv
endif

CSOURCE += $(HIDSRC)

COBJECT = $(patsubst %.c,%.o,$(CSOURCE))
CPPOBJECT = $(patsubst %.cpp,%.o,$(CPPSOURCE))

$(TARGET): $(COBJECT) $(CPPOBJECT)
	$(CXX) -Wall -s $^ $(LIBS) -o $(TARGET)

$(COBJECT): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(CPPOBJECT): %.o: %.cpp
	$(CXX) -c $(CFLAGS) $< -o $@

clean:
	rm $(COBJECT) $(CPPOBJECT) $(TARGET)

