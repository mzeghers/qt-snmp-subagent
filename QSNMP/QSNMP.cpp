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

#include "QSNMP.h"
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <QTimer>



/**********************************************************/
/******************** TYPE DEFINITIONS ********************/
/**********************************************************/

/* QSNMPType_e string conversions */
static const QMap<QSNMPType_e, QString> initSNMPTypeMap()
{
    qRegisterMetaType<QSNMPType_e>("QSNMPType_e");
    QMap<QSNMPType_e, QString> map;
    map.insert(QSNMPType_Integer, "INTEGER");
    map.insert(QSNMPType_OctetStr, "OCTET-STRING");
    map.insert(QSNMPType_BitStr, "BIT-STRING");
    map.insert(QSNMPType_Opaque, "OPAQUE");
    map.insert(QSNMPType_ObjectId, "OBJECT-ID");
    map.insert(QSNMPType_TimeTicks, "TIMETICKS");
    map.insert(QSNMPType_Gauge, "GAUGE");
    map.insert(QSNMPType_Counter, "COUNTER");
    map.insert(QSNMPType_IpAddress, "IPADDRESS");
    map.insert(QSNMPType_Counter64, "COUNTER64");
    return map;
}
static const QMap<QSNMPType_e, QString> snmpTypeMap = initSNMPTypeMap();
QString toString(QSNMPType_e snmpType)
{
    return snmpTypeMap.value(snmpType, "NULL");
}
QSNMPType_e toSNMPType(const QString & str)
{
    return snmpTypeMap.key(str.toUpper(), QSNMPType_Null);
}

/* QSNMPMaxAccess_e string conversions */
static const QMap<QSNMPMaxAccess_e, QString> initSNMPMaxAccessMap()
{
    qRegisterMetaType<QSNMPMaxAccess_e>("QSNMPMaxAccess_e");
    QMap<QSNMPMaxAccess_e, QString> map;
    map.insert(QSNMPMaxAccess_Notify, "NOTIFY");
    map.insert(QSNMPMaxAccess_ReadOnly, "RO");
    map.insert(QSNMPMaxAccess_ReadWrite, "RW");
    return map;
}
static const QMap<QSNMPMaxAccess_e, QString> snmpMaxAccessMap = initSNMPMaxAccessMap();
QString toString(QSNMPMaxAccess_e snmpMaxAccess)
{
    return snmpMaxAccessMap.value(snmpMaxAccess, "INVALID");
}
QSNMPMaxAccess_e toSNMPMaxAccess(const QString & str)
{
    return snmpMaxAccessMap.key(str.toUpper(), QSNMPMaxAccess_Invalid);
}

/* OID to string conversion */
QString toString(const QSNMPOid & oid)
{
    QString str;
    for(int k=0; k<oid.size(); k++)
        str.append(QString(".%1").arg(oid[k]));
    return str;
}

/* Qt to/from Net-SNMP OID conversions */
static void convertOidQtToSnmp(const QSNMPOid & qtOid, oid * snmpOid, size_t * snmpOidLen, size_t maxSnmpOidLen = 32)
{
    size_t maxLen = qMin((size_t)qtOid.size(), maxSnmpOidLen);
    for(size_t k=0; k<maxLen; k++)
        snmpOid[k] = qtOid[k];
    *snmpOidLen = maxLen;
}
static QSNMPOid convertOidSnmpToQt(oid * snmpOid, size_t snmpOidLen)
{
    QSNMPOid qtOid;
    if(snmpOid)
    {
        for(size_t k=0; k<snmpOidLen; k++)
            qtOid << snmpOid[k];
    }
    return qtOid;
}



/******************************************************************/
/******************** VARIABLE GET/SET HANDLER ********************/
/******************************************************************/

/* Net-SNMP request callback, forward to QSNMPAgent handler. */
static int variableHandler(netsnmp_mib_handler * handler, netsnmp_handler_registration * reginfo,
                            netsnmp_agent_request_info * reqinfo, netsnmp_request_info * requests)
{
    /* Call agent handler */
    QSNMPAgent * agent = static_cast<QSNMPAgent*>(reginfo->my_reg_void);
    if(!agent)
        return SNMP_ERR_GENERR;
    return agent->handler(handler, reginfo, reqinfo, requests);
}



