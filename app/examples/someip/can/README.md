# SOMEIP CAN message publisher and subscriber example

## The configuration for the publisher and subscriber

- [configuration for the publisher](./config/pub/Network.json)
- [configuration for the subscriber](./config/sub/Network.json)

Go through this 2 configuration, it's almost the same as one configures the server for the publisher and the other one configures the client for the subscriber.

And for the configuration json.editor can be utilized.

### Below command to launch the json.editor configuration tool

```sh
cd tools/json.editor
python main.py
```

- Click Menu File -> Load and choose to open "app/examples/someip/can/config/pub/Network.json".
- click and workaround
- Click Menu File -> Save and File -> Generate, can see a "Net.json" and its related configuration generated at "tools/json.editor/config".


## Build and Run on host

- Build
```sh
scons --app=CanSubEx --os=OSAL
scons --app=CanPubEx --os=OSAL
```

- Run
```sh
build\nt\GCC\CanPubEx\CanPubEx.exe
build\nt\GCC\CanSubEx\CanSubEx.exe
```

The CanSubEx will give output as below:
```
CANBUS  :receive: session=10, canid = 0xa, dlc = 8, data = [0A, 0B, 0C, 0D,0E, 0F, 10, 11]
CANBUS  :receive: session=11, canid = 0xb, dlc = 8, data = [0B, 0C, 0D, 0E,0F, 10, 11, 12]
CANBUS  :receive: session=12, canid = 0xc, dlc = 8, data = [0C, 0D, 0E, 0F,10, 11, 12, 13]
```

And the CanSubEx can be start multiple times, and each can get the same message published by CanPubEx.

Using wireshark, open the Adapter for loopback trafic capture, we can see the published CAN message through UDP multicast.



