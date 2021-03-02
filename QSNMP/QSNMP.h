#ifndef QSNMP_H
#define QSNMP_H

#include <QString>
#include <QMetaType>
#include <QVector>
#include <QMap>
#include <QList>
#include <QObject>
#include <QVariant>
#include <QElapsedTimer>



/**********************************************************/
/******************** TYPE DEFINITIONS ********************/
/**********************************************************/

/* SNMP data types and their Qt counter-part */
typedef enum
{
    QSNMPType_Integer = 0,  /* quint32 */
    QSNMPType_OctetStr,     /* QString */
    QSNMPType_BitStr,       /* QString */
    QSNMPType_Opaque,       /* QByteArray */
    QSNMPType_ObjectId,     /* QSNMPOid */
    QSNMPType_TimeTicks,    /* quint32 */
    QSNMPType_Gauge,        /* quint32 */
    QSNMPType_Counter,      /* quint32 */
    QSNMPType_IpAddress,    /* quint32 */
    QSNMPType_Counter64,    /* quint64 */
    QSNMPType_Null,
} QSNMPType_e;
QString toString(QSNMPType_e snmpType);
QSNMPType_e toSNMPType(const QString & str);
Q_DECLARE_METATYPE(QSNMPType_e)

/* SNMP maximum access level for a variable */
typedef enum
{
    QSNMPMaxAccess_Notify = 0,
    QSNMPMaxAccess_ReadOnly,
    QSNMPMaxAccess_ReadWrite,
    QSNMPMaxAccess_Invalid,
} QSNMPMaxAccess_e;
QString toString(QSNMPMaxAccess_e snmpMaxAccess);
QSNMPMaxAccess_e toSNMPMaxAccess(const QString & str);
Q_DECLARE_METATYPE(QSNMPMaxAccess_e)

/* SNMP OID stored as QVector */
typedef QVector<quint32> QSNMPOid;
QString toString(const QSNMPOid & oid);
static const QSNMPOid scalarOidIndex = QSNMPOid() << 0; // Scalar variable OID (.0), as opposed to tabular variable

/* SNMP agent forward declaration */
class QSNMPAgent;

/* SNMP object forward declaration */
class QSNMPObject;

/* SNMP variable forward declaration */
class QSNMPVar;
typedef QMap<QString, QSNMPVar *> QSNMPVarMap; // Map of SNMP variables, where key is OID=toString(QSNMPOid)
typedef QList<QSNMPVar *> QSNMPVarList; // List of SNMP variables



/****************************************************/
/******************** SNMP AGENT ********************/
/****************************************************/

/* QSNMPAgent class definition */
class QSNMPAgent : public QObject
{ Q_OBJECT

public:
                                QSNMPAgent(const QString & agentName, const QString & agentAddr = QString());
    virtual                     ~QSNMPAgent();

    /* Variables managed under this agent */
    const QSNMPVarMap &         varMap() const;
    QString                     registerVar(QSNMPVar * var);
    void                        unregisterVar(QSNMPVar * var);

    /* Traps */
    bool                        trapsEnabled() const;
    void                        setTrapsEnabled(bool enabled);

private:
    /* Name */
    QString                     mAgentName;

    /* Variables */
    QSNMPVarMap                 mVarMap;

    /* Traps */
    bool                        mTrapsEnabled;

    /* SNMP agent event processing */
    QElapsedTimer               mTimer;

private slots:
    /* SNMP agent event processing */
    void                        processEvents();

};



/*****************************************************/
/******************** SNMP OBJECT ********************/
/*****************************************************/

/* QSNMPObject class definition */
class QSNMPObject
{

public:
                                QSNMPObject(QSNMPAgent * snmpAgent, const QString & snmpName);
    virtual                     ~QSNMPObject();

    /* Getters */
    QSNMPAgent *                snmpAgent() const;
    const QString &             snmpName() const;
    const QSNMPVarList &        snmpVarList() const;
    QSNMPVar *                  snmpVar(const QString & name) const;
    QSNMPVar *                  snmpVar(quint32 fieldId) const;

    /* Get/Set a variable's value, implemented in the subclass */
    virtual QVariant            snmpGet(const QSNMPVar * var) = 0;
    virtual bool                snmpSet(const QSNMPVar * var, const QVariant & v) = 0;

protected:
    /* Add/Remove variables to/from this object */
    QSNMPVar *                  snmpAddNotifyVar(const QString & name, QSNMPType_e type,
                                                 const QSNMPOid & baseOid, quint32 fieldId, const QSNMPOid & indexes = scalarOidIndex);
    QSNMPVar *                  snmpAddReadOnlyVar(const QString & name, QSNMPType_e type,
                                                   const QSNMPOid & baseOid, quint32 fieldId, const QSNMPOid & indexes = scalarOidIndex);
    QSNMPVar *                  snmpAddReadWriteVar(const QString & name, QSNMPType_e type,
                                                    const QSNMPOid & baseOid, quint32 fieldId, const QSNMPOid & indexes = scalarOidIndex);
    void                        snmpRemoveVar(QSNMPVar * var);
    void                        snmpRemoveVar(const QString & name);
    void                        snmpRemoveVar(quint32 fieldId);
    void                        snmpRemoveAllVars();

private:
    QSNMPAgent *                mSnmpAgent;
    QString                     mSnmpName;
    QSNMPVarList                mSnmpVarList;

};



/*******************************************************/
/******************** SNMP VARIABLE ********************/
/*******************************************************/

/* SNMPVar class definition */
class QSNMPVar
{

public:
                                QSNMPVar(QSNMPObject * object, const QString & name, QSNMPType_e type, QSNMPMaxAccess_e maxAccess,
                                             const QSNMPOid & baseOid, quint32 fieldId, const QSNMPOid & indexes = scalarOidIndex);
    virtual                     ~QSNMPVar();

    /* Getters */
    QSNMPObject *               object() const;
    const QString &             name() const;
    QSNMPType_e                 type() const;
    QSNMPMaxAccess_e            maxAccess() const;
    const QSNMPOid &            baseOid() const;
    quint32                     fieldId() const;
    const QSNMPOid &            indexes() const;
    const QSNMPOid &            oid() const;
    const QString &             oidString() const;
    const QString &             descrString() const;

    /* Registration (opaque) */
    void *                      registration() const;
    void                        setRegistration(void * registration);

    /* Value getter/setter */
    QVariant                    get() const;
    bool                        set(const QVariant & v) const;

private:
    /* Constants */
    QSNMPObject *               mObject;
    QString                     mName;
    QSNMPType_e                 mType;
    QSNMPMaxAccess_e            mMaxAccess;
    QSNMPOid                    mBaseOid;
    quint32                     mFieldId;
    QSNMPOid                    mIndexes;
    QSNMPOid                    mOid;
    QString                     mOidString;
    QString                     mDescrString;

    /* Registration (opaque) */
    void *                      mRegistration;

};

#endif // QSNMP_H
