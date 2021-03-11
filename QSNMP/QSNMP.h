/* MIT License

Copyright (c) 2021 mzeghers

This file is part of the qt-snmp-subagent repository
https://github.com/mzeghers/qt-snmp-subagent

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

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

/* SNMP log types */
typedef enum
{
    QSNMPLogType_GET = 0,           // debug ?
    QSNMPLogType_SET,               // info ?
    QSNMPLogType_TRAP,              // info ?
    QSNMPLogType_RegisterOK,        // debug ?
    QSNMPLogType_RegisterFail,      // error ?
    QSNMPLogType_UnregisterOK,      // debug ?
    QSNMPLogType_UnregisterFail,    // error ?
} QSNMPLogType_e;
Q_DECLARE_METATYPE(QSNMPLogType_e)

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
    bool                        registerVar(QSNMPVar * var);
    void                        unregisterVar(QSNMPVar * var);

    /* SNMP agent event processing (Net-SNMP internal) */
    int                         handler(void * handler, void * reginfo, void * reqinfo, void * requests);

    /* Traps */
    bool                        trapsEnabled() const;
    void                        setTrapsEnabled(bool enabled);
    void                        sendTrap(const QString & name, const QSNMPOid & groupOid,
                                         quint32 fieldId, QSNMPVar * var = nullptr);
    void                        sendTrap(const QString & name, const QSNMPOid & groupOid,
                                         quint32 fieldId, const QSNMPVarList & varList);

private:
    /* Name */
    QString                     mAgentName;

    /* Variables */
    QSNMPVarMap                 mVarMap;

    /* SNMP agent event processing */
    QElapsedTimer               mTimer;

    /* Traps */
    bool                        mTrapsEnabled;

private slots:
    /* SNMP agent event processing */
    void                        processEvents();

signals:
    /* Logging */
    void                        newLog(QSNMPLogType_e logType, const QString & msg);

};



/*****************************************************/
/******************** SNMP MODULE ********************/
/*****************************************************/

/* QSNMPModule class definition */
class QSNMPModule
{

public:
                                QSNMPModule(QSNMPAgent * snmpAgent);
    virtual                     ~QSNMPModule();

    /* Getters */
    QSNMPAgent *                snmpAgent() const;
    const QSNMPVarList &        snmpVarList() const;
    QSNMPVar *                  snmpVar(const QString & name) const;
    QSNMPVar *                  snmpVar(const QSNMPOid & oid) const;

    /* Get variable's value, implemented in the user-derived class. */
    virtual QVariant            snmpGetValue(const QSNMPVar * var) = 0;

    /* Get variable's value, implemented in the user-derived class. Return true on
     * success, or false to respond with a bad value error. */
    virtual bool                snmpSetValue(const QSNMPVar * var, const QVariant & v) = 0;

protected:
    /* Add/Remove variables to/from this module */
    QSNMPVar *                  snmpCreateVar(const QString & name, QSNMPType_e type, QSNMPMaxAccess_e maxAccess,
                                              const QSNMPOid & groupOid, quint32 fieldId, const QSNMPOid & indexes = qsnmpScalarIndex);
    bool                        snmpDeleteVar(QSNMPVar * var);
    void                        snmpDeleteAllVars();

private:
    QSNMPAgent *                mSnmpAgent;
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
    const QString &             fullName() const;

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
    QString                     mFullName;

    /* Registration (opaque) */
    void *                      mRegistration;

};

#endif // QSNMP_H
