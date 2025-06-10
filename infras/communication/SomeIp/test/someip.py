import socket
import time

SOMEIP_MSG_REQUEST = 0
SOMEIP_TP_MAX = 1392
SOMEIP_TP_FLAG = 0x20


class SomeIpClient:
    def __init__(self, ip, port, serviceId, clientId, isUDP=True):
        self.isUDP = isUDP
        self.serviceId = serviceId
        self.clientId = clientId
        self.sessionId = 0
        self.remoteAddr = (ip, port)
        if self.isUDP:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        else:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect(self.remoteAddr)

    def build_header(self, serviceId, methodId, clientId, sessionId,
                     interfaceVersion, messageType, returnCode, payloadLength):
        data = []
        length = payloadLength + 8
        data.append((serviceId >> 8) & 0xFF)
        data.append(serviceId & 0xFF)
        data.append((methodId >> 8) & 0xFF)
        data.append(methodId & 0xFF)
        data.append((length >> 24) & 0xFF)
        data.append((length >> 16) & 0xFF)
        data.append((length >> 8) & 0xFF)
        data.append(length & 0xFF)
        data.append((clientId >> 8) & 0xFF)
        data.append(clientId & 0xFF)
        data.append((sessionId >> 8) & 0xFF)
        data.append(sessionId & 0xFF)
        data.append(1)
        data.append(interfaceVersion)
        data.append(messageType)
        data.append(returnCode)
        return data

    def request_tp(self, methodId, data, interfaceVersion=0):
        offset = 0
        total = len(data)
        while(offset < total):
            length = min(SOMEIP_TP_MAX, total - offset)
            req = self.build_header(self.serviceId, methodId, self.clientId, self.sessionId,
                                    interfaceVersion, SOMEIP_MSG_REQUEST | SOMEIP_TP_FLAG, 0, length + 4)
            if offset + length < total:
                offset_ = offset | 0x01
            else:
                offset_ = offset
            req.append((offset_ >> 24) & 0xFF)
            req.append((offset_ >> 16) & 0xFF)
            req.append((offset_ >> 8) & 0xFF)
            req.append(offset_ & 0xFF)
            req += [x & 0xFF for x in data[offset:offset+length]]
            offset += length
            self.sock.sendto(bytes(req), self.remoteAddr)
            time.sleep(0.01)
        res, remoteAddr = self.sock.recvfrom(4096)
        print(res, remoteAddr)
        self.sessionId += 1

    def request(self, methodId, data, interfaceVersion=0):
        if self.isUDP and len(data) > SOMEIP_TP_MAX:
            return self.request_tp(methodId, data, interfaceVersion)
        req = self.build_header(self.serviceId, methodId, self.clientId, self.sessionId,
                                interfaceVersion, SOMEIP_MSG_REQUEST, 0, len(data))
        req += [x & 0xFF for x in data]
        if self.isUDP:
            self.sock.sendto(bytes(req), self.remoteAddr)
            res, remoteAddr = self.sock.recvfrom(4096)
        else:
            self.sock.send(bytes(req))
            res, remoteAddr = self.scok.recv(4096), self.remoteAddr
        print(res, remoteAddr)
        self.sessionId += 1


if __name__ == '__main__':
    while(True):
        sc = SomeIpClient('192.168.31.177', 30560,
                          serviceId=0x1234, clientId=0x6666, isUDP=True)
        sc.request(0x421, data=range(5000))
        time.sleep(0.2)
