QT += core gui qml quick quickcontrols2
CONFIG += c++17 release
TARGET = arch-sign
TEMPLATE = app
HEADERS += src/backend.h src/theme.h src/translator.h
SOURCES += src/main.cpp src/backend.cpp src/theme.cpp src/translator.cpp
RESOURCES += src/resources.qrc

HEADERS += src/signpreview.h
SOURCES += src/signpreview.cpp
