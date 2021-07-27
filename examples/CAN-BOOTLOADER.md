# CAN BOOTLOADER

Download [CAN.Bootloader.Simulator.on.Win10.7z](https://github.com/autoas/ssas-public/releases/download/CanBL-0.0.1/CAN.Bootloader.Simulator.on.Win10.7z), use 7z to unzip it and just double click the run.bat to let the demo run.

```bash
# File list:
App.s19.sign # prebuilt demo Application s19 file and it was signed
FlashDriver.s19.sign # prebuild demo flash driver s19 file and it was signed
CanBL.exe # prebuilt demo bootloader
CanSimulator.exe # CAN bus simulator over IP socket

Loader.exe # tool Loader act as UDS client to do software update, this tool can also be used to sign a s19 file.
libwinpthread-1.dll # runtime dependency
run.bat # entry point, double click to run the demo
```

After run, a tmp folder will be generated to store the related logs

```bash
Flash.img # Application Flash simulation
Fls.img   # NvM/Fee Flash simulation
can0.log  # CAN message over the simulated CAN bus 0
loader0.log # loader UDS message log
ssas.log # also a copy of loader UDS message log
```

```
# can0.log
can(0) socket driver on-line!
can socket D8 on-line!
can socket 94 on-line!
canid=00000731,dlc=08,data=[02,10,03,55,55,55,55,55,] [...UUUUU] @ 1.484271 s
canid=00000732,dlc=08,data=[06,50,03,13,88,00,32,55,] [.P....2U] @ 1.499895 s
canid=00000731,dlc=08,data=[02,27,01,55,55,55,55,55,] [.'.UUUUU] @ 1.562390 s
canid=00000732,dlc=08,data=[06,67,01,29,CF,A3,4B,55,] [.g.)..KU] @ 1.578014 s
canid=00000731,dlc=08,data=[06,27,02,51,5C,E5,38,55,] [.'.Q\.8U] @ 1.640509 s
canid=00000732,dlc=08,data=[02,67,02,55,55,55,55,55,] [.g.UUUUU] @ 1.656133 s
canid=00000731,dlc=08,data=[02,10,02,55,55,55,55,55,] [...UUUUU] @ 1.703005 s
canid=00000732,dlc=08,data=[06,50,02,13,88,00,32,55,] [.P....2U] @ 1.718629 s
canid=00000731,dlc=08,data=[02,27,03,55,55,55,55,55,] [.'.UUUUU] @ 1.765500 s
canid=00000732,dlc=08,data=[06,67,03,22,5A,54,16,55,] [.g."ZT.U] @ 1.781125 s
canid=00000731,dlc=08,data=[06,27,04,B6,02,33,84,55,] [.'...3.U] @ 1.843620 s
canid=00000732,dlc=08,data=[02,67,04,55,55,55,55,55,] [.g.UUUUU] @ 1.859245 s
canid=00000731,dlc=16,data=[00,0B,34,00,44,20,00,00,00,00,00,07,B0,55,55,55,] [..4.D .......UUU] @ 1.921739 s
canid=00000732,dlc=08,data=[04,74,20,02,02,55,55,55,] [.t ..UUU] @ 1.937364 s
```

```
# loader0.log
loader started:
enter extended session
 request service 10:
  TX: len=2 10 03
  RX: len=6 50 03 13 88 00 32
  PASS
 okay
level 1 security access
 request service 27:
  TX: len=2 27 01
  RX: len=6 67 01 29 CF A3 4B
  PASS

 request service 27:
  TX: len=6 27 02 51 5C E5 38
  RX: len=2 67 02
  PASS
 okay
enter program session
 request service 10:
  TX: len=2 10 02
  RX: len=6 50 02 13 88 00 32
  PASS
 okay
```

