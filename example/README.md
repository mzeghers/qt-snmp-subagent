# QSNMP example application

This example Qt application is provided for a better understanding of how to use QSNMP.

What this application does is implement a [dummy MIB module](net-snmp/mibs/QSNMP-EXAMPLE-MIB.txt), for which the subtree can be simplified as follow:

```
.iso(1).org(3).dod(6).internet(1).private(4).enterprises(1)
	.example(12345)
		.myModule(1)
			.moduleUptime(1) : RO TimeTicks
			.moduleValueA(2) : RW Integer32
			.moduleValueB(3) : RW Integer32
			.moduleValueSum(4) : RO Integer32
			.moduleExitMessage(5) : NOTIFY DisplayString
			.moduleStartupDone(10) : NOTIFICATION
			.moduleExiting(11) : NOTIFICATION => moduleExitMessage(5)
			.moduleValueSumChanged(12) : NOTIFICATION => moduleValueA(2)
								  => moduleValueB(3)
								  => moduleValueSum(4)
		.myTable(2) : TABLE
			.myTableEntry(1) : ENTRY [myTableEntryIndex1][myTableEntryIndex2]
				.myTableEntryIndex1(1) : RO Gauge32
				.myTableEntryIndex2(2) : RO Gauge32
				.myTableEntryColor(3) : RW Integer(enum)	
```

## Code walkthrough

This code walkthrough shows how to use all the basic functionalities of QSNMP. It is divided into three parts:
1. The `main()` function, which sets up the SNMP sub-agent, and creates the MIB module sub-trees.
2. The `MyModule` object, which is a simple SNMP mobule using scalar variables, as well as notifications.
3. The `MyTableEntry` object, demonstrating the usage of SNMP tabular variables.

#### The `main()` function

In `main.cpp` file, after initializing the QtCore application, the QSNMP Agent is created using parsed Command Line arguments. Any application will typically need only a single instance of QSNMPAgent. Note that here the `agentAddr` argument obeys the rules of the `snmpd -x` option (see Net-SNMP manual for more information).

``` c++
/* Initialize SNMP agent */
QString agentName = parser.value(agentNameOption);
QString agentAddr = parser.value(agentAddrOption);
QSNMPAgent * snmpAgent = new QSNMPAgent(agentName, agentAddr);
```

Optionally here, we use the Qt slot mechanism with (C++11) lambda function to catch the logs emitted by QSNMP, and simply output them to standard output using `qDebug()`. Note that we could use the enumeration `logType` to filter the messages if desired.

``` c++
snmpAgent->connect(snmpAgent, &QSNMPAgent::newLog, [=](QSNMPLogType_e logType, const QString & msg)
{
    Q_UNUSED(logType)
    qDebug() << msg;
});
```

After that, we instantiate QSNMP Modules which can represent a complete MIB, a MIB subtree (`MyModule`), tabular entries (`MyTableEntry`), or a combination thereof. Here, `MyModule` and `MyTableEntry` (detailled in the following sections) are both classes that inherit from `QSNMPModule`. They will provide SNMP variables creation and handle their actual values.

``` c++
MyModule * myModule = new MyModule(snmpAgent);

QList<MyTableEntry *> myTable;
myTable << new MyTableEntry(snmpAgent, 0, 2);
myTable << new MyTableEntry(snmpAgent, 1, 3, Green);
myTable << new MyTableEntry(snmpAgent, 8, 6, Blue);
myTable << new MyTableEntry(snmpAgent, 9, 5, Green);
myTable << new MyTableEntry(snmpAgent, 12, 0, Red);

```

Finally, we enter the Qt event loop by calling `app.exec()`. During the event loop, the `QSNMPAgent` object will receive messages from the SNMP master agent and call the appropriate data set/get functions from the `QSNMPModule`-derived classes.

#### The `MyModule` object

`MyModule.cpp` codes a derived class of `QSNMPModule`. It simulates a dummy MIB subtree with a few scalar SNMP variables, as well as SNMP notifications.

In the constructor, we store the module's group OID `mMyModuleOid`, that is the OID of the parent object as defined in the MIB file (here *.myModule*).

``` c++
/* MyModule group OID */
mMyModuleOid = QSNMPOid() << 1 << 3 << 6 << 1 << 4 << 1 /* iso.org.dod.internet.private.enterprises */
                                                    << 12345 /* .example */
                                                    << 1; /* .myModule */
```

Still in the constructor, we also create the SNMP variables according to our MIB file (type, max access...). Those variables are automatically registered with `QSNMPAgent`, and by extension with the Net-SNMP master agent. It is worth noting that the `indexes` parameter of the `snmpCreateVar()` method is left to its default value `qsnmpScalarIndex`, which means that the variable is a scalar variable where its OID is ended by `.0`.

