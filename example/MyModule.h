#ifndef MYMODULE_H
#define MYMODULE_H

#include "../QSNMP/QSNMP.h"



/* MyModule class definition: an example MIB module */
class MyModule : public QObject, public QSNMPModule
{ Q_OBJECT

public:
                                MyModule(QSNMPAgent * snmpAgent);
    virtual                     ~MyModule();

    /* SNMP module get/set variable value implementation */
    virtual QVariant            snmpGet(const QSNMPVar * var);
    virtual bool                snmpSet(const QSNMPVar * var, const QVariant & v);

private:
    QSNMPAgent *                mSnmpAgent;

};

#endif // MYMODULE_H
