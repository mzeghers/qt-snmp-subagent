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
    qRegisterMetaType<QSNMPType_e>("SNMPType_e");
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
    qRegisterMetaType<QSNMPMaxAccess_e>("SNMPMaxAccess_e");
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

/* The main variable GET/SET callback handler, called by the Net-SNMP library on GET/SET messages.
 * Note that here we use the same callback for all variables, so that implementation-specific
 * behavior is determined outside of the QSNMP context. */
static int variableHandler(netsnmp_mib_handler * handler, netsnmp_handler_registration * reginfo,
                            netsnmp_agent_request_info * reqinfo, netsnmp_request_info * requests)
{
    Q_UNUSED(handler)
    netsnmp_variable_list * netsnmp_varlist = requests->requestvb;

    /* Retrieve context data */
    QSNMPAgent * agent = static_cast<QSNMPAgent*>(reginfo->my_reg_void);
    if(!agent)
        return SNMP_ERR_GENERR;

    /* Action */
    if((reqinfo->mode == MODE_GET) || (reqinfo->mode == MODE_GETNEXT))
    {
        /* Multiple variables supported for GET requests */
        while(netsnmp_varlist)
        {
            /* Get the corresponding variable from agent */
            QSNMPOid qtOid = convertOidSnmpToQt(netsnmp_varlist->name, netsnmp_varlist->name_length);
            QSNMPVar * var = agent->varMap().value(toString(qtOid), NULL);
            if(!var)
                return SNMP_ERR_GENERR;

            /* Check access, should not be needed because the Net-SNMP library will do it for us, but still... */
            if(var->maxAccess() < QSNMPMaxAccess_ReadOnly)
                return SNMP_ERR_GENERR;

            /* Read value from user application */
            QVariant v = var->get();

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
        QSNMPVar * var = agent->varMap().value(toString(qtOid), NULL);
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
            if(!var->set(v))
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
        }
        else
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_GENERR);
    }

    /* Done */
    return SNMP_ERR_NOERROR;
}



/****************************************************/
/******************** SNMP AGENT ********************/
/****************************************************/

/* SNMP agent constructor, initialize Net-SNMP library as AgentX sub-agent */
QSNMPAgent::QSNMPAgent(const QString & agentName, const QString & agentAddr)
{
    /* Initialize member variables */
    mAgentName = agentName;
    mVarMap.clear();
    mTrapsEnabled = true;
    mTimer.start();

    /* Initialize agentX/SNMP libraries */
    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 1);
    if(!mAgentName.isEmpty())
        netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_X_SOCKET, agentAddr.toStdString().c_str());
    init_agent(mAgentName.toStdString().c_str());
    init_snmp(mAgentName.toStdString().c_str());

    /* Initial event processing start */
    QTimer::singleShot(0, this, SLOT(processEvents()));
}

/* SNMP agent destructor, shutdown Net-SNMP library */
QSNMPAgent::~QSNMPAgent()
{
    snmp_shutdown(mAgentName.toStdString().c_str());
    shutdown_agent();
}

/* Returns the list of SNMP variables managed under this agent */
const QSNMPVarMap & QSNMPAgent::varMap() const
{
    return mVarMap;
}

/* Register a SNMP variable to this agent.
 * Returns an empty string on success, else a non-empty string (indicating the
 * failure reason) on error.
 * This function is called by the QSNMPObject variable registration helper functions and
 * should typically not be called directly by the user. */
