#ifndef MYTABLEENTRY_H
#define MYTABLEENTRY_H

#include "../QSNMP/QSNMP.h"



/* An enumerated color */
typedef enum {
    Red = 0,
    Green = 1,
    Blue = 2
} Color;



/* MyTableEntry class definition: an example MIB table entry */
class MyTableEntry : public QSNMPModule
{

public:
                                MyTableEntry(QSNMPAgent * snmpAgent, quint32 index1, quint32 index2, Color defaultColor = Red);
    virtual                     ~MyTableEntry();

    /* SNMP module get/set variable value implementation */
    virtual QVariant            snmpGetValue(const QSNMPVar * var);
    virtual bool                snmpSetValue(const QSNMPVar * var, const QVariant & v);

private:
    /* Local parameters */
    QSNMPAgent *                mSnmpAgent;
    quint32                     mIndex1;
    quint32                     mIndex2;
    Color                       mColor;

    /* SNMP variables */
    QSNMPVar *                  mVar1;
    QSNMPVar *                  mVar2;
    QSNMPVar *                  mVar3;

};

#endif // MYTABLEENTRY_H
