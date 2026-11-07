QT += core gui quick quickcontrols2 network testlib
CONFIG += c++17 testcase
TARGET = rewind_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/archive.h ../src/backend.h ../src/download.h ../src/nts.h ../src/queue.h \
           ../src/theme.h ../src/translator.h
SOURCES += rewind_tests.cpp ../src/archive.cpp ../src/backend.cpp ../src/download.cpp ../src/nts.cpp \
           ../src/queue.cpp ../src/theme.cpp ../src/translator.cpp
