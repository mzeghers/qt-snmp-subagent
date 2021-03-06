#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <signal.h>
#include "../QSNMP/QSNMP.h"
#include "MyModule.h"
#include "MyTableEntry.h"



/**********************************************************/
/******************** STATIC FUNCTIONS ********************/
/**********************************************************/

/* Signal caught callback: quit application */
static void terminate(int signum)
{
    Q_UNUSED(signum)
    QCoreApplication::quit();
}



/**********************************************************/
/******************** MAIN ENTRY POINT ********************/
/**********************************************************/

int main(int argc, char * argv[])
{
    /* Setup Qt application */
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("Qt-SNMP AgentX sub-agent example application");

    /* Create signal catching */
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = terminate;
    sigaction(SIGHUP, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGSTOP, &action, NULL);

    /* Parse application arguments */
    QCommandLineParser parser;
    parser.setApplicationDescription(QString("%1 -- Tests the QSNMP interface.").arg(QCoreApplication::applicationName()));
    parser.addHelpOption();
    QCommandLineOption agentNameOption(QStringList() << "n", "Sets the SNMP sub-agent name.", "name", "example");
    parser.addOption(agentNameOption);
    QCommandLineOption agentAddrOption(QStringList() << "x", "Sets the SNMP AgentX socket address, see snmpd manual.", "agentxsocket");
    parser.addOption(agentAddrOption);
    parser.process(app);

    /* Initialize SNMP agent */
    QString agentName = parser.value(agentNameOption);
    QString agentAddr = parser.value(agentAddrOption);
    QSNMPAgent * snmpAgent = new QSNMPAgent(agentName, agentAddr);

    /* Handle logs from SNMP agent */
    snmpAgent->connect(snmpAgent, &QSNMPAgent::newLog, [=](QSNMPLogType_e logType, const QString & msg)
    {
        Q_UNUSED(logType)
        qDebug() << msg;
    });

    /* Create my MIB module, which will instantiate SNMP variables */
    MyModule * myModule = new MyModule(snmpAgent);

    /* Create some MyTableEntry to demonstrate how tables work with QSNMP */
    QList<MyTableEntry *> myTable;
    myTable << new MyTableEntry(snmpAgent, 0, 2);
    myTable << new MyTableEntry(snmpAgent, 1, 3, Green);
    myTable << new MyTableEntry(snmpAgent, 8, 6, Blue);
    myTable << new MyTableEntry(snmpAgent, 9, 5, Green);
    myTable << new MyTableEntry(snmpAgent, 12, 0, Red);

    /* Execute application event loop */
    int rc = app.exec();

    /* Delete the MIB modules: this will also automatically unregister all SNMP variables that
     * where instantiated within those */
    qDeleteAll(myTable);
    delete myModule;

    /* Delete SNMP agent */
    delete snmpAgent;

    /* Exit */
    return rc;
}
