---
layout: post
title: AUTOSAR Diagnostic Over IP
category: AUTOSAR
comments: true
---

# DoIP Socket Configuration and Example Setup

This document explains the socket types used in a DoIP (Diagnostics over Internet Protocol) server implementation and provides a step-by-step example for testing with sample applications.

---

## 1. DoIP Socket Types

### 1.1 UDP Discovery Socket
- **Purpose**: Handles broadcast communication for vehicle discovery.  
- **Configuration**: By default, this socket is set to broadcast mode to send vehicle announcement messages (e.g., `VehicleAnnouncement` PDU) to announce the Ecu¡¯s presence on the network.  
- **Key Feature**: Broadcast traffic allows other DoIP clients (e.g., diagnostic tools) to detect the server on the local network.

### 1.2 TCP DoIP Server Socket
- **Purpose**: Acts as a server to accept incoming TCP connections from DoIP clients (e.g., diagnostic testers).  
- **Function**: Listens for client connection requests (via the DoIP `Connect` PDU) and manages persistent communication channels for diagnostic services (e.g., vehicle data exchange).  

---

## 2. Example Application Setup

This example demonstrates testing DoIP functionality using two sample applications:  
- **NetApp**: Integrates SOME/IP/SD, DoIP, and CAN stack for end-to-end communication.  
- **DoIPSend**: A standalone DoIP tester to validate server behavior (e.g., discovery, connection, and data exchange).  

---

### 2.1 Building CanApp as edge node

```sh
scons --app=CanApp
```

---

### 2.2 Building NetApp Without LWIP/VirtualBox Adapter
To test CAN-related functionality (bypassing virtual network adapters), rebuild NetApp using the OS Abstraction Layer (OSAL) instead of LWIP:

```sh
# Rebuild NetApp without LWIP/VirtualBox (uses OSAL for networking)
scons --app=NetApp --os=OSAL
```

---

### 2.3 Building the DoIP Tester (DoIPSend)  
Compile the standalone DoIP tester to validate server behavior:  

```sh
# Build the DoIPSend tester application
scons --app=DoIPSend
```

---

### 2.4 Running the Example  
ExEcute the applications in sequence to test DoIP communication:

#### Step 1: Start NetApp (Server)  
Launch the NetApp server, which initializes DoIP, SOME/IP-SD, and CAN stacks:

```sh
# Run NetApp
build\nt\GCC<br>etApp<br>etApp.exe

# run CanApp as edge node
build\nt\GCC\CanApp\CanApp.exe
```

#### Step 2: Start DoIPSend (Tester)  
Use `DoIPSend` to send DoIP requests to the server. Examples include:

- **Basic Connection Test**:
  ```sh
  build\nt\GCC\DoIPSend\DoIPSend.exe -v 1001
  # press key "d" to simulate DoIP online `DoIP_ActivationLineSwitchActive`
  ```

- **Edge Node Connection Test**:  
  ```sh
  build\nt\GCC\DoIPSend\DoIPSend.exe -v 1001 -t caaa
  ```

- **TLS Encrypted Connection Test**:

  For this test, need to ensure `EnableTLS` of [App Network.json](../../app/app/config/Net/Network.json) is `true`, and need to rebuild the NetApp.

  ```sh
  # Specify a custom TLS certificate for sEcure communication
  build\nt\GCC\DoIPSend\DoIPSend.exe -v 1001 -T app\app\config<br>et\Cert\TLS0_CasCerts.pem
  ```


# 3 DoIP Architecture

# 3.1 DoIP_TesterConnectionType

