#include "MyModule.h"

/* Constructs a MyModule object, it implements a QSNMP Module. */
MyModule::MyModule(QSNMPAgent * snmpAgent) : QSNMPModule(snmpAgent)
{
    /* Store local parameters */
    mSnmpAgent = snmpAgent;

    /* MyModule group OID */
    mMyModuleOid = QSNMPOid() << 1 << 3 << 6 << 1 << 4 << 1 /* iso.org.dod.internet.private.enterprises */
                                                        << 12345 /* .example */
                                                        << 1; /* .myModule */
    /* Initialize dummy values */
    mStartDateTime = QDateTime::currentDateTime();
    mA = 142;
    mB = -100;

    /* Create SNMP variables */
    this->snmpCreateVar("moduleUptime", QSNMPType_TimeTicks, QSNMPMaxAccess_ReadOnly, mMyModuleOid, 1);
    this->snmpCreateVar("moduleValueA", QSNMPType_Integer, QSNMPMaxAccess_ReadWrite, mMyModuleOid, 2);
    this->snmpCreateVar("moduleValueB", QSNMPType_Integer, QSNMPMaxAccess_ReadWrite, mMyModuleOid, 3);
    this->snmpCreateVar("moduleValueSum", QSNMPType_Integer, QSNMPMaxAccess_ReadOnly, mMyModuleOid, 4);

    /* Send a startup done trap (no variable binding) */
    mSnmpAgent->sendTrap("moduleStartupDone", mMyModuleOid, 10);
}

/* MyModule destructor. */
MyModule::~MyModule()
{
    /* Send exit trap, note that here for demonstration purpose we create the variable right before the
     * trap is sent because this is the only time where we will use it. Also, the NMS cannot purposefully
     * read that variable because its max-access is 'accessible-for-notify'. */
    QSNMPVar * var = this->snmpCreateVar("moduleExitMessage", QSNMPType_OctetStr, QSNMPMaxAccess_Notify, mMyModuleOid, 5);
    mSnmpAgent->sendTrap("moduleExiting", mMyModuleOid, 11, var);

    /* No need to delete SNMP variables here because they are automatically freed by QSNMPModule base class destructor. */
}

/* Returns the value associated to SNMP variable var (in order to respond to a SNMP GET request). */
QVariant MyModule::snmpGetValue(const QSNMPVar * var)
{
    /* Identify target variable.
     * Note that in this class, we can switch based on the fieldId value only because
     * all SNMP variables in MyModule were created with the same groupOid and indexes,
     * meaning that the fieldId is sufficient to uniquely identify the variable.
     * In other cases, the application code should identify the target variable either
     * via the variable name, complete OID, or pointer value. */
    switch(var->fieldId())
    {
    case 1: /* moduleUptime: TimeTicks */
        return QVariant((quint32)(this->startDateTime().msecsTo(QDateTime::currentDateTime())/10));
    case 2: /* moduleValueA: Integer32 */
        return QVariant((qint32)this->A());
    case 3: /* moduleValueB: Integer32 */
        return QVariant((qint32)this->B());
    case 4: /* moduleValueSum: Integer32 */
        return QVariant((qint32)this->sum());
    case 5: /* moduleExitMessage: DisplayString */
        return QVariant(QString("Goodbye!"));
    default:
        break;
    }
    return QVariant();
}

/* Sets the value associated to SNMP variable var (in order to fulfil a SNMP SET request). */
bool MyModule::snmpSetValue(const QSNMPVar * var, const QVariant & v)
{
    /* Identify target variable.
     * Note that in this class, we can switch based on the fieldId value only because
     * all SNMP variables in MyModule were created with the same groupOid and indexes,
     * meaning that the fieldId is sufficient to uniquely identify the variable.
     * In other cases, the application code should identify the target variable either
     * via the variable name, complete OID, or pointer value. */
    switch(var->fieldId())
    {
    case 2: /* moduleValueA: Integer32 */
    {
        this->setA(v.value<qint32>());
        return true;
    }
    case 3: /* moduleValueB: Integer32 */
    {
        this->setB(v.value<qint32>());
        return true;
    }
    default:
        break;
    }
    return false;
}

/* Returns the module's startup date and time */
QDateTime MyModule::startDateTime() const
{
    return mStartDateTime;
}

/* Returns the value of A. */
qint32 MyModule::A() const
{
    return mA;
}

/* Sets the value of A. */
void MyModule::setA(qint32 val)
{
    if(mA != val)
    {
        /* Set new value. */
        mA = val;

        /* The value changed, which means the sum has changed.
         * Send a trap with some variable bindings. */
        QSNMPVarList varList;
        varList << this->snmpVar("moduleValueA");
        varList << this->snmpVar("moduleValueB");
        varList << this->snmpVar("moduleValueSum");
        mSnmpAgent->sendTrap("moduleValueSumChanged", mMyModuleOid, 12, varList);
    }
}

/* Returns the value of B. */
qint32 MyModule::B() const
{
    return mB;
}

/* Sets the value of B. */
void MyModule::setB(qint32 val)
{
    if(mB != val)
    {
        /* Set new value. */
        mB = val;

        /* The value changed, which means the sum has changed.
         * Send a trap with some variable bindings. */
        QSNMPVarList varList;
        varList << this->snmpVar("moduleValueA");
        varList << this->snmpVar("moduleValueB");
        varList << this->snmpVar("moduleValueSum");
        mSnmpAgent->sendTrap("moduleValueSumChanged", mMyModuleOid, 12, varList);
    }
}

/* Returns the sum A+B. */
qint32 MyModule::sum() const
{
    return mA+mB;
}
