#ifndef MYMODULE_H
#define MYMODULE_H

#include "../QSNMP/QSNMP.h"
#include <QDateTime>



/* MyModule class definition: an example MIB module */
class MyModule : public QObject, public QSNMPModule
{ Q_OBJECT

public:
                                MyModule(QSNMPAgent * snmpAgent);
    virtual                     ~MyModule();

    /* SNMP module get/set variable value implementation */
    virtual QVariant            snmpGet(const QSNMPVar * var);
    virtual bool                snmpSet(const QSNMPVar * var, const QVariant & v);

    /* Dummy values get/set */
    QDateTime                   startDateTime() const;
    qint32                      A() const;
    void                        setA(qint32 val);
    qint32                      B() const;
    void                        setB(qint32 val);
    qint32                      sum() const;

private:
    QSNMPAgent *                mSnmpAgent;

    /* Dummy values */
    QDateTime                   mStartDateTime;
    qint32                      mA;
    qint32                      mB;

};

#endif // MYMODULE_H
