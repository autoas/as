# OsekNm

Download [OsekNm.Simulator.on.Win10.7z](https://github.com/autoas/ssas-public/releases/download/OSEKNM-0.0.1/OsekNm.Simulator.on.Win10.7z), use 7z to unzip it and just double click the run.bat to let the demo run.

```bash
# File list:
OsekNm.exe # prebuilt demo OsekNm
CanSimulator.exe # CAN bus simulator over IP socket

libwinpthread-1.dll # runtime dependency
run.bat # entry point, double click to run the demo
```

Click on the first cmd "CanSimulator.exe" window to bring it frontend.

Press key "x" to wakeup the network of all OsekNm Node from ID 0 to ID 2, you could see OsekNm message over CAN bus as below, you could see the ring was built up successfully.

```bash
canid=00000502,dlc=08,data=[02,01,00,00,00,00,00,00,] [........] @ 8.292728 s rel 4814.27 ms
canid=00000501,dlc=08,data=[01,01,00,00,00,00,00,00,] [........] @ 8.304721 s rel 11.99 ms
canid=00000500,dlc=08,data=[00,01,00,00,00,00,00,00,] [........] @ 8.326709 s rel 21.99 ms
canid=00000502,dlc=08,data=[00,02,00,00,00,00,00,00,] [........] @ 9.310144 s rel 983.44 ms
canid=00000501,dlc=08,data=[02,02,00,00,00,00,00,00,] [........] @ 9.330132 s rel 19.99 ms
canid=00000500,dlc=08,data=[01,02,00,00,00,00,00,00,] [........] @ 9.349121 s rel 18.99 ms
canid=00000501,dlc=08,data=[02,02,00,00,00,00,00,00,] [........] @ 10.380528 s rel 1031.41 ms
canid=00000502,dlc=08,data=[00,02,00,00,00,00,00,00,] [........] @ 11.438920 s rel 1058.39 ms
canid=00000500,dlc=08,data=[01,02,00,00,00,00,00,00,] [........] @ 12.479322 s rel 1040.40 ms
canid=00000501,dlc=08,data=[02,02,00,00,00,00,00,00,] [........] @ 13.543710 s rel 1064.39 ms
canid=00000502,dlc=08,data=[00,02,00,00,00,00,00,00,] [........] @ 14.608098 s rel 1064.39 ms
canid=00000500,dlc=08,data=[01,02,00,00,00,00,00,00,] [........] @ 15.670487 s rel 1062.39 ms
canid=00000501,dlc=08,data=[02,02,00,00,00,00,00,00,] [........] @ 16.737873 s rel 1067.39 ms
canid=00000502,dlc=08,data=[00,02,00,00,00,00,00,00,] [........] @ 17.824249 s rel 1086.38 ms
canid=00000500,dlc=08,data=[01,02,00,00,00,00,00,00,] [........] @ 18.866650 s rel 1042.40 ms
canid=00000501,dlc=08,data=[02,02,00,00,00,00,00,00,] [........] @ 19.960022 s rel 1093.37 ms
```

Then press key "x" again to release the network of all OsekNm node, could see OsekNm message stopped:

```bash
canid=00000502,dlc=08,data=[00,12,00,00,00,00,00,00,] [........] @ 21.010418 s rel 1050.40 ms
canid=00000500,dlc=08,data=[01,12,00,00,00,00,00,00,] [........] @ 22.098791 s rel 1088.37 ms
canid=00000501,dlc=08,data=[02,12,00,00,00,00,00,00,] [........] @ 23.138193 s rel 1039.40 ms
canid=00000502,dlc=08,data=[00,32,00,00,00,00,00,00,] [.2......] @ 24.201584 s rel 1063.39 ms
```

Note: the key "x" is used to toggle the request/release of OsekNm network mode.