``` c++
this->snmpCreateVar("moduleUptime", QSNMPType_TimeTicks, QSNMPMaxAccess_ReadOnly, mMyModuleOid, 1);
this->snmpCreateVar("moduleValueA", QSNMPType_Integer, QSNMPMaxAccess_ReadWrite, mMyModuleOid, 2);
this->snmpCreateVar("moduleValueB", QSNMPType_Integer, QSNMPMaxAccess_ReadWrite, mMyModuleOid, 3);
this->snmpCreateVar("moduleValueSum", QSNMPType_Integer, QSNMPMaxAccess_ReadOnly, mMyModuleOid, 4);
```

Getting and setting the variable's values is then done by implementing the `snmpGetValue()` and `snmpSetValue()` member functions. When the `QSNMPAgent` receives a SNMP GET or SET request from the Net-SNMP master agent, it will call those functions as required with the target SNMP variable as argument. The user application shall then either return or set the variable's value accordingly. The data being passed around as a `QVariant`, the user application must make sure that the value is casted (or implicitly castable) to/from the Qt-type that matches the SNMP-type of the variable (see inline comments for `QSNMPType_e` enumeration). See the code comments for more information.

This class also demonstrates how traps (notifications) can be emitted by the user application. For instance, at the end of the constructor we generate the `moduleStartupDone(10)` notification (which does not have any payload).

``` c++
mSnmpAgent->sendTrap("moduleStartupDone", mMyModuleOid, 10);.
```

In the destructor we can also generate a `moduleExiting(11)` trap, which takes the `moduleExitMessage(5)` variable as payload. Note that the actual value for the payload will still be retrieved by QSNMP by calling `snmpGetValue()` function.

``` c++
QSNMPVar * var = this->snmpCreateVar("moduleExitMessage", QSNMPType_OctetStr, QSNMPMaxAccess_Notify, mMyModuleOid, 5);
mSnmpAgent->sendTrap("moduleExiting", mMyModuleOid, 11, var);
```

Finally, it is possible to generate multi-variables payload for a trap, as demonstrated when either `moduleValueA(2)` or `moduleValueB(3)` changes. Again, the values for those variable will automatically be read via `snmpGetValue()` function.

``` c++
/* The value changed, which means the sum has changed.
 * Send a trap with some variable bindings. */
QSNMPVarList varList;
varList << this->snmpVar("moduleValueA");
varList << this->snmpVar("moduleValueB");
varList << this->snmpVar("moduleValueSum");
mSnmpAgent->sendTrap("moduleValueSumChanged", mMyModuleOid, 12, varList);
```

#### The `MyTableEntry` object

`MyTableEntry.cpp` also codes a derived class of `QSNMPModule`. It shows how a module can represent a SNMP table entry.

In the constructor, we set the module's group OID `myTableEntryOid`, that is the OID of the parent object as defined in the MIB file (here *.myTableEntry*).

``` c++
/* MyTableEntry group OID and indexes (for tabular variables) */
QSNMPOid myTableEntryOid;
myTableEntryOid << 1 << 3 << 6 << 1 << 4 << 1 /* iso.org.dod.internet.private.enterprises */
					<< 12345 /* .example */
					<< 2 /* .myTable */
					<< 1; /* .myTableEntry */
```

Similarly to `MyModule`, we create the SNMP variables according to our MIB file (type, max access...). An important distinction however is that here all SNMP variables under *.myTableEntry* will be tabular variables. This means that the `indexes` parameter of the `snmpCreateVar()` method is set accordingly to the indexing objects declared in the MIB (here `myTableEntryIndex1` and `myTableEntryIndex2`). That way, the variables are declared as tabular where their OID are ended by `.mIndex1.mIndex2` instead of `.0`. Note that having SNMP variables that refer to the indexes is not actually mandatory but good practice.

``` c++
QSNMPOid indexes  = QSNMPOid() << mIndex1 << mIndex2;
mVar1 = this->snmpCreateVar("myTableEntryIndex1", QSNMPType_Gauge, QSNMPMaxAccess_ReadOnly, myTableEntryOid, 1, indexes);
mVar2 = this->snmpCreateVar("myTableEntryIndex2", QSNMPType_Gauge, QSNMPMaxAccess_ReadOnly, myTableEntryOid, 2, indexes);
mVar3 = this->snmpCreateVar("myTableEntryColor", QSNMPType_Integer, QSNMPMaxAccess_ReadWrite, myTableEntryOid, 3, indexes);
```

The code also shows how to use enumerated integer values through the `myTableEntryColor(3)` SNMP variable.

``` c++
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
```




:warning: Work in progress :warning:

  - [ ] Code walkthrough
  - [ ] Configure/Execute Net-SNMP master agent
  - [ ] Net-SNMP commands to check functionality
  - [ ] Trap-viewer
