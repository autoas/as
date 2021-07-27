# CanNm

Download [CanNm.Simulator.on.Win10.7z](https://github.com/autoas/ssas-public/releases/download/CanNm-0.0.1/CanNm.Simulator.on.Win10.7z), use 7z to unzip it and just double click the run.bat to let the demo run.

```bash
# File list:
CanNm.exe # prebuilt demo CanNm
CanSimulator.exe # CAN bus simulator over IP socket

libwinpthread-1.dll # runtime dependency
run.bat # entry point, double click to run the demo
```

Click on the first cmd "CanSimulator.exe" window to bring it frontend.

Press key "x" to request network mode of all CanNm Node from ID 0 to ID 4, you could see CanNm message over CAN bus as below:

```bash
can(0) socket driver on-line!
can socket CC on-line!
can socket D0 on-line!
can socket D4 on-line!
can socket D8 on-line!
can socket DC on-line!
canid=00000402,dlc=08,data=[02,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 4.180963 s rel 0.00 ms
canid=00000404,dlc=08,data=[04,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 4.219946 s rel 38.98 ms
canid=00000400,dlc=08,data=[00,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 4.231936 s rel 11.99 ms
canid=00000403,dlc=08,data=[03,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 4.237932 s rel 6.00 ms
canid=00000401,dlc=08,data=[01,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 4.257096 s rel 19.16 ms
canid=00000402,dlc=08,data=[02,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 4.287156 s rel 30.06 ms
canid=00000404,dlc=08,data=[04,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 4.295806 s rel 8.65 ms
canid=00000400,dlc=08,data=[00,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 4.302801 s rel 7.00 ms
canid=00000403,dlc=08,data=[03,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 4.312794 s rel 9.99 ms
canid=00000401,dlc=08,data=[01,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 4.333310 s rel 20.52 ms
```

Then press key "x" again to release network of all CanNm node, could see CanNm message stopped:

```bash
canid=00000400,dlc=08,data=[00,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 152.260422 s rel 562.07 ms
canid=00000401,dlc=08,data=[01,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 152.931290 s rel 670.87 ms
canid=00000400,dlc=08,data=[00,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 153.494019 s rel 562.73 ms
canid=00000401,dlc=08,data=[01,50,FF,FF,55,AA,FF,FF,] [.P..U...] @ 154.149460 s rel 655.44 ms
```

And in each cmd window of "CanNm.exe", message below could be seen:

```bash
INFO    :CanNm NodeId=4, ReduceTime=90
INFO    :application build @ Jul 27 2021 22:37:46
CanNm request
CANNM   :0: confirm PN, flags 1
CANNM   :0: Enter Repeat Message Mode, flags 101
CANNM   :0: Enter Normal Operation Mode, flags 121
CanNm release
CANNM   :0: Enter Ready Sleep Mode, flags 120
CANNM   :0: Enter Prepare Bus Sleep Mode, flags 120
CANNM   :0: Enter Bus Sleep Mode, flags 120
```

Note: the key "x" is used to toggle the request/release of CanNm network mode.

