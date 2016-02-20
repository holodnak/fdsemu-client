CC = gcc
CXX = g++
LD = gcc

CFLAGS = -O2 -I../lib/lz4 -I../lib/hidapi/hidapi -I../lib/fdsemu -I../cli -I../lib
LIBS = -lsetupapi -lgdi32 -lcomdlg32 -lcomctl32

TARGET = fdsemu-win32

CPPSOURCE = fdsemu-diskrw.cpp 
CPPSOURCE += ../cli/diskrw.cpp ../cli/flashrw.cpp ../cli/diskutil.cpp ../cli/fwupdate.cpp 
CPPSOURCE += ../lib/firmware.cpp ../lib/bootloader.cpp ../lib/crc32.cpp
CPPSOURCE += ../lib/fdsemu/Device.cpp ../lib/fdsemu/Flash.cpp ../lib/fdsemu/FlashUtil.cpp ../lib/fdsemu/Sram.cpp ../lib/fdsemu/System.cpp

CSOURCE = ../lib/lz4/lz4.c ../lib/lz4/lz4frame.c ../lib/lz4/lz4hc.c ../lib/lz4/xxhash.c
CSOURCE += ../lib/hidapi/windows/hid.c

COBJECT = $(patsubst %.c,%.o,$(CSOURCE))
CPPOBJECT = $(patsubst %.cpp,%.o,$(CPPSOURCE))

RCOBJECT = fdsemu-diskrw.rc

$(TARGET): $(COBJECT) $(CPPOBJECT)
	$(CXX) -Wall -s $^ $(LIBS) -o $(TARGET)

$(COBJECT): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(CPPOBJECT): %.o: %.cpp
	$(CXX) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(COBJECT) $(CPPOBJECT) $(TARGET)

