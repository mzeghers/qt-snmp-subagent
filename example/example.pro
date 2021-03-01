# Application
TARGET  = example
QT      += core
QT      -= gui

# NET-SNMP library linkage
LIBS += -lnetsnmp -lnetsnmpagent

# Source files
SOURCES += \
    ../QSNMP/QSNMP.cpp \
    example.cpp

HEADERS += \
    ../QSNMP/QSNMP.h
