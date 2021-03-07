# qt-snmp-subagent
A simple Net-SNMP AgentX sub-agent interface for Qt (C++) applications that implement a MIB module.

### Installing the Net-SNMP software suite

The qt-snmp-subagent interface (QSNMP) uses the Net-SNMP agent library API. In this regards, the Net-SNMP software suite needs to be installed on the host computer.
This section provides a short description on how to build and install the Net-SNMP suite from source code. The source code can be downloaded from the [Net-SNMP website](http://www.net-snmp.org/). I recommend downloading the latest version (`net-snmp-5.9.tar.gz` at the time of writing this notice), then extract the source code using the following terminal commands.
``` bash
tar -xzvf net-snmp-5.9.tar.gz
cd net-snmp-5.9
```

Then follow the instructions from the README and INSTALL files located in that folder. The installation instructions are also available on the Net-SNMP website [here](http://www.net-snmp.org/docs/INSTALL.html). A minimal install only needs those few commands to be executed:
``` bash
./configure
make
sudo make install
```

At this point, most SNMP applications (`snmpd`, `snmptrapd`, `snmpget`, `snmpset`, ...) and libraries (`libnetsmp`, `libnetsnmpagent`) should be installed on the system. Note that it may be necessary to execute the command `sudo ldconfig` for the linker to reference the newly installed Net-SNMP libraries.



### :warning: Work In Progress :warning:
- [x] QSNMP header/source implementation
  - [x] QSNMPAgent and Net-SNMP callback handler
  - [x] QSNMPModule (SNMP module abstract class)
  - [x] QSNMPVar (SNMP variable/OBJECT-TYPE)
  - [x] Traps/Notifications
  - [x] Logging: debug, info, error
- [x] Example application
  - [x] Main function, SNMP agent creation
  - [x] MyModule (simple group) with Read/Write variables
  - [x] MyModule with Notifications (traps)
  - [x] MyTableEntry (table group) with indexes and Read/Write variables
  - [x] Corresponding MIB file
- [ ] This README
  - [ ] Detailed presentation of this project
  - [x] Net-SNMP install HOWTO
  - [ ] How-to integrate and build QSNMP into the user's application
  - [ ] Code description, HOWTO use QSNMP classes
  - [ ] Detailed description of Example application
    - [ ] Code walkthrough
    - [ ] Configure/Execute Net-SNMP master agent
    - [ ] Net-SNMP commands to check functionality
    - [ ] Trap-viewer
