QT += core gui testlib qml quick quickcontrols2
CONFIG += c++17 testcase
TARGET = arch-sign_tests
SOURCES += arch-sign_tests.cpp ../src/backend.cpp ../src/theme.cpp ../src/translator.cpp
HEADERS += ../src/backend.h ../src/theme.h ../src/translator.h
RESOURCES += ../src/resources.qrc

HEADERS += ../src/signpreview.h
SOURCES += ../src/signpreview.cpp