/****************************************************/
/******************** SNMP AGENT ********************/
/****************************************************/

/* SNMP agent constructor, initializes Net-SNMP library as AgentX sub-agent. */
QSNMPAgent::QSNMPAgent(const QString & agentName, const QString & agentAddr)
{
    /* Initialize member variables */
    mAgentName = agentName;
    mVarMap.clear();
    mTimer.start();
    mTrapsEnabled = true;

    /* Initialize agentX/SNMP libraries */
    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 1);
    if(!mAgentName.isEmpty())
        netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_X_SOCKET, agentAddr.toStdString().c_str());
    init_agent(mAgentName.toStdString().c_str());
    init_snmp(mAgentName.toStdString().c_str());

    /* Initial event processing start */
    QTimer::singleShot(0, this, SLOT(processEvents()));
}

/* SNMP agent destructor, shutdowns Net-SNMP library. */
QSNMPAgent::~QSNMPAgent()
{
    snmp_shutdown(mAgentName.toStdString().c_str());
    shutdown_agent();
}

/* Returns the map of SNMP variables managed by this agent. */
const QSNMPVarMap & QSNMPAgent::varMap() const
{
    return mVarMap;
}

/* Registers a SNMP variable to this agent.
 * This function is called by the QSNMPModule snmpCreateVar function and
 * should typically not be called directly by the user application.
 * Returns true on success, or false on failure. */
bool QSNMPAgent::registerVar(QSNMPVar * var)
{
    /* Check if already registered */
    if(mVarMap.contains(var->oidString()))
    {
        emit this->newLog(QSNMPLogType_RegisterFail,
                          QString("Could not register SNMP variable %1: already registered").arg(var->fullName()));
        return false;
    }

    /* Notify variables are not actually registered with the Net-SNMP library */
    if(var->maxAccess() != QSNMPMaxAccess_Notify)
    {
        /* Create handler registration */
        var->setRegistration(nullptr);
        QVector<oid> oidVector;
        foreach(quint32 k, var->oid())
            oidVector << k;
        netsnmp_handler_registration * reginfo = netsnmp_create_handler_registration(var->name().toStdString().c_str(),
                                                                                     variableHandler,
                                                                                     (const oid *)oidVector.constData(),
                                                                                     oidVector.size(),
                                                                                     (var->maxAccess()==QSNMPMaxAccess_ReadWrite)?HANDLER_CAN_RWRITE:HANDLER_CAN_RONLY);
        if(!reginfo)
        {
            emit this->newLog(QSNMPLogType_RegisterFail,
                              QString("Could not register SNMP variable %1: handler creation failed").arg(var->fullName()));
            return false;
        }

        /* Our context data */
        reginfo->my_reg_void = this;

        /* Register instance */
        int rc = netsnmp_register_instance(reginfo);
        if(rc == MIB_REGISTRATION_FAILED)
        {
            netsnmp_handler_registration_free(reginfo);
            emit this->newLog(QSNMPLogType_RegisterFail,
                              QString("Could not register SNMP variable %1: handler registration failed").arg(var->fullName()));
            return false;
        }
        else if(rc == MIB_DUPLICATE_REGISTRATION)
        {
            netsnmp_handler_registration_free(reginfo);
            emit this->newLog(QSNMPLogType_RegisterFail,
                              QString("Could not register SNMP variable %1: duplicate handler registration").arg(var->fullName()));
            return false;
        }
        var->setRegistration(reginfo);
    }

    /* Done, add to map */
    mVarMap.insert(var->oidString(), var);
    emit this->newLog(QSNMPLogType_RegisterOK,
                      QString("Registered SNMP variable %1").arg(var->fullName()));
    return true;
}

/* Unregisters a SNMP variable from this agent.
 * This function is called by the QSNMPModule snmpDeleteVar function and
 * should typically not be called directly by the user application. */
