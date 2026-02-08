# SOME/IP

## SOME/IP UDP TP Message Request&Reply Flow Diagram

```mermaid
sequenceDiagram
    participant ClientApp as Client Application
    participant ClientSOMEIP as Client SOME/IP Stack
    participant UDP as UDP Network
    participant ServerSOMEIP as Server SOME/IP Stack
    participant ServerApp as Server Application
    participant Main as SomeIp_MainFunction

    %% =======================
    %% CLIENT SIDE: SEND REQUEST
    %% =======================
    
    %% Step 1: Client Application initiates request
    ClientApp->>ClientSOMEIP: SomeIp_Request() - Send request
    ClientSOMEIP->>ClientSOMEIP: SomeIp_RequestOrFire() - Process request
    
    %% Step 2: Check if request needs TP (large message)
    alt Request needs TP (large message)
        ClientSOMEIP->>ClientSOMEIP: Create SomeIp_TxTpMsgType - Track TP request status (offset, length, timer, sessionId)
        ClientSOMEIP->>ClientSOMEIP: SomeIp_SendNextTxTpMsg() - Prepare first TP segment
        ClientSOMEIP->>ClientApp: onTpCopyTxData() - Get segment data from application
        ClientSOMEIP->>UDP: Send SOME/IP UDP TP Request Segment 1
        UDP->>ServerSOMEIP: SomeIp_RxIndication() - Receive request(s)
        ServerSOMEIP->>ServerSOMEIP: SomeIp_ProcessRxTpMsg() - Process TP Request Segment 1
        ServerSOMEIP->>ServerSOMEIP: Create SomeIp_RxTpMsgType - Track TP message status (offset, timer, clientId, sessionId)
        ServerSOMEIP->>ServerApp: onTpCopyRxData() - Copy received TP Request Segment 1
        %% Step 3: Periodic sending of remaining request segments
        loop Until all request segments sent
            Main->>ClientSOMEIP: SomeIp_MainFunction() - Next iteration
            ClientSOMEIP->>ClientSOMEIP: SomeIp_MainClientTxTpMsg() - Process pending TxTpMsg
            ClientSOMEIP->>ClientSOMEIP: SomeIp_SendNextTxTpMsg() - Prepare next segment
            ClientSOMEIP->>ClientApp: onTpCopyTxData() - Get next segment data
            ClientSOMEIP->>UDP: Send SOME/IP UDP TP Request Segment N
            UDP->>ServerSOMEIP: SomeIp_RxIndication() - Receive request(s)
            ServerSOMEIP->>ServerSOMEIP: SomeIp_ProcessRxTpMsg() - Process TP Request Segment N
            ServerSOMEIP->>ServerSOMEIP: Update SomeIp_RxTpMsgType - Track TP message status (offset, timer, clientId, sessionId)
            ServerSOMEIP->>ServerApp: onTpCopyRxData() - Copy received TP Request Segment N
        end
        ServerSOMEIP->>ServerSOMEIP: All segments received, assemble complete message
        ServerSOMEIP->>ServerApp: onRequest() - Forward complete request to application
    else Request is small (single segment)
        ClientSOMEIP->>UDP: Send SOME/IP UDP Request (single segment)
        UDP->>ServerSOMEIP: SomeIp_RxIndication() - Receive request(s)
        ServerSOMEIP->>ServerApp: onRequest() - Forward complete request to application
    end
    
    %% Step 6: Server Application processes request
    ServerApp-->>ServerSOMEIP: Return SOMEIP_E_PENDING - Processing deferred
    ServerSOMEIP->>ServerSOMEIP: Store AsyncReqMsg - Cache request info for later
    
    %% Step 7: Main function processes pending async requests
    Main->>ServerSOMEIP: SomeIp_MainFunction() - Periodic main loop
    ServerSOMEIP->>ServerSOMEIP: SomeIp_MainServer() - Server main processing
    ServerSOMEIP->>ServerSOMEIP: SomeIp_MainServerAsyncRequest() - Check async requests
    ServerSOMEIP->>ServerApp: onAsyncRequest() - Get response from application
    
    %% Step 8: Server Application provides response data
    ServerApp-->>ServerSOMEIP: Return E_OK with response data
    
    %% Step 9: Server sends response back
    ServerSOMEIP->>ServerSOMEIP: SomeIp_ReplyRequest() - Prepare response
    
    alt Response needs TP (large message)
        ServerSOMEIP->>ServerSOMEIP: Create SomeIp_TxTpMsgType - Track TP reply status (offset, length, timer, clientId, sessionId)
        ServerSOMEIP->>ServerSOMEIP: SomeIp_SendNextTxTpMsg() - Prepare first TP segment
        ServerSOMEIP->>ServerApp: onTpCopyTxData() - Get segment data from application
        ServerSOMEIP->>UDP: Send SOME/IP UDP TP Response Segment 1
        UDP->>ClientSOMEIP: SomeIp_RxIndication() - Receive response
        ClientSOMEIP->>ClientSOMEIP: SomeIp_ProcessRxTpMsg() - Process TP Response Segment 1
        ClientSOMEIP->>ClientSOMEIP: Create SomeIp_RxTpMsgType - Track TP message status (offset, timer, clientId, sessionId)
        ClientSOMEIP->>ClientApp: onTpCopyRxData() - Copy received TP Response Segment 1
        %% Step 10: Periodic sending of remaining segments
        loop Until all segments sent
            Main->>ServerSOMEIP: SomeIp_MainFunction() - Next iteration
            ServerSOMEIP->>ServerSOMEIP: SomeIp_MainServerTxTpMsg() - Process pending TxTpMsg
            ServerSOMEIP->>ServerSOMEIP: SomeIp_SendNextTxTpMsg() - Prepare next segment
            ServerSOMEIP->>ServerApp: onTpCopyTxData() - Get next segment data
            ServerSOMEIP->>UDP: Send SOME/IP UDP TP Response Segment N
            ClientSOMEIP->>ClientSOMEIP: SomeIp_ProcessRxTpMsg() - Process TP Response Segment N
            ClientSOMEIP->>ClientSOMEIP: Create SomeIp_RxTpMsgType - Track TP message status (offset, timer, clientId, sessionId)
            ClientSOMEIP->>ClientApp: onTpCopyRxData() - Copy received TP Response Segment N
        end
        ClientSOMEIP->>ClientSOMEIP: All segments received, assemble complete message
        ClientSOMEIP->>ClientApp: onResponse() - Forward complete response to application
    else Response is small (single segment)
        ServerSOMEIP->>UDP: Send SOME/IP UDP Response (single segment)
        UDP->>ClientSOMEIP: SomeIp_RxIndication() - Receive response
        ClientSOMEIP->>ClientApp: onResponse() - Forward complete response to application
    end
```

## Notes

1. **Timing of `SomeIp_RxIndication()`**: 
   - On the server side, `SomeIp_RxIndication()` generally happens soon after the client sends the request(s)
   - On the client side, `SomeIp_RxIndication()` generally happens soon after the server sends the response(s)

2. **TP Message Handling**: 
   - Large messages are split into segments using `SomeIp_TxTpMsgType` to track transmission status
   - Received segments are assembled using `SomeIp_RxTpMsgType` to track reception status
   - The `Main` function periodically processes pending TP messages to ensure all segments are sent/received

3. **Async Request Processing**: 
   - When the server application returns `SOMEIP_E_PENDING`, the request is cached in an `AsyncReqMsg`
   - The `Main` function later calls `onAsyncRequest()` to get the completed response

4. **Callback Interactions**: 
   - `onTpCopyTxData()`: Used to get segment data from the application for transmission
   - `onTpCopyRxData()`: Used to copy/receive segment data from received messages
   - `onRequest()`: Called when a complete request is ready for processing
   - `onAsyncRequest()`: Called to get the response for a pending request
   - `onResponse()`: Called when a complete response is ready for the application

