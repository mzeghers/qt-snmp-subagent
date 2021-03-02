#include "MyModule.h"

/* Constructs a MyModule object, it implements a QSNMP Module */
MyModule::MyModule(QSNMPAgent * snmpAgent) : QSNMPModule(snmpAgent, "mymodule")
{
    /* Store SNMP agent */
    mSnmpAgent = snmpAgent;
}

MyModule::~MyModule()
{
    /* Nothing to do */
}

QVariant MyModule::snmpGet(const QSNMPVar * var)
{
    /* TODO */
    return QVariant();
}

bool MyModule::snmpSet(const QSNMPVar * var, const QVariant & v)
{
    /* TODO */
    return false;
}