```mermaid
classDiagram
  %% Core connection struct
  class DoIP_TesterConnectionType {
    +DoIP_TesterConnectionContextType *context
    +SoAd_SoConIdType SoConId
    +PduIdType SoAdTxPdu
    +boolean bEnableTLS (optional)
  }

  %% runtime context for a connection
  class DoIP_TesterConnectionContextType {
    +DoIP_ConnectionStateType state
    +DoIP_MessageContextType msg
    +DoIP_RoutineActivationManagerType ramgr // temporary runtime data for routing activation
    +uint32_t RAMask
    +const DoIP_TesterType *TesterRef
    +uint16_t InactivityTimer
    +uint16_t AliveCheckResponseTimer
    +boolean isAlive
  }

  %% message exchange context per connection
  class DoIP_MessageContextType {
    +uint8_t *req                // len = Tester.NumByteDiagAckNack
    +const DoIP_TargetAddressType *TargetAddressRef
    +PduLengthType TpSduLength
    +PduLengthType index
    +DoIP_MessageStateType state
  }

  %% routine activation state manager
  class DoIP_RoutineActivationManagerType {
    +DoIP_RoutineActivationStateType state
    +const DoIP_TesterType *tester
    +uint8_t raid
    +uint8_t OEM[4]
  }

  %% Tester (Source Address + routing refs)
  class DoIP_TesterType {
    +uint16_t NumByteDiagAckNack
    +uint16_t TesterSA
    +const DoIP_RoutingActivationType *const *RoutingActivationRefs
    +uint8_t numOfRoutingActivations
  }

  %% Routing activation description (links to target addresses)
  class DoIP_RoutingActivationType {
    +uint8_t Number
    +uint8_t OEMReqLen
    +uint8_t OEMResLen
    +const DoIP_TargetAddressType *const *TargetAddressRefs
    +uint16_t numOfTargetAddressRefs
    +AuthenticationCallback()
    +ConfirmationCallback()
  }

  %% Target address grouping and its Rx mapping (PduR)
  class DoIP_TargetAddressType {
    +const DoIP_TargetNodeType *targetNodes
    +uint16_t numTargetNodes
    +uint16_t TargetAddress    // logical TA value
    +PduIdType RxPduId         // PduR ID used to receive from tester => this TA
  }

  %% Per-node mapping (how DoIP forwards to a node)
  class DoIP_TargetNodeType {
    +DoIP_TargetNodeContextType *context
    +PduIdType TxPduId         // PduR ID used to send to target Ecu
    +PduIdType doipTxPduId     // DoIP-internal PDU id called by PduR
    +uint16_t TargetAddress
  }

  class DoIP_TargetNodeContextType {
    +PduLengthType TpSduLength
    +PduLengthType index
    +DoIP_MessageStateType state
  }

  %% relationships & multiplicities
  DoIP_TesterConnectionType --> DoIP_TesterConnectionContextType : context
  DoIP_TesterConnectionContextType --> DoIP_MessageContextType : msg
  DoIP_TesterConnectionContextType --> DoIP_RoutineActivationManagerType : ramgr
  DoIP_TesterConnectionContextType --> DoIP_TesterType : TesterRef
  DoIP_RoutineActivationManagerType --> DoIP_TesterType : tester
  DoIP_RoutineActivationManagerType --> DoIP_RoutingActivationType: raid
  DoIP_TesterConnectionContextType -->DoIP_RoutingActivationType: RAMask [1..*]
  DoIP_MessageContextType --> DoIP_TargetAddressType : TargetAddressRef
  DoIP_TesterType --> DoIP_RoutingActivationType : RoutingActivationRefs [1..*]
  DoIP_RoutingActivationType --> DoIP_TargetAddressType : TargetAddressRefs [1..*]
  DoIP_TargetAddressType --> DoIP_TargetNodeType : targetNodes [1..*]
  DoIP_TargetNodeType --> DoIP_TargetNodeContextType : context
```

# 3.2 DoIP UDS Message Rx

