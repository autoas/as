# SOMEIP math add request/response examples

## The configuration for the request client and the response server

- [configuration for the client](./config/client/Network.json)
- [configuration for the server](./config/server/Network.json)

Go through this 2 configuration, it's almost the same as one configures the server for the response and the other one configures the client for the request.

And for the configuration json.editor can be utilized.

## Build and Run on host

- Build
```sh
scons --app=AddServerEx --os=OSAL
scons --app=AddClientEx --os=OSAL
```

- Run
```sh
build\nt\GCC\AddServerEx\AddServerEx.exe
build\nt\GCC\AddClientEx\AddClientEx.exe
```

The AddClientEx will give output as below:
```
MATH    :add response session=2: ercd = 0 summary 64448 == 64448
MATH    :add response session=3: ercd = 0 summary 68096 == 68096
MATH    :add response session=4: ercd = 0 summary 71808 == 71808
MATH    :add response session=5: ercd = 0 summary 75584 == 75584
MATH    :add response session=6: ercd = 0 summary 79424 == 79424
MATH    :add response session=7: ercd = 0 summary 83328 == 83328
MATH    :add response session=8: ercd = 0 summary 87296 == 87296
```

Using wireshark, open the Adapter for loopback trafic capture, we can see the add request and response message through TCP.