void QSNMPAgent::unregisterVar(QSNMPVar * var)
{
    if(mVarMap.contains(var->oidString()))
    {
        /* Notify variables are not actually registered with the Net-SNMP library */
        emit this->newLog(QSNMPLogType_UnregisterOK,
                          QString("Unregistered SNMP variable %1").arg(var->fullName()));
        if(var->maxAccess() != QSNMPMaxAccess_Notify)
        {
            /* Unregister variable and remove from map */
            netsnmp_unregister_handler((netsnmp_handler_registration *)var->registration());
            var->setRegistration(nullptr);
            mVarMap.remove(var->oidString());
        }
    }
    else
        emit this->newLog(QSNMPLogType_UnregisterFail,
                          QString("Could not unregister SNMP variable %1: not registered").arg(var->fullName()));
}

/* The main variable GET/SET callback handler, called by the Net-SNMP library on GET/SET messages.
 * Note that here we use the same callback for all variables, so that implementation-specific
 * behavior is determined outside of the QSNMP context. */
int QSNMPAgent::handler(void * _handler, void * _reginfo, void * _reqinfo, void * _requests)
{
    Q_UNUSED(_handler)
    Q_UNUSED(_reginfo)
    netsnmp_agent_request_info * reqinfo = (netsnmp_agent_request_info * )_reqinfo;
    netsnmp_request_info * requests = (netsnmp_request_info * )_requests;

    /* Variables list */
    netsnmp_variable_list * netsnmp_varlist = requests->requestvb;

    /* Action */
    if((reqinfo->mode == MODE_GET) || (reqinfo->mode == MODE_GETNEXT))
    {
        /* Multiple variables supported for GET requests */
        while(netsnmp_varlist)
        {
            /* Get the corresponding variable from agent */
            QSNMPOid qtOid = convertOidSnmpToQt(netsnmp_varlist->name, netsnmp_varlist->name_length);
            QSNMPVar * var = mVarMap.value(toString(qtOid), nullptr);
            if(!var)
                return SNMP_ERR_GENERR;

            /* Check access, should not be needed because the Net-SNMP library will do it for us, but still... */
            if(var->maxAccess() < QSNMPMaxAccess_ReadOnly)
                return SNMP_ERR_GENERR;

            /* Read value from user application */
            QVariant v = var->get();
            emit this->newLog(QSNMPLogType_GET,
                              QString("SNMP-GET: %1 [%2] : %4 = %5").arg(var->fullName())
                                                                    .arg(toString(var->maxAccess()))
                                                                    .arg(toString(var->type()))
                                                                    .arg(v.toString()));

            /* Convert from Qt to SNMP data */
            switch(var->type())
            {
            case QSNMPType_Integer: /* qint32 */
            {
                qint32 value = v.value<qint32>();
                snmp_set_var_typed_value(netsnmp_varlist, ASN_INTEGER, &value, 4);
                break;
            }
            case QSNMPType_OctetStr: /* QString */
            {
                QByteArray byteArray = v.value<QString>().toUtf8();
                snmp_set_var_typed_value(netsnmp_varlist, ASN_OCTET_STR, byteArray.constData(), byteArray.size());
                break;
            }
            case QSNMPType_BitStr: /* QString */
            {
                QByteArray byteArray = v.value<QString>().toUtf8();
                snmp_set_var_typed_value(netsnmp_varlist, ASN_BIT_STR, byteArray.constData(), byteArray.size());
                break;
            }
            case QSNMPType_Opaque: /* QByteArray */
            {
                QByteArray byteArray = v.value<QByteArray>();
                snmp_set_var_typed_value(netsnmp_varlist, ASN_OPAQUE, byteArray.constData(), byteArray.size());
                break;
            }
            case QSNMPType_ObjectId: /* QSNMPOid */
            {
                QSNMPOid qtOid = v.value<QSNMPOid>();
                oid snmpOid[32];
                size_t snmpOidLen;
                convertOidQtToSnmp(qtOid, snmpOid, &snmpOidLen, 32);
                snmp_set_var_typed_value(netsnmp_varlist, ASN_OBJECT_ID, snmpOid, snmpOidLen*sizeof(oid));
                break;
            }
            case QSNMPType_TimeTicks: /* quint32 */
            {
                quint32 value = v.value<quint32>();
                snmp_set_var_typed_value(netsnmp_varlist, ASN_TIMETICKS, &value, 4);
                break;
            }
            case QSNMPType_Gauge: /* quint32 */
            {
                quint32 value = v.value<quint32>();
                snmp_set_var_typed_value(netsnmp_varlist, ASN_GAUGE, &value, 4);
                break;
            }
            case QSNMPType_Counter: /* quint32 */
            {
                quint32 value = v.value<quint32>();
                snmp_set_var_typed_value(netsnmp_varlist, ASN_COUNTER, &value, 4);
                break;
            }
            case QSNMPType_IpAddress: /* quint32 */
            {
                quint32 value = v.value<quint32>();
                quint8 ip[4];
                ip[0] = (value >> 24) & 0xFF;
                ip[1] = (value >> 16) & 0xFF;
                ip[2] = (value >> 8) & 0xFF;
                ip[3] = (value >> 0) & 0xFF;
                snmp_set_var_typed_value(netsnmp_varlist, ASN_IPADDRESS, ip, 4);
                break;
            }
            case QSNMPType_Counter64: /* quint64 */
            {
                quint64 value = v.value<quint64>();
                snmp_set_var_typed_value(netsnmp_varlist, ASN_COUNTER64, &value, 8);
                break;
            }
            case QSNMPType_Null:
                return SNMP_ERR_GENERR;
            default:
                return SNMP_ERR_GENERR;
            }

            /* Next variable */
            netsnmp_varlist = netsnmp_varlist->next_variable;
        }
    }
    else if(reqinfo->mode == MODE_SET_ACTION)
    {
        /* Only single variable supported for SET requests */
        if(netsnmp_varlist->next_variable)
            return SNMP_ERR_GENERR;

        /* Get the corresponding variable from agent */
        QSNMPOid qtOid = convertOidSnmpToQt(netsnmp_varlist->name, netsnmp_varlist->name_length);
        QSNMPVar * var = mVarMap.value(toString(qtOid), nullptr);
        if(!var)
            return SNMP_ERR_GENERR;

        /* Check access, should not be needed because the Net-SNMP library will do it for us, but still... */
        if(var->maxAccess() < QSNMPMaxAccess_ReadWrite)
            return SNMP_ERR_GENERR;

        /* Convert from SNMP to Qt data */
        QVariant v;
        bool validType = true;
        switch(var->type())
        {
        case QSNMPType_Integer: /* qint32 */
        {
            if(netsnmp_varlist->type != ASN_INTEGER)
                validType = false;
            qint32 value = *((qint32*)netsnmp_varlist->val.integer);
            v.setValue<qint32>(value);
            break;
        }
        case QSNMPType_OctetStr: /* QString */
        {
            if(netsnmp_varlist->type != ASN_OCTET_STR)
                validType = false;
            QString str = QString(QByteArray((const char*)netsnmp_varlist->val.string, netsnmp_varlist->val_len));
            v.setValue<QString>(str);
            break;
        }
        case QSNMPType_BitStr: /* QString */
        {
            if(netsnmp_varlist->type != ASN_BIT_STR)
                validType = false;
            QString str = QString(QByteArray((const char*)netsnmp_varlist->val.bitstring, netsnmp_varlist->val_len));
            v.setValue<QString>(str);
            break;
        }
        case QSNMPType_Opaque: /* QByteArray */
        {
            if(netsnmp_varlist->type != ASN_OPAQUE)
                validType = false;
            QByteArray ba = QByteArray((const char*)netsnmp_varlist->val.string, netsnmp_varlist->val_len);
            v.setValue<QByteArray>(ba);
            break;
        }
        case QSNMPType_ObjectId: /* QSNMPOid */
        {
            if(netsnmp_varlist->type != ASN_OBJECT_ID)
                validType = false;
            QSNMPOid qtOid = convertOidSnmpToQt(netsnmp_varlist->val.objid, netsnmp_varlist->val_len/sizeof(oid));
            v.setValue<QSNMPOid>(qtOid);
            break;
        }
        case QSNMPType_TimeTicks: /* quint32 */
        {
            if(netsnmp_varlist->type != ASN_TIMETICKS)
                validType = false;
            quint32 value = *((quint32*)netsnmp_varlist->val.integer);
            v.setValue<quint32>(value);
            break;
        }
        case QSNMPType_Gauge: /* quint32 */
        {
            if(netsnmp_varlist->type != ASN_GAUGE)
                validType = false;
            quint32 value = *((quint32*)netsnmp_varlist->val.integer);
            v.setValue<quint32>(value);
            break;
        }
        case QSNMPType_Counter: /* quint32 */
        {
            if(netsnmp_varlist->type != ASN_COUNTER)
                validType = false;
            quint32 value = *((quint32*)netsnmp_varlist->val.integer);
            v.setValue<quint32>(value);
            break;
        }
        case QSNMPType_IpAddress: /* quint32 */
        {
            if(netsnmp_varlist->type != ASN_IPADDRESS)
                validType = false;
            quint8 * ip = netsnmp_varlist->val.string;
            quint32 value = ((quint32)ip[0] << 24) | ((quint32)ip[1] << 16) | ((quint32)ip[2] << 8) | ((quint32)ip[3] << 0);
            v.setValue<quint32>(value);
            break;
        }
        case QSNMPType_Counter64: /* quint64 */
        {
            if(netsnmp_varlist->type != ASN_COUNTER64)
                validType = false;
            quint64 value = ((quint64)netsnmp_varlist->val.counter64->high << 32) | (quint64)netsnmp_varlist->val.counter64->low;
            v.setValue<quint64>(value);
            break;
        }
        case QSNMPType_Null:
            return SNMP_ERR_GENERR;
        default:
            return SNMP_ERR_GENERR;
        }

        /* Write value to user application */
        if(validType)
        {
            emit this->newLog(QSNMPLogType_SET,
                              QString("SNMP-SET: %1 [%2] : %4 = %5").arg(var->fullName())
                                                                    .arg(toString(var->maxAccess()))
                                                                    .arg(toString(var->type()))
                                                                    .arg(v.toString()));
            if(!var->set(v))
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
        }
        else
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_GENERR);
    }

    /* Done */
    return SNMP_ERR_NOERROR;
}

