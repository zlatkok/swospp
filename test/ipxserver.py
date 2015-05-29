#
#   DOSBox IPX server for testing SWOS++ network code.
#

import re
import sys
import struct
import socket


class IPXPacket:
    _format = '>HHBB IIHH IIHH'

    _fields = (
        'checksum', 'length', 'transControl', 'pType', 'dstNetwork', 'dstHost', 'dstPort', 'dstSocket',
        'srcNetwork', 'srcHost', 'srcPort', 'srcSocket', 'data',
    )

    def __init__(self, buffer=None):
        if not buffer:
            buffer = bytes([ 0 ] * self.headerSize())
            dataLen = 0
        else:
            dataLen = len(buffer) - self.headerSize()
            if dataLen < 0:
                raise ValueError('Malformed packet - size too small')

        self.init(*struct.unpack(self._format + str(dataLen) + 's', buffer))

    def init(self, *args):
        assert(len(args) == len(self._fields))
        for field, value in zip(self._fields, args):
            setattr(self, field, value)

    def getBytes(self):
        values = []
        for field in self._fields:
            values.append(getattr(self, field))
        return struct.pack(self._format + str(len(self.data)) + 's', *values)

    @classmethod
    def headerSize(cls):
        if not hasattr(cls, '_headerSize'):
            cls._headerSize = struct.calcsize(cls._format)
        return cls._headerSize

    def __repr__(self):
        ret = '<'
        for field in self._fields:
            ret += field + ': '  + str(getattr(self, field)) + ', '
        return ret[:-2] + '>'

    def setChecksum(self, checksum):
        self.checksum = checksum

    def setLength(self, length):
        self.length = length

    def setTransControl(self, transControl):
        self.transControl = transControl

    def getSourceHost(self):
        return self.srcHost

    def setSourceHost(self, host):
        self.srcHost = host

    def getSourcePort(self):
        return self.srcPort

    def setSourcePort(self, port):
        self.srcPort = port

    def setSourceSocket(self, socket):
        self.srcSocket = socket

    def getDestSocket(self):
        return self.dstSocket

    def setDestSocket(self, socket):
        self.dstSocket = socket

    def getDestHost(self):
        return self.dstHost

    def setDestHost(self, host):
        self.dstHost = host

    def getDestPort(self):
        return self.dstPort

    def setDestPort(self, port):
        self.dstPort = port

    def setDestNetwork(self, network):
        self.dstNetwork = network

    def setSourceNetwork(self, network):
        self.srcNetwork = network


class Connection:
    def __init__(self, host, port):
        self.host = hostToInt(host)
        self.port = port

    def __hash__(self):
        return (self.host << 16) + self.port

    def __eq__(self, other):
        return self.host == other.host and self.port == other.port

    def __repr__(self):
        return '<' +  self._hostToStr(self.host) + ':' + str(self.port) + '>'

    def _hostToStr(self, host):
        if isinstance(host, str):
            return host
        return str((host >> 24) & 0xff) + '.' + str((host >> 16) & 0xff) + '.' + str((host >> 8) & 0xff) + '.' + str(host & 0xff)

    def getHost(self):
        assert(isinstance(self.host, int))
        return self._hostToStr(self.host)

    def getPort(self):
        return self.port


def hostToInt(host):
    if isinstance(host, str):
        match = ipRegex.match(host)
        if match:
            return int(match.group(4)) | (int(match.group(3)) << 8) | (int(match.group(2)) << 16) | (int(match.group(1)) << 24)

    return host

def ackClient(serverSocket, host, port):
    ipxHeader = IPXPacket()
    ipxHeader.setChecksum(0xffff)
    ipxHeader.setLength(ipxHeader.headerSize())
    ipxHeader.setDestNetwork(0)
    ipxHeader.setDestHost(hostToInt(host))
    ipxHeader.setDestPort(port)
    ipxHeader.setDestSocket(0x2)
    ipxHeader.setSourceNetwork(1)
    serverHost, serverPort = serverSocket.getsockname()
    serverHost = hostToInt(serverHost)
    ipxHeader.setSourceHost(hostToInt(serverHost))
    ipxHeader.setSourcePort(serverPort)
    ipxHeader.setSourceSocket(0x2)
    ipxHeader.setTransControl(0)
    serverSocket.sendto(ipxHeader.getBytes(), ( host, port ))


class ConnectionPool:

    def __init__(self):
        self.connections = set()

    def addConnection(self, host, port):
        print('Adding client {}:{}'.format(hostToInt(host), port))
        self.connections.add(Connection(host, port))

    def getConnection(self, host, port):
        c = Connection(hostToInt(host), port)
        return c if c in self.connections else None

    def getConnectionsExcept(self, host, port):
        host = hostToInt(host)
        return { c for c in self.connections if c.host != host or c.port != port }


ipRegex = re.compile('(\d+)\.(\d+)\.(\d+)\.(\d+)')

def sendIPXPacket(ipConnections, serverSocket, message, host, port, ipxHeader):
    if ipxHeader.getDestHost() == 0xffffffff:
        # broadcast
        for connection in ipConnections.getConnectionsExcept(ipxHeader.getSourceHost(), ipxHeader.getSourcePort()):
            serverSocket.sendto(message, ( connection.getHost(), connection.getPort() ))
    else:
        # specific address
        connection = ipConnections.getConnection(ipxHeader.getDestHost(), ipxHeader.getDestPort())
        if connection:
            serverSocket.sendto(message, ( connection.getHost(), connection.getPort() ))


def handlePacket(ipConnections, serverSocket, host, port, message):
    ipxHeader = IPXPacket(message)
    #print('ipxPacket:', ipxHeader)
    if ipxHeader.getDestSocket() == 0x2 and ipxHeader.getDestHost() == 0x0:
        ipConnections.addConnection(host, port)
        ackClient(serverSocket, host, port)
    else:
        sendIPXPacket(ipConnections, serverSocket, message, host, port, ipxHeader)


def getPort():
    port = 213
    if len(sys.argv) > 1:
        port = int(argv[1])
    return port


def serverLoop(serverSocket):
    print('IPX server online.')
    # todo: add prunning of inactive nodes
    ipConnections = ConnectionPool()
    while True:
        message, (host, port) = serverSocket.recvfrom(2048)
        try:
            handlePacket(ipConnections, serverSocket, host, port, message)
        except (struct.error, ValueError):
            pass


def main():
    port = getPort()

    serverSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    serverSocket.bind(('', port))

    serverLoop(serverSocket)


if __name__ == '__main__':
    main()
