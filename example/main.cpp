#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <signal.h>
#include "../QSNMP/QSNMP.h"
#include "MyModule.h"



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

    /* Create my MIB module, which will instantiate SNMP variables */
    MyModule * myModule = new MyModule(snmpAgent);

    /* Execute application event loop */
    int rc = app.exec();

    /* Delete my MIB module: this will also automatically unregister SNMP variables that
     * where instantiated within myModule */
    delete myModule;

    /* Delete SNMP agent */
    delete snmpAgent;

    /* Exit */
    return rc;
}
