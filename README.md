# qt-snmp-subagent
A simple Net-SNMP AgentX sub-agent interface for Qt (C++) applications that implement a MIB module.

### :warning: Work In Progress :warning:
- [ ] QSNMP header/source implementation
  - [x] QSNMPAgent and Net-SNMP callback handler
  - [x] QSNMPModule (SNMP module abstract class)
  - [x] QSNMPVar (SNMP variable/OBJECT-TYPE)
  - [ ] Traps/Notifications
  - [ ] Logging: debug, info, error
- [ ] Example application
  - [x] Main function, SNMP agent creation
  - [x] MyModule (simple group) with Read/Write variables
  - [ ] MyModule with Notifications (traps)
  - [ ] MyTableEntry (table group) with indexes and Read/Write variables
  - [ ] Corresponding MIB file
- [ ] This README
  - [ ] Detailed presentation of this project
  - [ ] Net-SNMP install HOWTO
  - [ ] How-to integrate and build QSNMP into the user's application
  - [ ] Code description, HOWTO use QSNMP classes
  - [ ] Detailed description of Example application
    - [ ] Code walkthrough
    - [ ] Configure/Execute Net-SNMP master agent
    - [ ] Net-SNMP commands to check functionality
    - [ ] Trap-viewer