QString QSNMPAgent::registerVar(QSNMPVar * var)
{
    /* Notify variables are not actually registered within the agent */
    if(var->maxAccess() == QSNMPMaxAccess_Notify)
        return QString();

    /* Check if already registered */
    if(mVarMap.contains(var->oidString()))
        return QString("Variable is already registered");

    /* Create handler registration */
    var->setRegistration(NULL);
    QVector<oid> oidVector;
    foreach(quint32 k, var->oid())
        oidVector << k;
    netsnmp_handler_registration * reginfo = netsnmp_create_handler_registration(var->name().toStdString().c_str(),
                                                                                 variableHandler,
                                                                                 (const oid *)oidVector.constData(),
                                                                                 oidVector.size(),
                                                                                 (var->maxAccess()==QSNMPMaxAccess_ReadWrite)?HANDLER_CAN_RWRITE:HANDLER_CAN_RONLY);
    if(!reginfo)
        return QString("Failed to create handler registration");

    /* Our context data */
    reginfo->my_reg_void = this;

    /* Register instance */
    int rc = netsnmp_register_instance(reginfo);
    if(rc == MIB_REGISTRATION_FAILED)
    {
        netsnmp_handler_registration_free(reginfo);
        return QString("Handler registration failed");
    }
    else if(rc == MIB_DUPLICATE_REGISTRATION)
    {
        netsnmp_handler_registration_free(reginfo);
        return QString("Duplicate registration aborted");
    }

    /* Add to list */
    var->setRegistration(reginfo);
    mVarMap.insert(var->oidString(), var);
    return QString();
}

/* Unregister a SNMP variable from this agent.
 * This function is called by the SNMPObject variable registration helper functions and
 * should typically not be called directly by the user. */
void QSNMPAgent::unregisterVar(QSNMPVar * var)
{
    /* Notify variables are not actually registered within the agent */
    if(var->maxAccess() == QSNMPMaxAccess_Notify)
        return;

    /* Unregister */
    if(mVarMap.contains(var->oidString()))
    {
        /* Unregister variable and release memory */
        netsnmp_unregister_handler((netsnmp_handler_registration *)var->registration());
        var->setRegistration(NULL);
        mVarMap.remove(var->oidString());
    }
}

/* Returns true if SNMP traps are enabled */
bool QSNMPAgent::trapsEnabled() const
{
    return mTrapsEnabled;
}

/* Sets SNMP traps enablement. */
void QSNMPAgent::setTrapsEnabled(bool enabled)
{
    mTrapsEnabled = enabled;
}

/* Process events received by the Net-SNMP library regularly */
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
/******************** SNMP OBJECT ********************/
/*****************************************************/

/* Constructor for an SNMP object. Not an actual scalar/tabular item,
 * but a collection of variables, or group, in the MIB syntax.
 * The 'snmpName' argument can be any as desired by the user application, it is not
 * related to the MIB. */
QSNMPObject::QSNMPObject(QSNMPAgent * snmpAgent, const QString & snmpName)
{
    mSnmpAgent = snmpAgent;
    mSnmpName = snmpName;
    mSnmpVarList.clear();
}

/* Destructor for an SNMP object. Unregisters and frees all associated SNMP variables. */
QSNMPObject::~QSNMPObject()
{
    this->snmpRemoveAllVars();
}

/* Returns the parent SNMP agent */
QSNMPAgent * QSNMPObject::snmpAgent() const
{
    return mSnmpAgent;
}

/* Returns the name of this object */
const QString & QSNMPObject::snmpName() const
{
    return mSnmpName;
}

/* Returns the list of SNMP variables allocated under this object */
const QSNMPVarList & QSNMPObject::snmpVarList() const
{
    return mSnmpVarList;
}

/* Returns the SNMP variable (allocated under this object) with given name */
QSNMPVar * QSNMPObject::snmpVar(const QString & name) const
{
    foreach(QSNMPVar * var, mSnmpVarList)
    {
        if(var->name() == name)
            return var;
    }
    return NULL;
}

/* Returns the SNMP variable (allocated under this object) with given field identifier */
QSNMPVar * QSNMPObject::snmpVar(quint32 fieldId) const
{
    foreach(QSNMPVar * var, mSnmpVarList)
    {
        if(var->fieldId() == fieldId)
            return var;
    }
    return NULL;
}

