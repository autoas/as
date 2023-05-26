# AS - Automotive Software

This project is only free to be used for evaluation and study purpose, all of the BSWs are developed by me alone according to AUTOSAR 4.4.

## AUTOSAR and its toolchain

![architecture](doc/images/architecture.png)

## Tools & Libraries

This project now provide below tools & libraries:

 **CanBusSimulator**: CAN bus simulator over IP socket

 **LinBusSimulator**: LIN bus simulator over IP socket

 **CanLib**: CAN lib used to access CAN hardware(Vecotr CanCaseXL, PeakCan, ZLGCAN, etc)

 **LinLib**: LIN lib used to access LIN hardware(COM, etc)

 **DevLib**: abstract device libraries used to access any other kind of devices for automotive

 **IsoTp**: LIN or CAN(CANFD) Transport Layer(ISO15765)

 **Loader**: A library used for bootloader

 **AsOne**: PyQT5 based GUI tool for Com/Dcm/FlashLoader

 **DoIPClient**: DoIP client library to access DoIP server

 **JSON Editor**: JSON schema & PyQT5 based JSON configuation GUI tool for ssas

 ![JSON Editor](doc/images/json-editor-ssas.gif)


## Documents

  **Background**: [CN](doc/CN/background.md)

  **How to setup build environment**: [CN](doc/CN/build-env-setup.md) [EN](doc/EN/build-env-setup.md)

  **Virtuan CAN environment**: [CN](doc/CN/virtual-can-env.md)

  **Virtuan LIN environment**: TBD

  **CAN Bootloader**: [CN](doc/CN/can-bootloader.md)

  **CAN OSEKNM**: [CN](doc/CN/can-oseknm.md)

  **NVM**: [CN](doc/CN/nvm.md)

  **SOMEIP/SD**: [EN](doc/EN/SOMEIP-SD.md)

  **DoIP**: [EN](doc/EN/DoIP.md)

  **Json Editor**: [EN](doc/EN/JsonEditor.md)