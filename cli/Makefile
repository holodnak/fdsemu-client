CC = gcc
CXX = g++
LD = gcc

CFLAGS = -I../lib/lz4 -I../lib/hidapi -I../lib/fdsemu
LIBS = -lsetupapi

TARGET = fdsemu-cli

CPPSOURCE = main.cpp firmware.cpp crc32.cpp
CPPSOURCE += ../lib/fdsemu/Device.cpp ../lib/fdsemu/Flash.cpp ../lib/fdsemu/FlashUtil.cpp ../lib/fdsemu/Sram.cpp ../lib/fdsemu/System.cpp

CSOURCE += ../lib/lz4/lz4.c ../lib/lz4/lz4frame.c ../lib/lz4/lz4hc.c ../lib/lz4/xxhash.c
CSOURCE += ../lib/hidapi/windows/hid.c

COBJECT = $(patsubst %.c,%.o,$(CSOURCE))
CPPOBJECT = $(patsubst %.cpp,%.o,$(CPPSOURCE))

$(TARGET): $(COBJECT) $(CPPOBJECT)
	$(CXX) -Wall -s $^ $(LIBS) -o $(TARGET)

$(COBJECT): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(CPPOBJECT): %.o: %.cpp
	$(CXX) -c $(CFLAGS) $< -o $@
