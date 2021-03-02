# Application
TARGET  = example
QT      += core
QT      -= gui

# NET-SNMP library linkage
LIBS += -lnetsnmp -lnetsnmpagent

# Source files
SOURCES += \
    ../QSNMP/QSNMP.cpp \
    MyModule.cpp \
    main.cpp

HEADERS += \
    ../QSNMP/QSNMP.h \
    MyModule.h
