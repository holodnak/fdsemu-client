CC = gcc
CXX = g++
LD = gcc
RC = windres

CFLAGS = -DMINGW -O2 -I../lib/lz4 -I../lib/hidapi/hidapi -I../lib/fdsemu -I../cli -I../lib
RCFLAGS = -DMINGW -J rc -O coff
LIBS = -lsetupapi -lgdi32 -lcomdlg32 -lcomctl32

TARGET = fdsemu-win32.exe

CPPSOURCE = fdsemu-diskrw.cpp Disk.cpp DiskSide.cpp Fdsemu.cpp
CPPSOURCE += readdiskdlg.cpp writediskdlg.cpp
CPPSOURCE += ../cli/diskrw.cpp ../cli/flashrw.cpp ../cli/diskutil.cpp ../cli/fwupdate.cpp 
CPPSOURCE += ../lib/firmware.cpp ../lib/bootloader.cpp ../lib/savedisk.cpp ../lib/crc32.cpp
CPPSOURCE += ../lib/fdsemu/Device.cpp ../lib/fdsemu/Flash.cpp ../lib/fdsemu/FlashUtil.cpp ../lib/fdsemu/Sram.cpp ../lib/fdsemu/System.cpp

CSOURCE = ../lib/lz4/lz4.c ../lib/lz4/lz4frame.c ../lib/lz4/lz4hc.c ../lib/lz4/xxhash.c
CSOURCE += ../lib/hidapi/windows/hid.c

COBJECT = $(patsubst %.c,%.o,$(CSOURCE))
CPPOBJECT = $(patsubst %.cpp,%.o,$(CPPSOURCE))

RCOBJECT = fdsemu-diskrw.res

$(TARGET): $(COBJECT) $(CPPOBJECT) $(RCOBJECT)
	$(CXX) -mwindows -Wall -s $^ $(LIBS) -o $(TARGET)

$(RCOBJECT): %.res: %.rc
	$(RC) $(RCFLAGS) -i $< -o $@

$(COBJECT): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(CPPOBJECT): %.o: %.cpp
	$(CXX) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(RCOBJECT) $(COBJECT) $(CPPOBJECT) $(TARGET)

