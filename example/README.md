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


## Testing the application

👉 Starting Net-SNMP daemons

In order to test the application, we first need to launch the SNMP master agent. This is done by executing the `snmpd` daemon. How to configure `snmpd` is out of scope of this notice, but a [snmpd.conf](net-snmp/snmpd.conf) file is provided in this repository. In this file we configure as AgentX master, set access control, and traps are forwarded locally.
``` bash
cd <qt-snmp-subagent-dir>/example/net-snmp/
sudo snmpd -f -Lo -C -c snmpd.conf
```

If you don't want to type out the complete OIDs, you better copy the [QSNMP-EXAMPLE-MIB.txt](net-snmp/mibs/QSNMP-EXAMPLE-MIB.txt) MIB file into Net-SNMP search path ( usually `/usr/local/share/snmp/mibs/`). This way Net-SNMP applications will automatically convert OID to/from human readable names.

We also want to monitor traps, this can be done by using `snmptrapd` daemon. An example [snmptrapd.conf](net-snmp/snmptrapd.conf) file is also provided that formats the text output on the console. In a new terminal launch the trap-receiver daemon:
``` bash
cd <qt-snmp-subagent-dir>/example/net-snmp/
sudo snmptrapd -f -Lo -m ALL -C -c snmptrapd.conf
```

At this time, you should have two terminals open: one with `snmpd` running, and the other with `snmptrapd`.

👉 Launching the example application

Build the example source code by the method of your choice (e.g. QtCreator build), and you can simply execute the application as root in yet another terminal: `sudo ./example`. The application should print out a connection to the Net-SNMP master agent, variables being registered, and also a `moduleStartupDone` trap being emitted. Those logs (shown below), are output by the `QSNMPAgent::newLog()` signal:
```
NET-SNMP version 5.9 AgentX subagent connected
"Registered SNMP variable moduleUptime.0"
"Registered SNMP variable moduleValueA.0"
"Registered SNMP variable moduleValueB.0"
"Registered SNMP variable moduleValueSum.0"
"SNMP-TRAP: moduleStartupDone"
"Registered SNMP variable myTableEntryIndex1.0.2"
(...)
```

The `snmptrapd` daemon should have caught that notification, with the following lines appearing in its terminal:
```
DISMAN-EVENT-MIB::sysUpTimeInstance = Timeticks: (245682) 0:40:56.82
 SNMPv2-MIB::snmpTrapOID.0 = OID: QSNMP-EXAMPLE-MIB::moduleStartupDone
```

👉 Getting and setting values

Now that the application is running, we can read and write values via Net-SNMP applications `snmpget`, `snmpwalk`, `snmpset`, `snmptable`...

For instance:
``` bash
$ snmpget -v 2c -c public localhost -m ALL moduleUptime.0
QSNMP-EXAMPLE-MIB::moduleUptime.0 = Timeticks: (53179) 0:08:51.79

$ snmpget -v 2c -c public localhost -m ALL moduleValueSum.0
QSNMP-EXAMPLE-MIB::moduleValueSum.0 = INTEGER: 42

$ snmpget -v 2c -c public localhost -m ALL myTableEntryColor.1.3
QSNMP-EXAMPLE-MIB::myTableEntryColor.1.3 = INTEGER: green(1)
```

Or getting the complete `myTable`:
``` bash
$ snmptable -v 2c -c public -m ALL -Cb localhost myTable
SNMP table: QSNMP-EXAMPLE-MIB::myTable

 Index1 Index2 Color
      0      2   red
      1      3 green
      8      6  blue
      9      5 green
     12      0   red

```

Let's set a new value to `moduleValueA.0`:
``` bash
$ snmpset -v 2c -c private -m ALL localhost moduleValueA.0 i 30
QSNMP-EXAMPLE-MIB::moduleValueA.0 = INTEGER: 30
```

The example application updates the new value accordingly, and the code generates a trap indicating that the value sum has changed (with variables bindings). A shown in the application's console:
```
"SNMP-SET: moduleValueA.0 [RW] : INTEGER = 30"
"SNMP-TRAP: moduleValueSumChanged"
"           => moduleValueA.0 [RW] : INTEGER = 30"
"           => moduleValueB.0 [RW] : INTEGER = -100"
"           => moduleValueSum.0 [RO] : INTEGER = -70"

```

The `snmptrapd` daemon catches that notification and displays it all:
``` 
DISMAN-EVENT-MIB::sysUpTimeInstance = Timeticks: (337306) 0:56:13.06
 SNMPv2-MIB::snmpTrapOID.0 = OID: QSNMP-EXAMPLE-MIB::moduleValueSumChanged
 QSNMP-EXAMPLE-MIB::moduleValueA.0 = INTEGER: 30
 QSNMP-EXAMPLE-MIB::moduleValueB.0 = INTEGER: -100
 QSNMP-EXAMPLE-MIB::moduleValueSum.0 = INTEGER: -70
```

If we try to walk the whole MIB:
```
$ snmpwalk -v 2c -c public localhost -m ALL example
QSNMP-EXAMPLE-MIB::moduleUptime.0 = Timeticks: (118739) 0:19:47.39
QSNMP-EXAMPLE-MIB::moduleValueA.0 = INTEGER: 30
QSNMP-EXAMPLE-MIB::moduleValueB.0 = INTEGER: -100
QSNMP-EXAMPLE-MIB::moduleValueSum.0 = INTEGER: -70
QSNMP-EXAMPLE-MIB::myTableEntryIndex1.0.2 = Gauge32: 0
QSNMP-EXAMPLE-MIB::myTableEntryIndex1.1.3 = Gauge32: 1
QSNMP-EXAMPLE-MIB::myTableEntryIndex1.8.6 = Gauge32: 8
QSNMP-EXAMPLE-MIB::myTableEntryIndex1.9.5 = Gauge32: 9
QSNMP-EXAMPLE-MIB::myTableEntryIndex1.12.0 = Gauge32: 12
QSNMP-EXAMPLE-MIB::myTableEntryIndex2.0.2 = Gauge32: 2
QSNMP-EXAMPLE-MIB::myTableEntryIndex2.1.3 = Gauge32: 3
QSNMP-EXAMPLE-MIB::myTableEntryIndex2.8.6 = Gauge32: 6
QSNMP-EXAMPLE-MIB::myTableEntryIndex2.9.5 = Gauge32: 5
QSNMP-EXAMPLE-MIB::myTableEntryIndex2.12.0 = Gauge32: 0
QSNMP-EXAMPLE-MIB::myTableEntryColor.0.2 = INTEGER: red(0)
QSNMP-EXAMPLE-MIB::myTableEntryColor.1.3 = INTEGER: green(1)
QSNMP-EXAMPLE-MIB::myTableEntryColor.8.6 = INTEGER: blue(2)
QSNMP-EXAMPLE-MIB::myTableEntryColor.9.5 = INTEGER: green(1)
QSNMP-EXAMPLE-MIB::myTableEntryColor.12.0 = INTEGER: red(0)
```

And finally, if we exit the example application (`Ctrl+C`), `snmptrapd` should catch the exit message:
```
DISMAN-EVENT-MIB::sysUpTimeInstance = Timeticks: (374855) 1:02:28.55
 SNMPv2-MIB::snmpTrapOID.0 = OID: QSNMP-EXAMPLE-MIB::moduleExiting
 QSNMP-EXAMPLE-MIB::moduleExitMessage.0 = STRING: Goodbye!
```
