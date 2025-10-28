---
layout: post
title: AUTOSAR SOMEIP and SD
category: AUTOSAR
comments: true
---


# AUTOSAR SOMEIP/SD Overview

**SOME/IP** (Scalable service-Oriented MiddlewarE over IP) is an AUTOSAR standard for service-oriented communication in automotive systems, enabling dynamic discovery and invocation of services over IP networks. **SOME/IP-SD** (Service Discovery) extends SOMEIP to manage service registration, subscription, and discovery between ECUs. This document guides you through setting up, testing, and verifying SOMEIP/SD communication using example applications.

---

## 1. Key Concepts

### 1.1 SOMEIP/SD Fundamentals  
- **Service-Oriented Communication**: Services are advertised (published) by servers and discovered (subscribed to) by clients dynamically.  
- **Dynamic Addressing**: Uses IP multicast for service discovery, avoiding static IP dependencies.  
- **Middleware Role**: SOMEIP/SD runs on top of TCP/IP, enabling cross-ECU communication over Ethernet.  

### 1.2 Example Applications  
The repository includes two primary examples to explore SOMEIP/SD:  
- **NetApp**: Integrates SOMEIP/SD, DoIP, and CAN stacks for end-to-end automotive communication (built with FreeRTOS + LWIP).  
- **NetAppT**: A lightweight SOMEIP/SD-only test app (built with OSAL) for isolated protocol validation.  

---

## 2. Environment Setup and Build Instructions

### 2.1 Prerequisites  
- **VirtualBox**: Installed with a stable network adapter (e.g., "Oracle VM VirtualBox Network Adapter").  
- **WSL (Windows Subsystem for Linux)**: For building vsomeip (optional, but recommended for validation).  
- **LWIP/FreeRTOS**: Preconfigured in the project for NetApp.  

### 2.2 Build NetApp (Full Stack Example)  
NetApp combines SOMEIP/SD, DoIP, and CAN stacks. Follow these steps:  

```sh
# Configure VirtualBox adapter (index 0) with static IPv4: 172.18.0.1
# (Adjust adapter index if yours differs; check via "ipconfig" in Command Prompt)

# Set environment variables for LWIP and PCAP dumping
set PACKET_LIB_ADAPTER_NR=0   # Use VirtualBox adapter
set USE_PCAP=YES              # Enable Wireshark logging

# Build NetApp with FreeRTOS + LWIP
scons --app=NetApp --net=LWIP --os=FreeRTOS
```

Please note that for the NetApp + FreeRTOS + LWIP setup, it uses [Windows PCAP](https://www.winpcap.org/) to simulate a virtual network adapter for easy study/development. And I find that the [VirtualBox](https://www.virtualbox.org/) network adapter is the most stable one that works perfectly. So you need to install VirtualBox and then manually configure its IPv4 address to "172.18.0.1", as shown in the image below:  

![vbox-ip-config](../images/someip-vbox-net-adapter-ip-config.png)

### 2.3 Build NetAppT (SOMEIP/SD-Only Test)  
NetAppT tests core SOMEIP/SD functionality without additional stacks:  

```sh
# Build NetAppT with OSAL (operates on host Windows sockets)
scons --app=NetAppT --os=OSAL
```

---

## 3. Running the Example Applications

### 3.1 Launch NetApp and NetAppT  
Start both applications in separate terminals to simulate a client-server interaction:  

```sh
# Launch NetApp (server/client with SOMEIP/SD, DoIP, CAN)
build\nt\GCC\NetApp\NetApp.exe

# Launch NetAppT (SOMEIP/SD-only test client)
build\nt\GCC\NetAppT\NetAppT.exe
```

### 3.2 Key Observations in Logs  
Upon launching, NetApp outputs logs showing:  
- LWIP initialization with the VirtualBox adapter IP (`172.18.0.200`).  
- SOMEIP/SD service registration and discovery events.  

Example log snippet:  
```
INFO    :application build @ May  3 2022 21:06:24
... ...
TCPIPI  :Starting lwIP, IP 172.18.0.200
 0: NPF_{7C69ADB8-A49E-46AA-AA62-5E367B965EE9}
     Desc: "Oracle"
 1: NPF_{D8E510AC-9490-42A0-AA88-3BE0BD41E52D}
     Desc: "TAP-Windows Adapter V9"
 2: NPF_{8FAE2F35-41DC-4D5D-A2FC-8426852265FF}
     Desc: "Microsoft"
 3: NPF_{8F9E1CE9-4EA7-4354-844F-848EA56AF7EE}
     Desc: "Microsoft"
 4: NPF_{434E0818-480A-450B-A7F1-99F473224151}
     Desc: "Microsoft"
Using adapter_num: 0
Using adapter: "Oracle"
```

### 3.3 Controlling Service Online/Offline  
Press the `s` key in NetApp to toggle SOMEIP/SD services:  
- **First press**: Brings services online (triggers service discovery).  
- **Second press**: Takes services offline (terminates subscriptions).  

### 3.4 Analyzing Network Traffic  
Check `net.log` (text logs) and `wireshark.pcap` (packet capture) in the project root for:  
- SOMEIP/SD multicast messages (e.g., `SSDP/NOTIFY`).  
- TCP/UDP packets for service data exchange.  

![Wireshark Capture](../images/someip-netapp-netappt-pcap.png)

---

## 4. Verifying Communication with vsomeip

To validate cross-platform compatibility, use **vsomeip** (a reference SOMEIP implementation) on Linux.  

### 4.1 Build vsomeip on WSL Ubuntu 20.04  
```sh
# Clone the repository
git clone https://gitee.com/autoas/vsomeip.git
cd vsomeip

# Build the project
mkdir build && cd build
cmake ..
make -j4
```

### 4.2 Run the SSAS Client Example  
Link the vsomeip configuration and start the client:  

```sh
# Set library path and link config file
export LD_LIBRARY_PATH=`pwd`:$LD_LIBRARY_PATH
ln -fs ../examples/ssas/vsomeip.json vsomeip.json

# Start the client example
./examples/ssas/client-example
```

### 4.3 Expected Outcome  
The client should discover and communicate with the SOMEIP/SD server (NetApp), confirming end-to-end functionality.  

---

## 5. Troubleshooting Common Issues

### 5.1 VirtualBox Adapter Not Detected  
- Ensure VirtualBox is installed and the "Oracle VM VirtualBox Network Adapter" is enabled.  
- Manually set its IPv4 address to `172.18.0.1` (via "Settings ¡ú Network & Internet").  

### 5.2 LWIP Fails to Initialize  
- Check `PACKET_LIB_ADAPTER_NR` matches the correct VirtualBox adapter index (use `ipconfig` to list adapters).  
- Disable firewall/antivirus temporarily to avoid blocking LWIP traffic.  

### 5.3 vsomeip Client Fails to Connect  
- Verify `vsomeip.json` is correctly linked and contains valid IP/port configurations.  
- Ensure NetApp is running and services are online (press `s` in NetApp).  

---