/* Allocates a "Notification" variable under this object and registers it with the agent.
 * The 'name' argument can be any as desired by the user application, but it is recommended
 * to use the same one as inside the MIB file to ease log parsing.
 * The 'baseoid' is the OID of the parent group, 'fieldId' is the variable's identifier
 * under the parent group, and 'indexes' can either be scalarOidIndex (.0) for scalar
 * variables or a list of values for tabular variables.
 * The complete variable OID is thus 'baseOid.fieldId.indexes'.
 * Returns the newly allocated variable. */
QSNMPVar * QSNMPObject::snmpAddNotifyVar(const QString & name, QSNMPType_e type,
                                         const QSNMPOid & baseOid, quint32 fieldId, const QSNMPOid & indexes)
{
    QSNMPVar * var = new QSNMPVar(this, name, type, QSNMPMaxAccess_Notify, baseOid, fieldId, indexes);
    QString errStr = mSnmpAgent->registerVar(var);
    if(!errStr.isEmpty())
    {
        delete var;
        return NULL;
    }
    mSnmpVarList << var;
    return var;
}

/* Allocates a "Read-only" variable under this object and registers it with the agent.
 * The 'name' argument can be any as desired by the user application, but it is recommended
 * to use the same one as inside the MIB file to ease log parsing.
 * The 'baseoid' is the OID of the parent group, 'fieldId' is the variable's identifier
 * under the parent group, and 'indexes' can either be scalarOidIndex (.0) for scalar
 * variables or a list of values for tabular variables.
 * The complete variable OID is thus 'baseOid.fieldId.indexes'.
 * Returns the newly allocated variable. */
QSNMPVar * QSNMPObject::snmpAddReadOnlyVar(const QString & name, QSNMPType_e type,
                                           const QSNMPOid & baseOid, quint32 fieldId, const QSNMPOid & indexes)
{
    QSNMPVar * var = new QSNMPVar(this, name, type, QSNMPMaxAccess_ReadOnly, baseOid, fieldId, indexes);
    QString errStr = mSnmpAgent->registerVar(var);
    if(!errStr.isEmpty())
    {
        delete var;
        return NULL;
    }
    mSnmpVarList << var;
    return var;
}

/* Allocates a "Read-write" variable under this object and registers it with the agent.
 * The 'name' argument can be any as desired by the user application, but it is recommended
 * to use the same one as inside the MIB file to ease log parsing.
 * The 'baseoid' is the OID of the parent group, 'fieldId' is the variable's identifier
 * under the parent group, and 'indexes' can either be scalarOidIndex (.0) for scalar
 * variables or a list of values for tabular variables.
 * The complete variable OID is thus 'baseOid.fieldId.indexes'.
 * Returns the newly allocated variable. */
QSNMPVar * QSNMPObject::snmpAddReadWriteVar(const QString & name, QSNMPType_e type,
                                            const QSNMPOid & baseOid, quint32 fieldId, const QSNMPOid & indexes)
{
    QSNMPVar * var = new QSNMPVar(this, name, type, QSNMPMaxAccess_ReadWrite, baseOid, fieldId, indexes);
    QString errStr = mSnmpAgent->registerVar(var);
    if(!errStr.isEmpty())
    {
        delete var;
        return NULL;
    }
    mSnmpVarList << var;
    return var;
}

/* Unregisters a variable (allocated under this object) from the agent, and frees it. */
void QSNMPObject::snmpRemoveVar(QSNMPVar * var)
{
    QSNMPVarList::iterator it = mSnmpVarList.begin();
    while(it != mSnmpVarList.end())
    {
        QSNMPVar * var2 = *it;
        if(var2 == var)
        {
            it = mSnmpVarList.erase(it);
            mSnmpAgent->unregisterVar(var2);
            delete var2;
        }
        else
            it++;
    }
}

