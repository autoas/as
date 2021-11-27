# AS - Automotive Software

This project is not open source, all of the BSWs are developed by me alone according to AUTOSAR 4.4. 

## BSWs of AUTOSAR

This project now provide below BSWs:

	LinIf
	LinTp
	PduR (zero cost only)
	CanTp
	Dem
	Dcm

	OsekNm
	CanNm

	Fls
	Fee
	Eep
	Ea
	MemIf (zero cost only)
	NvM

	TcpIP (socket based, LWIP is supported)
	SoAd
	DoIP
	SOMEIP

## Tools & Libraries

This project now provide below tools & libraries:

​	**CanBusSimulator**: CAN bus simulator over IP socket

​	**LinBusSimulator**: LIN bus simulator over IP socket

​	**CanLib**: CAN lib used to access CAN hardware(Vecotr CanCaseXL, PeakCan, ZLGCAN, etc)

​	**LinLib**: LIN lib used to access LIN hardware(COM, etc)

​	**DevLib**: abstract device libraries used to access any other kind of devices for automotive

​	**IsoTp**: LIN or CAN(CANFD) Transport Layer(ISO15765)

​	**Loader**: A library use for bootloader

​	**AsOne**: PyQT5 based GUI tool for Com/Dcm

	**DoIPClient** : DoIP client library to access DoIP server


## prebuilt demo applications and its tools

* [CAN bootloader demo](examples/CAN-BOOTLOADER.md)
* [CanNm](examples/CanNm.md)
* [OsekNm](examples/OsekNm.md)

