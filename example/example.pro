# Application
TARGET  = example
QT      += core
QT      -= gui

# NET-SNMP library linkage
LIBS += -lnetsnmp -lnetsnmpagent

# Source files
SOURCES += \
    example.cpp
