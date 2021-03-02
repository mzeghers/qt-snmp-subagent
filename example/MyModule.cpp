#include "MyModule.h"

/* Constructs a MyModule object, it is also a SNMP object (=collection) */
MyModule::MyModule(QSNMPAgent * snmpAgent) : QSNMPObject(snmpAgent, "mymodule")
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
