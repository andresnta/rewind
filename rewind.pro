QT += core gui qml quick quickcontrols2 network

CONFIG += c++17 release
TARGET = rewind
TEMPLATE = app

HEADERS += \
    src/archive.h \
    src/backend.h \
    src/cli.h \
    src/download.h \
    src/nts.h \
    src/queue.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/archive.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/download.cpp \
    src/nts.cpp \
    src/queue.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