```mermaid
flowchart TD
  subgraph SoAd
    Sock["TCP socket connection<br>(One socket)"]
  end

  subgraph DoIP
    Rx["DoIP_SoAdTpCopyRxData(RxPduId,PduInfo)"]
    ConnLookup["map RxPduId -> TesterConnection"]
    Decode["doipDecodeMsg(header)"]
    Resolve["resolve Tester (SA) and find TargetAddressRef (TA)"]
  end

  subgraph PduR
    PduR1["TA 0x1234<br>PduR_DoIPStartOfReception(Rx_1,...)"]
    PduR1_Dest0["PduR Dest[0]"]
    PduR2["TA 0x2345<br>PduR_DoIPStartOfReception(Rx_2,...)"]
    PduR2_Dest0["PduR Dest[0]"]
    PduR2_Dest1["PduR Dest[1]"]
    PduR3["TA 0x3456<br>PduR_DoIPStartOfReception(Rx_3,...)"]
    PduR3_Dest1["PduR Dest[0]"]
  end

  subgraph CanTp ["Multiple CanTp Tx paths (Ecu nodes)"]
    CAN1["CanTp Tx to Ecu1"]
    CAN2["CanTp Tx to Ecu2"]
    CAN3["CanTp Tx to Ecu2"]
  end

  PduR2 --> PduR2_Dest0
  PduR2 --> PduR2_Dest1
  PduR3 --> PduR3_Dest1

  PduR2_Dest0 --> CAN1
  PduR2_Dest1 --> CAN2
  PduR3_Dest1 --> CAN3

  PduR1 --> PduR1_Dest0
  PduR1_Dest0 --> Dcm

  Sock --> Rx
  Rx --> ConnLookup
  ConnLookup --> Decode
  Decode --> Resolve

  Resolve --> TA1["TA 0x1234<br>(Rx_1)"]
  TA2["TA 0x2345<br>(Rx_2)"]
  Resolve --> TA3["TA 0x3456<br>(Rx_3)"]

  TA1 --> PduR1
  TA3 --> PduR3

  Resolve -->|matched TA -> forward UDS| TA2
  TA2 -->|forwarded to PduR| PduR2

  classDef faded fill:#f3f3f3,stroke:#bbb,color:#777
  classDef active fill:#dff0d8,stroke:#3c763d,color:#274e13

  class TA1,TA3 faded
  class PduR1,PduR3 faded

  class TA2 active
  class PduR2 active
```

# 3.2 DoIP UDS Message Reply

```mermaid
flowchart TD
  subgraph CAN_TP_Rx ["Multiple CanTp Rx paths (Ecu nodes)"]
    CAN1["Ecu1 CanTp Rx<br>(PduR Rx Path 1)"]
    CAN2["Ecu2 CanTp Rx<br>(PduR Rx Path 2)"]
    CAN3["Ecu3 CanTp Rx<br>(PduR Rx Path 3)"]
  end

  subgraph PduR["PduR"]
    PduR_DoIP["PduR -> DoIP_TpTransmit(TxPduId,PduInfo)"]
  end

  subgraph DoIP["DoIP"]
    Lookup["Lookup TargetNode by TxPduId<br>(targetNode.doipTxPduId == TxPduId)"]
    TargetNode["TargetNode<br>(TargetAddress, TxPduId, doipTxPduId)"]
    FindTester["Find Tester & Connection<br>(from connection contexts)"]
    BuildMsg["Build DoIP DIAGNOSTIC_MESSAGE<br>(use TargetNode.TargetAddress and Tester SA)"]
    Send["doipTpSendResponse(...)<br>-> SoAd_TpTransmit(SoAdTxPdu,PduInfo)"]
  end

  subgraph SoAd["SoAd / Network"]
    Sock["TCP socket connection<br>(Tester)"]
  end

  %% flows
  CAN1 -->|UDS response| PduR_DoIP
  CAN2 -->|UDS response| PduR_DoIP
  CAN3 -->|UDS response| PduR_DoIP

  PduR_DoIP --> Lookup
  Lookup --> TargetNode
  TargetNode --> FindTester
  FindTester --> BuildMsg
  BuildMsg --> Send
  Send --> Sock

  %% highlights
  class CAN_TP_Rx highlighted
  classDef highlighted fill:#ffe5e5,stroke:#d9534f,color:#a94442;
  classDef normal fill:#f8f9fa,stroke:#333;
```