/* Unregisters a variable (allocated under this object) from the agent, and frees it. */
void QSNMPObject::snmpRemoveVar(const QString & name)
{
    QSNMPVarList::iterator it = mSnmpVarList.begin();
    while(it != mSnmpVarList.end())
    {
        QSNMPVar * var = *it;
        if(var->name() == name)
        {
            it = mSnmpVarList.erase(it);
            mSnmpAgent->unregisterVar(var);
            delete var;
        }
        else
            it++;
    }
}

/* Unregisters a variable (allocated under this object) from the agent, and frees it. */
void QSNMPObject::snmpRemoveVar(quint32 fieldId)
{
    QSNMPVarList::iterator it = mSnmpVarList.begin();
    while(it != mSnmpVarList.end())
    {
        QSNMPVar * var = *it;
        if(var->fieldId() == fieldId)
        {
            it = mSnmpVarList.erase(it);
            mSnmpAgent->unregisterVar(var);
            delete var;
        }
        else
            it++;
    }
}

/* Unregisters all variables (allocated under this object)from the agent, and frees them */
void QSNMPObject::snmpRemoveAllVars()
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
 * Variables are created via QSNMPObject variable registration helper functions and
 * should typically not be created directly by the user.
 * The 'name' argument can be any as desired by the user application, but it is recommended
 * to use the same one as inside the MIB file to ease log parsing.
 * The 'baseoid' is the OID of the parent group, 'fieldId' is the variable's identifier
 * under the parent group, and 'indexes' can either be scalarOidIndex (.0) for scalar
 * variables or a list of values for tabular variables.
 * The complete variable OID is thus 'baseOid.fieldId.indexes'. */
QSNMPVar::QSNMPVar(QSNMPObject * object, const QString & name, QSNMPType_e type, QSNMPMaxAccess_e maxAccess,
                           const QSNMPOid & baseOid, quint32 fieldId, const QSNMPOid & indexes)
{
    /* Constants */
    mObject = object;
    mName = name;
    mType = type;
    mMaxAccess = maxAccess;
    mBaseOid = baseOid;
    mFieldId = fieldId;
    mIndexes = indexes;
    mOid << mBaseOid << mFieldId << mIndexes;
    mOidString = toString(mOid);

    /* Descriptive string */
    mDescrString = QString("%1%2 [%3] : %4").arg(mName)
                                            .arg(toString(mIndexes))
                                            .arg(toString(mMaxAccess))
                                            .arg(toString(mType));

    /* Registration (opaque) */
    mRegistration = NULL;
}

/* Returns the parent SNMP object. */
QSNMPObject * QSNMPVar::object() const
{
    return mObject;
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

/* Returns the base OID of this variable, that is the OID of the
 * parent group. */
const QSNMPOid & QSNMPVar::baseOid() const
{
    return mBaseOid;
}

/* Returns the variable's field identifier under the parent group. */
quint32 QSNMPVar::fieldId() const
{
    return mFieldId;
}

/* Returns the indexes of this variable. This will be .0 for scalars
 * or a list of values for tabular variables. */
const QSNMPOid & QSNMPVar::indexes() const
{
    return mIndexes;
}

/* Returns this variable's complete OID (baseOid.fieldId.indexes). */
const QSNMPOid & QSNMPVar::oid() const
{
    return mOid;
}

/* Returns this variable's complete OID (baseOid.fieldId.indexes) in string format. */
const QString & QSNMPVar::oidString() const
{
    return mOidString;
}

/* Returns a descriptive string for this variable. */
const QString & QSNMPVar::descrString() const
{
    return mDescrString;
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

/* Returns this variable's value, by calling the snmpGet handler of the parent object. */
QVariant QSNMPVar::get() const
{
    if(mObject)
        return mObject->snmpGet(this);
    return QVariant();
}

/* Sets this variable's value, by calling the snmpSet handler of the parent object. */
bool QSNMPVar::set(const QVariant & v) const
{
    if(mObject)
        return mObject->snmpSet(this, v);
    return false;
}
