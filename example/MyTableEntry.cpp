#include "MyTableEntry.h"

/* Constructs a MyTableEntry object, it implements a QSNMP Module. */
MyTableEntry::MyTableEntry(QSNMPAgent * snmpAgent, quint32 index1, quint32 index2, Color defaultColor)
    : QSNMPModule(snmpAgent, QString("entry(%1, %2)").arg(index1).arg(index2))
{
    /* Store local parameters */
    mSnmpAgent = snmpAgent;
    mIndex1 = index1;
    mIndex2 = index2;
    mColor = defaultColor;

    /* MyTableEntry group OID and indexes (for tabular variables) */
    QSNMPOid myTableEntryOid;
    myTableEntryOid << 1 << 3 << 6 << 1 << 4 << 1 /* iso.org.dod.internet.private.enterprises */
                                                << 12345 /* .example */
                                                << 2 /* .myTable */
                                                << 1; /* .myTableEntry */
    QSNMPOid indexes  = QSNMPOid() << mIndex1 << mIndex2;

    /* Create SNMP variables */
    mVar1 = this->snmpCreateVar("myTableEntryIndex1", QSNMPType_Gauge, QSNMPMaxAccess_ReadOnly, myTableEntryOid, 1, indexes);
    mVar2 = this->snmpCreateVar("myTableEntryIndex2", QSNMPType_Gauge, QSNMPMaxAccess_ReadOnly, myTableEntryOid, 2, indexes);
    mVar3 = this->snmpCreateVar("myTableEntryColor", QSNMPType_Integer, QSNMPMaxAccess_ReadWrite, myTableEntryOid, 3, indexes);
}

/* MyTableEntry destructor. */
MyTableEntry::~MyTableEntry()
{
    /* No need to delete SNMP variables here because they are automatically freed by QSNMPModule base class destructor. */
}

/* Returns the value associated to SNMP variable var (in order to respond to a SNMP GET request). */
QVariant MyTableEntry::snmpGetValue(const QSNMPVar * var)
{
    /* Switch based on target variable. */
    if(var == mVar1)
        return QVariant((quint32)mIndex1);
    else if(var == mVar2)
        return QVariant((quint32)mIndex2);
    else if(var == mVar3)
        return QVariant((qint32)mColor);
    return QVariant();
}

/* Sets the value associated to SNMP variable var (in order to fulfil a SNMP SET request). */
bool MyTableEntry::snmpSetValue(const QSNMPVar * var, const QVariant & v)
{
    /* Only "myTableEntryColor" has write-access. */
    if(var == mVar3) // Same as: if(var->name() == "myTableEntryColor")
    {
        switch(v.value<qint32>())
        {
        case Red:
            mColor = Red;
            return true;
        case Green:
            mColor = Green;
            return true;
        case Blue:
            mColor = Blue;
            return true;
        default:
            /* Bad value ! Will return false. */
            break;
        }
    }
    return false;
}
