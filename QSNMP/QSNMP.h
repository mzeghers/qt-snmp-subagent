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
    QSNMPType_Integer = 0,  /* qint32 */
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
static const QSNMPOid qsnmpScalarIndex = QSNMPOid() << 0; // Scalar variable OID index (.0), as opposed to tabular variable

/* SNMP agent forward declaration */
class QSNMPAgent;

/* SNMP module forward declaration */
class QSNMPModule;

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
/******************** SNMP MODULE ********************/
/*****************************************************/

/* QSNMPModule class definition */
class QSNMPModule
{

public:
                                QSNMPModule(QSNMPAgent * snmpAgent, const QString & snmpName);
    virtual                     ~QSNMPModule();

    /* Getters */
    QSNMPAgent *                snmpAgent() const;
    const QString &             snmpName() const;
    const QSNMPVarList &        snmpVarList() const;

    /* Get/Set a variable's value, implemented in the user-derived class */
    virtual QVariant            snmpGet(const QSNMPVar * var) = 0;
    virtual bool                snmpSet(const QSNMPVar * var, const QVariant & v) = 0;

protected:
    /* Add/Remove variables to/from this module */
    QSNMPVar *                  snmpCreateVar(const QString & name, QSNMPType_e type, QSNMPMaxAccess_e maxAccess,
                                              const QSNMPOid & groupOid, quint32 fieldId, const QSNMPOid & indexes = qsnmpScalarIndex);
    bool                        snmpDeleteVar(QSNMPVar * var);
    void                        snmpDeleteAllVars();

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
                                QSNMPVar(QSNMPModule * module, const QString & name, QSNMPType_e type, QSNMPMaxAccess_e maxAccess,
                                             const QSNMPOid & groupOid, quint32 fieldId, const QSNMPOid & indexes = qsnmpScalarIndex);
    virtual                     ~QSNMPVar();

    /* Getters */
    QSNMPModule *               module() const;
    const QString &             name() const;
    QSNMPType_e                 type() const;
    QSNMPMaxAccess_e            maxAccess() const;
    const QSNMPOid &            groupOid() const;
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
    QSNMPModule *               mModule;
    QString                     mName;
    QSNMPType_e                 mType;
    QSNMPMaxAccess_e            mMaxAccess;
    QSNMPOid                    mGroupOid;
    quint32                     mFieldId;
    QSNMPOid                    mIndexes;
    QSNMPOid                    mOid;
    QString                     mOidString;
    QString                     mDescrString;

    /* Registration (opaque) */
    void *                      mRegistration;

};

#endif // QSNMP_H
