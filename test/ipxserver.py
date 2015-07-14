""" DOSBox IPX server for testing SWOS++ network code. """


import re
import time
import heapq
import types
import struct
import socket
import random
import argparse


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

        self._init(*struct.unpack(self._format + str(dataLen) + 's', buffer))
        self._createMethods()

    def _init(self, *args):
        assert len(args) == len(self._fields)
        for field, value in zip(self._fields, args):
            setattr(self, '_' + field, value)

    def _createMethods(self):
        for field in self._fields:
            setattr(self, 'get' + field[0].upper() + field[1:], types.MethodType(lambda self, field=field: getattr(self, '_' + field), self))
            setattr(self, 'set' + field[0].upper() + field[1:], types.MethodType(lambda self, value, field=field: setattr(self, '_' + field, value), self))

    def getBytes(self):
        values = []
        for field in self._fields:
            values.append(getattr(self, '_' + field))
        return struct.pack(self._format + str(len(self._data)) + 's', *values)

    @classmethod
    def headerSize(cls):
        if not hasattr(cls, '_headerSize'):
            cls._headerSize = struct.calcsize(cls._format)
        return cls._headerSize

    def __repr__(self):
        ret = '<'
        for field in self._fields:
            ret += field + ': ' + str(getattr(self, '_' + field)) + ', '
        return ret[:-2] + '>'


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


ipRegex = re.compile('(\d+)\.(\d+)\.(\d+)\.(\d+)')

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
    ipxHeader.setDstNetwork(0)
    ipxHeader.setDstHost(hostToInt(host))
    ipxHeader.setDstPort(port)
    ipxHeader.setDstSocket(0x2)
    ipxHeader.setSrcNetwork(1)
    serverHost, serverPort = serverSocket.getsockname()
    serverHost = hostToInt(serverHost)
    ipxHeader.setSrcHost(hostToInt(serverHost))
    ipxHeader.setSrcPort(serverPort)
    ipxHeader.setSrcSocket(0x2)
    ipxHeader.setTransControl(0)
    serverSocket.sendto(ipxHeader.getBytes(), ( host, port ))


class QueuedPacket:
    def __init__(self, packetNo, ipxHeader, message):
        self.packetNo = packetNo
        self.ipxHeader = ipxHeader
        self.message = message

    def __lt__(self, other):
        return self.packetNo < other.packetNo


class ConnectionPool:

    def __init__(self):
        self.connections = set()

    def addConnection(self, host, port):
        print('Adding client {}:{}'.format(host, port))
        self.connections.add(Connection(host, port))

    def getConnection(self, host, port):
        c = Connection(hostToInt(host), port)
        return c if c in self.connections else None

    def getConnectionsExcept(self, host, port):
        host = hostToInt(host)
        return { c for c in self.connections if c.host != host or c.port != port }


def sendIPXPacket(ipConnections, serverSocket, message, ipxHeader):
    if ipxHeader.getDstHost() == 0xffffffff:
        # broadcast
        for connection in ipConnections.getConnectionsExcept(ipxHeader.getSrcHost(), ipxHeader.getSrcPort()):
            serverSocket.sendto(message, ( connection.getHost(), connection.getPort() ))
    else:
        # specific address
        connection = ipConnections.getConnection(ipxHeader.getDstHost(), ipxHeader.getDstPort())
        if connection:
            serverSocket.sendto(message, ( connection.getHost(), connection.getPort() ))


def getReorderedPacket(reorderedPackets, ipxHeader, message):
    gotPacket = False
    queue = reorderedPackets['queue']
    if len(queue) and queue[0].packetNo <= reorderedPackets['packetNo']:
        # we have a winner
        packetFromThePast = heapq.heappop(queue)
        ipxHeader = packetFromThePast.ipxHeader
        message = packetFromThePast.message
        gotPacket = True
    return ipxHeader, message, gotPacket


def reorderPacket(reorderedPackets, ipxHeader, message):
    stuffedPacket = QueuedPacket(reorderedPackets['packetNo'], ipxHeader, message)
    heapq.heappush(reorderedPackets['queue'], stuffedPacket)


def reorderPacket(args, reorderedPackets, ipxHeader, message):
    if args.shuffleProbability:
        ipxHeader, message, gotPacket = getReorderedPacket(reorderedPackets, ipxHeader, message)
        if not gotPacket:
            percentage = 50
            if args.shuffleProbability >= 0:
                percentage = args.shuffleProbability
            if percentage >= random.randint(0, 100):
                reorderPacket(reorderedPackets, ipxHeader, message)
                return None, None

    return ipxHeader, message


def handlePacket(args, reorderedPackets, ipConnections, serverSocket, host, port, message):
    ipxHeader = IPXPacket(message)

    if ipxHeader.getDstSocket() == 0x2 and ipxHeader.getDstHost() == 0x0:
        ipConnections.addConnection(host, port)
        ackClient(serverSocket, host, port)
    else:
        ipxHeader, message = reorderPacket(args, reorderedPackets, ipxHeader, message)
        if ipxHeader:
            # we satisfy heapq requirement, no two packets will ever compare as equal
            reorderedPackets['packetNo'] += 1
            sendIPXPacket(ipConnections, serverSocket, message, ipxHeader)


def skipPacket(args):
    if args.packetLoss:
        percentage = 50
        if args.packetLoss >= 0:
            percentage = args.packetLoss
        return percentage >= random.randint(0, 100)
    return False


def induceLag(args):
    if args.lag:
        len = random.randint(args.lag[0], args.lag[1])
        time.sleep(len * 1000)


def simulateBadNetworkConditions(args):
    # do the nastiness, if requested
    if skipPacket(args):
        return True

    induceLag(args)
    return False


def serverLoop(serverSocket, args):
    print('IPX server online.\nCtrl-break to quit.')
    # todo: add prunning of inactive nodes
    ipConnections = ConnectionPool()
    reorderedPackets = { 'packetNo' : 0, 'queue': [] }

    while True:
        try:
            message, (host, port) = serverSocket.recvfrom(2048)
        except ConnectionError as e:
            pass

        if simulateBadNetworkConditions(args):
            continue

        try:
            handlePacket(args, reorderedPackets, ipConnections, serverSocket, host, port, message)
        except (struct.error, ValueError):
            pass


def parseCommandLine():
    version = 'IPX DOSBox server v1.0'
    description = globals()['__doc__'].strip()

    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('port', nargs='?', type=int, help='port number to listen on', default=213)
    parser.add_argument('-p', '--lose-packets', dest='packetLoss', metavar='<loss percentage>', type=int, choices=range(-1, 101), help='percentage of packet loss (-1 = random)')
    parser.add_argument('-s', '--shuffle-packets', dest='shufflePackets', metavar='<max packets>', type=int, choices=range(0, 10), help='reorder packets randomly, up to a limit')
    parser.add_argument('-u', '--shuffle-probability', dest='shuffleProbability', metavar='<percentage>', type=int, choices=range(-1,101),
        help='probability of out of order packet (-1 = random)')
    parser.add_argument('-l', '--induce-lag', dest='lag', nargs=2, metavar=( '<from>', '<to>' ), type=int, help='introduce lag')
    parser.add_argument('-f', '--log-file', dest='logFile', metavar='<log file>', help='file name to log every activity to')
    parser.add_argument('-v', '--version', action='version', version=version)

    return parser.parse_args()


def main():
    args = parseCommandLine()

    serverSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    serverSocket.bind(('', args.port))

    serverLoop(serverSocket, args)


if __name__ == '__main__':
    main()