/* Returns true if SNMP traps are enabled. */
bool QSNMPAgent::trapsEnabled() const
{
    return mTrapsEnabled;
}

/* Sets SNMP traps enablement. */
void QSNMPAgent::setTrapsEnabled(bool enabled)
{
    mTrapsEnabled = enabled;
}

/* Generates and sends an SNMP trap to the Net-SNMP master agent.
 * The 'name' argument can be any as desired by the user application, but it is recommended
 * to use the same one as inside the MIB file to ease log parsing.
 * The 'groupOid' is the OID of the parent group, 'fieldId' is the trap's identifier
 * under the parent group, so that 'groupOid.fieldId' form the NOTIFICATION-TYPE
 * identifier.
 * A variable binding can be added to the trap payload by setting the var argument. */
void QSNMPAgent::sendTrap(const QString & name, const QSNMPOid & groupOid, quint32 fieldId, QSNMPVar * var)
{
    if(var)
        this->sendTrap(name, groupOid, fieldId, QSNMPVarList() << var);
    else
        this->sendTrap(name, groupOid, fieldId, QSNMPVarList());
}

/* Generates and sends an SNMP trap to the Net-SNMP master agent.
 * The 'name' argument can be any as desired by the user application, but it is recommended
 * to use the same one as inside the MIB file to ease log parsing.
 * The 'groupOid' is the OID of the parent group, 'fieldId' is the trap's identifier
 * under the parent group, so that 'groupOid.fieldId' form the NOTIFICATION-TYPE
 * identifier.
 * Multiple variable bindings can be added to the trap payload via the varList argument. */
