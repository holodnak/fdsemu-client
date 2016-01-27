#-------------------------------------------------
#
# Project created by QtCreator 2015-11-25T10:12:10
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = fdsemu-qt
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    writestatus.cpp \
    diskreaddialog.cpp \
    writefilesdialog.cpp \
    dumpflashdialog.cpp \
    ../lib/fdsemu/Device.cpp \
    ../lib/fdsemu/Flash.cpp \
    ../lib/fdsemu/FlashUtil.cpp \
    ../lib/fdsemu/Sram.cpp \
    ../lib/fdsemu/System.cpp \
    ../lib/lz4/lz4.c \
    ../lib/lz4/lz4frame.c \
    ../lib/lz4/lz4hc.c \
    ../lib/lz4/xxhash.c \
	 ../lib/firmware.cpp \
	 ../lib/crc32.cpp

HEADERS  += mainwindow.h \
    hidapi/hidapi.h \
    writestatus.h \
    diskreaddialog.h \
    writefilesdialog.h \
    dumpflashdialog.h \
    ../lib/fdsemu/Device.h \
    ../lib/fdsemu/Flash.h \
    ../lib/fdsemu/FlashUtil.h \
    ../lib/fdsemu/Sram.h \
    ../lib/fdsemu/System.h \
    ../lib/lz4/lz4.h \
    ../lib/lz4/lz4frame.h \
    ../lib/lz4/lz4frame_static.h \
    ../lib/lz4/lz4hc.h \
    ../lib/lz4/xxhash.h \
	 ../lib/hidapi/hidapi/hidapi.h \
	 ../lib/crc32.h

FORMS    += mainwindow.ui \
    writestatus.ui \
    diskreaddialog.ui \
    writefilesdialog.ui \
    dumpflashdialog.ui

INCLUDEPATH += ../lib \
	../lib/hidapi/hidapi \
	../lib/fdsemu \
	../lib/lz4

LIBS += -llz4

win32 {
	LIBS += -lsetupapi
	SOURCES += ../lib/hidapi/windows/hid.c
}
macx {
	LIBS += -framework IOKit -framework CoreFoundation -liconv
	SOURCES += ../lib/hidapi/mac/hid.c
}