void QSNMPAgent::sendTrap(const QString & name, const QSNMPOid & groupOid, quint32 fieldId, const QSNMPVarList & varList)
{
    /* Return immediately if traps are disabled */
    if(!mTrapsEnabled)
        return;

    /* Variables binding */
    netsnmp_variable_list * snmpVarList = nullptr;

    /* snmpTrapOID.0 */
    static oid snmpTrapOid[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
    oid trapOid[32];
    size_t trapOidLen;
    convertOidQtToSnmp(QSNMPOid() << groupOid << fieldId, trapOid, &trapOidLen, 32);
    snmp_varlist_add_variable(&snmpVarList, snmpTrapOid, sizeof(snmpTrapOid)/sizeof(oid),
                              ASN_OBJECT_ID, trapOid, trapOidLen*sizeof(oid));

    /* Log */
    emit this->newLog(QSNMPLogType_TRAP,
                      QString("SNMP-TRAP: %1").arg(name));

    /* Variables bindings */
    foreach(QSNMPVar * var, varList)
    {
        if(var)
        {
            /* Variable OID */
            oid varOid[32];
            size_t varOidLen;
            convertOidQtToSnmp(var->oid(), varOid, &varOidLen, 32);

            /* Read value from user application */
            QVariant v = var->get();
            emit this->newLog(QSNMPLogType_TRAP,
                              QString("           => %1 [%2] : %4 = %5").arg(var->fullName())
                                                                        .arg(toString(var->maxAccess()))
                                                                        .arg(toString(var->type()))
                                                                        .arg(v.toString()));

            /* Convert from Qt to SNMP data */
            switch(var->type())
            {
            case QSNMPType_Integer: /* qint32 */
            {
                qint32 value = v.value<qint32>();
                snmp_varlist_add_variable(&snmpVarList, varOid, varOidLen,
                                          ASN_INTEGER, &value, 4);
                break;
            }
            case QSNMPType_OctetStr: /* QString */
            {
                QByteArray byteArray = v.value<QString>().toUtf8();
                snmp_varlist_add_variable(&snmpVarList, varOid, varOidLen,
                                          ASN_OCTET_STR, byteArray.constData(), byteArray.size());
                break;
            }
            case QSNMPType_BitStr: /* QString */
            {
                QByteArray byteArray = v.value<QString>().toUtf8();
                snmp_varlist_add_variable(&snmpVarList, varOid, varOidLen,
                                          ASN_BIT_STR, byteArray.constData(), byteArray.size());
                break;
            }
            case QSNMPType_Opaque: /* QByteArray */
            {
                QByteArray byteArray = v.value<QByteArray>();
                snmp_varlist_add_variable(&snmpVarList, varOid, varOidLen,
                                          ASN_OPAQUE, byteArray.constData(), byteArray.size());
                break;
            }
            case QSNMPType_ObjectId: /* QSNMPOid */
            {
                QSNMPOid qtOid = v.value<QSNMPOid>();
                oid snmpOid[32];
                size_t snmpOidLen;
                convertOidQtToSnmp(qtOid, snmpOid, &snmpOidLen, 32);
                snmp_varlist_add_variable(&snmpVarList, varOid, varOidLen,
                                          ASN_OBJECT_ID, snmpOid, snmpOidLen*sizeof(oid));
                break;
            }
            case QSNMPType_TimeTicks: /* quint32 */
            {
                quint32 value = v.value<quint32>();
                snmp_varlist_add_variable(&snmpVarList, varOid, varOidLen,
                                          ASN_TIMETICKS, &value, 4);
                break;
            }
            case QSNMPType_Gauge: /* quint32 */
            {
                quint32 value = v.value<quint32>();
                snmp_varlist_add_variable(&snmpVarList, varOid, varOidLen,
                                          ASN_GAUGE, &value, 4);
                break;
            }
            case QSNMPType_Counter: /* quint32 */
            {
                quint32 value = v.value<quint32>();
                snmp_varlist_add_variable(&snmpVarList, varOid, varOidLen,
                                          ASN_COUNTER, &value, 4);
                break;
            }
            case QSNMPType_IpAddress: /* quint32 */
            {
                quint32 value = v.value<quint32>();
                quint8 ip[4];
                ip[0] = (value >> 24) & 0xFF;
                ip[1] = (value >> 16) & 0xFF;
                ip[2] = (value >> 8) & 0xFF;
                ip[3] = (value >> 0) & 0xFF;
                snmp_varlist_add_variable(&snmpVarList, varOid, varOidLen,
                                          ASN_IPADDRESS, ip, 4);
                break;
            }
            case QSNMPType_Counter64: /* quint64 */
            {
                quint64 value = v.value<quint64>();
                snmp_varlist_add_variable(&snmpVarList, varOid, varOidLen,
                                          ASN_COUNTER64, &value, 8);
                break;
            }
            case QSNMPType_Null:
                break;
            default:
                break;
            }
        }
    }

    /* Send trap upstream and clean up */
    send_v2trap(snmpVarList);
    snmp_free_varbind(snmpVarList);
}


/* Processes events received by the Net-SNMP library regularly. */
void QSNMPAgent::processEvents()
{
    /* Process incoming SNMP packets until no more packet is available.
     * If a packet was received, relaunch the timer (the timer indicates how much time
     * has elapsed since the last SNMP packet was received). */
    while(agent_check_and_process(0) > 0)
        mTimer.start();

    /* Reprocess events at a later time, optimize both CPU time and latency by changing
     * the wait period depending on when the last SNMP packet was received.
     * There is almost no wait time (fast polling) when a packet was received very
     * recently (because with snmp walks/bulk requests they typically arrive in quick succession).
     * However wait time is increased if no packet was received recently, this reduces CPU
     * consumption (slower polling) in between NMS full updates. */
    int waitMs = qMin((qint64)5, mTimer.elapsed()/5);
    QTimer::singleShot(waitMs, this, SLOT(processEvents()));
}



/*****************************************************/
/******************** SNMP MODULE ********************/
/*****************************************************/

/* Constructor for a SNMP module.
 * Here, a SNMP module is not strictly related to the MIB, but can be seen as
 * a collection of variables, or group, in the MIB syntax. It can represent a
 * group of scalars, or a table entry (group of tabulars), or a combination of
 * both, as required by the user application. */
QSNMPModule::QSNMPModule(QSNMPAgent * snmpAgent)
{
    mSnmpAgent = snmpAgent;
    mSnmpVarList.clear();
}

/* Destructor for a SNMP module. Unregisters and deletes all SNMP variables. */
QSNMPModule::~QSNMPModule()
{
    this->snmpDeleteAllVars();
}

/* Returns the linked SNMP agent. */
QSNMPAgent * QSNMPModule::snmpAgent() const
{
    return mSnmpAgent;
}

/* Returns the list of SNMP variables allocated in this module. */
const QSNMPVarList & QSNMPModule::snmpVarList() const
{
    return mSnmpVarList;
}

/* Returns the first SNMP variable with given name from the list of
 * SNMP variables allocated in this module.
 * Returns nullptr if none found. */
QSNMPVar * QSNMPModule::snmpVar(const QString & name) const
{
    foreach(QSNMPVar * var, mSnmpVarList)
    {
        if(var->name() == name)
            return var;
    }
    return nullptr;
}

/* Returns the first SNMP variable with given OID from the list of
 * SNMP variables allocated in this module.
 * Returns nullptr if none found. */
QSNMPVar * QSNMPModule::snmpVar(const QSNMPOid & oid) const
{
    foreach(QSNMPVar * var, mSnmpVarList)
    {
        if(var->oid() == oid)
            return var;
    }
    return nullptr;
}

/* Creates a SNMP variable under this module and registers it with the agent.
 * The 'name' argument can be any as desired by the user application, but it is recommended
 * to use the same one as inside the MIB file to ease log parsing.
 * The 'type' and 'maxAccess' arguments determine the data type and R/W permissions.
 * The 'groupOid' is the OID of the parent group, 'fieldId' is the variable's identifier
 * under the parent group, and 'indexes' can either be qsnmpScalarIndex (.0) for scalar
 * variables or a list of values for tabular variables.
 * The complete variable OID is thus 'groupOid.fieldId.indexes'.
 * Returns the newly allocated variable, or nullptr on failure. */
QSNMPVar * QSNMPModule::snmpCreateVar(const QString & name, QSNMPType_e type, QSNMPMaxAccess_e maxAccess,
                                         const QSNMPOid & groupOid, quint32 fieldId, const QSNMPOid & indexes)
{
    QSNMPVar * var = new QSNMPVar(this, name, type, maxAccess, groupOid, fieldId, indexes);
    if(!mSnmpAgent->registerVar(var))
    {
        delete var;
        return nullptr;
    }
    mSnmpVarList << var;
    return var;
}

/* Unregisters a variable (that was allocated in this module) from the agent, and frees it.
 * Returns true on success. */
bool QSNMPModule::snmpDeleteVar(QSNMPVar * var)
{
    QSNMPVarList::iterator it = mSnmpVarList.begin();
    while(it != mSnmpVarList.end())
    {
        if(*it == var)
        {
            mSnmpVarList.erase(it);
            mSnmpAgent->unregisterVar(var);
            delete var;
            return true;
        }
        else
            it++;
    }
    return false;
}

/* Unregisters all variables (that were allocated in this module) from the agent, and frees them. */
void QSNMPModule::snmpDeleteAllVars()
{
    while(!mSnmpVarList.isEmpty())
    {
        QSNMPVar * var = mSnmpVarList.takeFirst();
        mSnmpAgent->unregisterVar(var);
        delete var;
    }
}



/*******************************************************/
/******************** SNMP VARIABLE ********************/
/*******************************************************/

/* Constructor for an SNMP variable. A variable can be either a scalar or tabular item,
 * based on 'indexes', and corresponds to an OBJECT-TYPE in the MIB syntax.
 * Variables are created via QSNMPModule snmpCreateVar function and
 * should typically not be created directly by the user application.
 * The 'name' argument can be any as desired by the user application, but it is recommended
 * to use the same one as inside the MIB file to ease log parsing.
 * The 'type' and 'maxAccess' arguments determine the data type and R/W permissions.
 * The 'groupOid' is the OID of the parent group, 'fieldId' is the variable's identifier
 * under the parent group, and 'indexes' can either be qsnmpScalarIndex (.0) for scalar
 * variables or a list of values for tabular variables.
 * The complete variable OID is thus 'groupOid.fieldId.indexes'. */
QSNMPVar::QSNMPVar(QSNMPModule * module, const QString & name, QSNMPType_e type, QSNMPMaxAccess_e maxAccess,
                           const QSNMPOid & groupOid, quint32 fieldId, const QSNMPOid & indexes)
{
    /* Constants */
    mModule = module;
    mName = name;
    mType = type;
    mMaxAccess = maxAccess;
    mGroupOid = groupOid;
    mFieldId = fieldId;
    mIndexes = indexes;
    mOid << mGroupOid << mFieldId << mIndexes;
    mOidString = toString(mOid);
    mFullName = QString("%1%2").arg(mName).arg(toString(mIndexes));

    /* Registration (opaque) */
    mRegistration = nullptr;
}

/* Destructor for an SNMP variable. */
QSNMPVar::~QSNMPVar()
{
    /* Nothing to do */
}

/* Returns the parent SNMP module. */
QSNMPModule * QSNMPVar::module() const
{
    return mModule;
}

/* Returns the name of this variable. */
const QString & QSNMPVar::name() const
{
    return mName;
}

/* Returns the data type of this variable. */
QSNMPType_e QSNMPVar::type() const
{
   return mType;
}

/* Returns the maximum access level of this variable. */
QSNMPMaxAccess_e QSNMPVar::maxAccess() const
{
    return mMaxAccess;
}

/* Returns the group OID of this variable, that is the OID of the
 * parent group. */
const QSNMPOid & QSNMPVar::groupOid() const
{
    return mGroupOid;
}

/* Returns the variable's field identifier under the parent group. */
quint32 QSNMPVar::fieldId() const
{
    return mFieldId;
}

/* Returns the indexes of this variable.
 * This will be qsnmpScalarIndex (.0).0 for a scalar variable
 * or a list of values for a tabular variable. */
const QSNMPOid & QSNMPVar::indexes() const
{
    return mIndexes;
}

/* Returns this variable's complete OID (groupOid.fieldId.indexes). */
const QSNMPOid & QSNMPVar::oid() const
{
    return mOid;
}

/* Returns this variable's complete OID (groupOid.fieldId.indexes) in string format. */
const QString & QSNMPVar::oidString() const
{
    return mOidString;
}

/* Returns the full name (name.indexes) for this variable. */
const QString & QSNMPVar::fullName() const
{
    return mFullName;
}

/* Gets the opaque, internal, Net-SNMP registration structure. */
void * QSNMPVar::registration() const
{
    return mRegistration;
}

/* Sets the opaque, internal, Net-SNMP registration structure. */
void QSNMPVar::setRegistration(void * registration)
{
    mRegistration = registration;
}

/* Returns this variable's value, by calling the snmpGetValue handler of the parent module. */
QVariant QSNMPVar::get() const
{
    return mModule->snmpGetValue(this);
}

/* Sets this variable's value, by calling the snmpSetValue handler of the parent module. */
bool QSNMPVar::set(const QVariant & v) const
{
    return mModule->snmpSetValue(this, v);
}
