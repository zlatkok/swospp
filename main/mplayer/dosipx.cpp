#include "dosipx.h"
#include "qalloc.h"

/* maximum number of clients we will accept as a server */
#define MAX_CONNECTIONS     8

#define MAX_RECV_PACKETS    10
#define UNACK_POOL_SIZE     6 * 1024

#define DEFAULT_TIMEOUT     8 * 70
#define MAX_TIMEOUT         30 * 70
#define MIN_TIMEOUT         3 * 70

static word m_timeout = DEFAULT_TIMEOUT;    /* anything beyond this we disconnect */
static char *m_lowMemory;                   /* base pointer to low memory         */
static int m_socketId;                      /* the socket port we'll be using     */
static int m_maxPacketSize;                 /* gotten from the driver             */
static SWOSPP_Packet *m_packets[MAX_RECV_PACKETS];
static SWOSPP_Packet *m_sendPacket;
static IPX_Address *m_ourAddress;
static IPX_Address m_broadcastAddress;

#define IMPORTANT_PACKET_SIG    0xa5
#define UNIMPORTANT_PACKET_SIG  0xbd
#define PARTIAL_PACKET_SIG      0x5a
#define ACK_PACKET_SIG          0xca
#define CANCEL_PARTIAL_SIG      0xd7

struct ClientAckId {
    IPX_Address address;
    dword sendingId;
    dword receivingId;
    dword sendingGroupId;
};

static ClientAckId *m_clientAckIds;
static int m_numClients;        /* number of currently connected clients */

static char *AddUnacknowledgedPacket(const IPX_Address *address,
    dword id, byte type, const char *data, int size, int offset);
static void ClearClientUnAckQueue(const IPX_Address *node);
static void SendPacket(const IPX_Address *dest, const char *data, int length);
static void AcknowledgePacket(dword id, const IPX_Address *dest);
void FreeAllUnAck();

struct Fragment {
    int dataSize;
    struct Fragment *next;
    byte data[];
};

struct FragmentLink {
    IPX_Address address;
    dword fragmentId;
    byte partsReceived;
    byte totalParts;
    dword totalSize;
    struct FragmentLink *next;
    Fragment *lastFragment;
    Fragment first;
};

#define MAX_FRAGMENTS 8

static FragmentLink *m_fragmentList;    /* list of pending packet fragments */
static UnAckPacket *m_unAckList;

#define RM_SEG(x) ((uint)(x) >> 4)
#define RM_OFF(x) ((uint)(x) & 0x0f)

static void *AllocateLowMemory(int size);
static void FreeLowMemory(void *ptr);

static dword *GetClientCurrentReceivingId(const IPX_Address *address);
static dword GetClientNextSendingId(const IPX_Address *address);
static dword GetClientNextSendingGroupId(const IPX_Address *address);

/** AddressMatch

    Optimized routine for comparing two IPX addresses. Return true if they match, false if they differ.
*/
bool AddressMatch(const IPX_Address *a, const IPX_Address *b)
{
    assert(a && b);
    const dword *aa = (dword *)a, *bb = (dword *)b;
    return aa[0] == bb[0] && aa[1] == bb[1] && aa[2] == bb[2];
}

/** CopyAddress

    Optimized routine for copying IPX addresses around.
*/
void CopyAddress(IPX_Address *dest, const IPX_Address *source)
{
    assert(dest && source);
    dword *dst = (dword *)dest;
    const dword *src = (const dword *)source;
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

void GetOurAddress(IPX_Address *dest)
{
    CopyAddress(dest, m_ourAddress);
}

#define MOD_ADLER 65521

static dword Adler32(const byte *data, int len)
{
    dword a = 1, b = 0;
    if (len-- > 0) {
        a = (a + *data++) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }

    while (len-- > 0) {
        a += *data++;
        /* no need for asm hackery anymore, GCC handles code like this just fine */
        a -= -(a >= MOD_ADLER) & MOD_ADLER;
        b += a;
        b -= -(b >= MOD_ADLER) & MOD_ADLER;
    }

    return (b << 16) | a;
}

static inline bool IsNetworkTimeoutInRange(word timeout)
{
    return timeout >= MIN_TIMEOUT && timeout <= MAX_TIMEOUT;
}

word SetNetworkTimeout(word timeout)
{
    if (IsNetworkTimeoutInRange(timeout))
        m_timeout = timeout;

    return m_timeout;
}

word GetNetworkTimeout()
{
    return m_timeout;
}

static void ResetClientAckIds(ClientAckId *ackId)
{
    assert(ackId);
    ackId->sendingId = ackId->receivingId = ackId->sendingGroupId = 0;
}

static ClientAckId *FindClientAckId(const IPX_Address *address)
{
    for (int i = 0; i < m_numClients; i++)
        if (AddressMatch(address, &m_clientAckIds[i].address))
            return m_clientAckIds + i;

    return nullptr;
}

/* return current ack id of the client with this address, or nullptr if we don't have them */
static dword *GetClientCurrentReceivingId(const IPX_Address *address)
{
    ClientAckId *c = FindClientAckId(address);
    return c ? &c->receivingId : nullptr;
}

static dword GetClientNextSendingId(const IPX_Address *address)
{
    ClientAckId *c = FindClientAckId(address);
    return c ? ++(*c).sendingId : 0;
}

static void RollbackSendingId(const IPX_Address *address)
{
    ClientAckId *c = FindClientAckId(address);
    if (c)
        c->sendingId--;
}

static dword GetClientNextSendingGroupId(const IPX_Address *address)
{
    ClientAckId *c = FindClientAckId(address);
    return c ? ++(*c).sendingGroupId : 0;
}

static void RemoveFragmentList(FragmentLink *p, FragmentLink *prev)
{
    if (p) {
        Fragment *q = p->first.next;
        while (q) {
            Fragment *next = q->next;
            qFree(q);
            q = next;
        }

        if (prev)
            prev->next = p->next;
        else
            m_fragmentList = p->next;

        qFree(p);
    }
}

static void ReleaseFragmentedPackets()
{
    FragmentLink *p = m_fragmentList;
    while (p) {
        FragmentLink *next = p->next;
        RemoveFragmentList(p, nullptr);
        p = next;
    }

    m_fragmentList = nullptr;
}

static void FindFragmentStart(const IPX_Address *source, dword fragId, FragmentLink **start, FragmentLink **prev)
{
    *start = m_fragmentList;
    *prev = nullptr;

    while (*start) {
        if (AddressMatch(source, &(*start)->address) && fragId == (*start)->fragmentId)
            break;

        *prev = *start;
        *start = (*start)->next;
    }
}

/** AddPacketFragment

    bufSize -> size of returned allocated buffer, if this was last fragment
    data    -> contents of packet fragment
    length  -  length of this fragment's data
    source  -  sender's address
    fragID  -  ID of the whole fragment group
    total   -  total number of fragments for this group

    Packet fragment successfully received. It is guaranteed to be unique and in
    the right order. See if we can assemble full packet, return pointer to it if
    so (memory will be allocated from heap and caller should free it when done).
    If this wasn't last fragment, just return nullptr and store it for later. In
    case buffer of sufficient size could not be allocated, it will be ignored
    without an ack, in hope we'll have enough memory when the sender sends it
    back again. Length will be set to -1 to indicate this situation to the
    caller.
*/
static char *AddPacketFragment(int *bufSize, char *data, int length,
    const IPX_Address *source, dword fragId, int total)
{
    FragmentLink *p, *prev;
    *bufSize = 0;
    FindFragmentStart(source, fragId, &p, &prev);

    if (!p) {
        /* first fragment of a packet arrived */
        p = (FragmentLink *)qAlloc(sizeof(FragmentLink) + length);

        if (!p) {
            WriteToLog("Out of memory while trying to store packet fragment!");
            *bufSize = -1;
            return nullptr;
        }

        p->next = m_fragmentList;
        m_fragmentList = p;
        CopyAddress(&p->address, source);
        p->fragmentId = fragId;
        p->partsReceived = 1;
        p->totalParts = total;
        p->totalSize = length;
        p->lastFragment = nullptr;
        p->first.dataSize = length;
        p->first.next = nullptr;
        memcpy(&p->first.data, data, length);
        return nullptr;
    } else {
        Fragment *q;
        /* we already have at least some previous fragment(s) for this group */
        if (p->partsReceived + 1 == total) {
            /* this is the last fragment, we can recreate original packet */
            char *bufPtr, *curPtr;

            *bufSize = p->totalSize + length;
            if (!(bufPtr = (char *)qAlloc(*bufSize))) {
                WriteToLog("Insufficient memory to assemble packet (needs %d bytes).", *bufSize);
                *bufSize = -1;
                return nullptr;
            }

            /* assemble packet and signal it */
            for (curPtr = bufPtr, q = &p->first; q; q = q->next) {
                memcpy(curPtr, q->data, q->dataSize);
                curPtr += q->dataSize;
            }

            memcpy(curPtr, data, length);
            RemoveFragmentList(p, prev);
            return bufPtr;
        } else {
            /* store this fragment to the end of the list */
            auto q = (Fragment *)qAlloc(sizeof(Fragment) + length);
            if (!q) {
                WriteToLog("Out of memory while trying to store packet fragment!");
                *bufSize = -1;
                return nullptr;
            }

            q->dataSize = length;
            q->next = nullptr;
            memcpy(q->data, data, length);

            if (p->lastFragment)
                p->lastFragment->next = q;
            else
                p->first.next = q;

            p->lastFragment = q;
            p->partsReceived++;
            p->totalSize += length;
            return nullptr;
        }
    }

    return nullptr;
}

static void CancelFragmentedPacket(const IPX_Address *source, dword fragId)
{
    FragmentLink *p, *prev;
    WriteToLog("Received request for cancellation for group with id %d", fragId);
    FindFragmentStart(source, fragId, &p, &prev);

    if (p)
        RemoveFragmentList(p, prev);
    else
        WriteToLog("Group id not found.");
}

/** InitializeNetwork

    Initialize the whole network subsystem. Do IPX installation check,
    determine maximum driver packet size and allocate buffer that has to be in
    low memory in order to be accessible by the IPX network driver. Open our
    socket on dynamically assigned port. Return string error description, or
    nullptr if all went well.

    Memory layout of low memory:
    +--------------------------------+
    |                                |
    | buffer for receiving packets,  |
    | sized dynamically              |
    |                                |
    |--------------------------------|
    | buffer for sending packet      |
    | (1 fragment descriptor -       |
    |  maximum size)                 |
    +--------------------------------|
    | our local ipx address          |
    +--------------------------------+
*/
const char *InitializeNetwork()
{
    if (!IPX_IsInstalled())
        return "IPX compatible network not detected.";

    if (m_lowMemory)      /* if this isn't nullptr we're initialized already */
        return nullptr;

    /* -1 cause DOSBox actually rejects packets of max size */
    if ((m_maxPacketSize = IPX_GetMaximumPacketSize() - 1) < (int)sizeof(IPX_Header) + 128)
        return "Insufficient IPX packet size.";

    WriteToLog("IPX maximum packet size is %d.", m_maxPacketSize);
    /* allocate 1 more byte for receiving buffer just in case (DOSBox receive() allows full size) */
    int lowMemorySize = MAX_RECV_PACKETS * (sizeof(ECB) + (m_maxPacketSize + 1));
    m_sendPacket = (SWOSPP_Packet *)lowMemorySize;
    lowMemorySize += sizeof(ECB) + m_maxPacketSize + 1 + sizeof(IPX_Address);
    m_lowMemory = (char *)AllocateLowMemory(lowMemorySize);
    if (!m_lowMemory)
        return "Failed to allocate DOS memory.";

    WriteToLog("%d bytes of low memory allocated, at %#x", lowMemorySize, m_lowMemory);
    /* got the memory, now initialize it and set up all the pointers */
    memset(m_lowMemory, 0, lowMemorySize);
    /* initialize send buffer, we can only send 1 max size descriptor anyway */
    m_sendPacket = (SWOSPP_Packet *)(m_lowMemory + (int)m_sendPacket);
    m_sendPacket->ecb.fragmentCount = 1;
    m_sendPacket->ecb.fragDesc[0].address[0] = (char *)(&m_sendPacket->ipx) - m_lowMemory;
    m_sendPacket->ecb.fragDesc[0].address[1] = (uint)m_lowMemory >> 4;

    /* get the port dynamically */
    if ((m_socketId = IPX_OpenSocket()) == -1) {
        FreeLowMemory(m_lowMemory);
        m_lowMemory = nullptr;
        return "Failed to open socket.";
    }

    /* and finally, space for our IPX address */
    m_ourAddress = IPX_GetInterNetworkAddress((char *)m_sendPacket + sizeof(ECB) + m_maxPacketSize + 1);
    m_ourAddress->socket = m_socketId;
    HexDumpToLog((char *)m_ourAddress, sizeof(IPX_Address), "our IPX address");

    /* set up receiving ECBs */
    for (int i = 0; i < MAX_RECV_PACKETS; i++) {
        m_packets[i] = (SWOSPP_Packet *)(m_lowMemory + i * (sizeof(ECB) + m_maxPacketSize));
        m_packets[i]->ecb.socketNumber = m_socketId;
        m_packets[i]->ecb.fragmentCount = 1;
        m_packets[i]->ecb.fragDesc[0].address[0] = (uint)&m_packets[i]->ipx - (uint)m_lowMemory;
        m_packets[i]->ecb.fragDesc[0].address[1] = (uint)m_lowMemory >> 4;
        m_packets[i]->ecb.fragDesc[0].size = m_maxPacketSize;
        IPX_Listen((ECB *)m_packets[i]);
    }

    /* set up broadcast node */
    CopyAddress(&m_broadcastAddress, m_ourAddress);
    memset(m_broadcastAddress.node, -1, sizeof(m_broadcastAddress.node));

    /* finish setting up sending ECB */
    m_sendPacket->ecb.socketNumber = m_socketId;
    m_sendPacket->verifyStamp = 'SWPP';
    CopyAddress(&m_sendPacket->ipx.source, m_ourAddress);
    m_clientAckIds = (ClientAckId *)qAlloc(MAX_CONNECTIONS * sizeof(ClientAckId));
    m_numClients = 0;

    /* phew... all OK */
    return nullptr;
}

void ShutDownNetwork()
{
    if (m_lowMemory) {
        CancelPackets();
        IPX_CloseSocket(m_socketId);
        qFree(m_clientAckIds);
        m_clientAckIds = nullptr;
        ReleaseFragmentedPackets();
        WriteToLog("Releasing low memory, ptr is %#x", m_lowMemory);
        FreeLowMemory(m_lowMemory);
        m_lowMemory = nullptr;
    }
}

bool ConnectTo(const IPX_Address *dest)
{
    ClientAckId *c = FindClientAckId(dest);
    if (c) {
        ResetClientAckIds(c);
        return true;
    }

    /* shouldn't really happen */
    if (m_numClients >= MAX_CONNECTIONS)
        return false;

    c = &m_clientAckIds[m_numClients++];
    CopyAddress(&c->address, dest);
    ResetClientAckIds(c);
    WriteToLog("New connection to [%#.12s] established.", dest);
    return true;
}

void DisconnectFrom(const IPX_Address *dest)
{
    int i;
    ClearClientUnAckQueue(dest);
    for (i = 0; i < m_numClients; i++)
        if (AddressMatch(&m_clientAckIds[i].address, dest))
            break;

    if (i >= m_numClients) {
        WriteToLog("Trying to disconnect from non-connected client: [%#.12s]", dest);
        return;
    }

    memmove(m_clientAckIds + i, m_clientAckIds + i + 1, (m_numClients-- - i - 1) * sizeof(*m_clientAckIds));
    IPX_Disconnect(dest);
    WriteToLog("Disconnected from [%#.12s].", dest);
}

void SendBroadcastPacket(const char *data, int length)
{
    m_sendPacket->type = UNIMPORTANT_PACKET_SIG;
    SendPacket(&m_broadcastAddress, data, length);
}

void SendAck(const IPX_Address *dest, dword id)
{
    //WriteToLog("Acknowledging packet %d from [%#.12s]", id, dest);
    m_sendPacket->type = ACK_PACKET_SIG;
    SendPacket(dest, (char *)&id, sizeof(id));
}

void SendSimplePacket(const IPX_Address *dest, const char *data, int length)
{
    if ((int)(length + sizeof(SWOSPP_Packet) - sizeof(ECB)) > m_maxPacketSize) {
        WriteToLog("Trying to send too large simple packet. Size = %d, max size = %d.",
            length, m_maxPacketSize - sizeof(SWOSPP_Packet) + sizeof(ECB));
        return;
    }

    m_sendPacket->type = UNIMPORTANT_PACKET_SIG;
    SendPacket(dest, data, length);
}

bool SendImportantPacket(const IPX_Address *dest, const char *destBuf, int destSize)
{
    if (!FindClientAckId(dest)) {
        WriteToLog("Trying to send important packet to a non-connected client [%#.12s]", dest);
        return false;
    }

    if (destSize <= (int)(m_maxPacketSize - sizeof(SWOSPP_Packet) + sizeof(ECB) - sizeof(dword))) {
        /* not fragmented */
        char *data;
        if (!(data = AddUnacknowledgedPacket(dest, GetClientNextSendingId(dest),
            IMPORTANT_PACKET_SIG, destBuf, destSize, 0)))
            return false;
        m_sendPacket->type = IMPORTANT_PACKET_SIG;
        SendPacket(dest, data, destSize + sizeof(dword));
    } else {
        /* fragmented */
        int bytesLeft = destSize, dataSpace, totalFragments, numFragmentedIds = 0;
        dword fragmentedIds[MAX_FRAGMENTS], groupId = GetClientNextSendingGroupId(dest);
        if (!groupId) {
            WriteToLog("Trying to send fragmented packet to a non-connected client");
            return false;
        }

        /* extra space for: dword id, dword group id, byte total */
        dataSpace = m_maxPacketSize - sizeof(SWOSPP_Packet) + sizeof(ECB) - 9;
        totalFragments = (destSize + dataSpace - 1) / dataSpace;
        if (totalFragments > MAX_FRAGMENTS) {
            WriteToLog("Trying to send too big packet, size = %d, %d fragments", destSize, totalFragments);
            return false;
        }

        while (bytesLeft > 0) {
            dword id = GetClientNextSendingId(dest);
            char *data = AddUnacknowledgedPacket(dest, id, PARTIAL_PACKET_SIG,
                destBuf + destSize - bytesLeft, min(bytesLeft, dataSpace), 5);

            if (!data) {
                int i;
                RollbackSendingId(dest);    /* or client will wait for this packet forever */
                /* cancel all fragments - entire group fails if one fails */
                WriteToLog("Out of memory while trying to send fragmented packet!");

                for (i = 0; i < numFragmentedIds; i++)
                    AcknowledgePacket(fragmentedIds[i], dest);

                m_sendPacket->type = CANCEL_PARTIAL_SIG;
                SendPacket(dest, (char *)groupId, sizeof(groupId));
                return false;
            }

            fragmentedIds[numFragmentedIds++] = id;
            *(dword *)(data + 4) = groupId;
            data[8] = totalFragments;
            m_sendPacket->type = PARTIAL_PACKET_SIG;
            SendPacket(dest, data, min(bytesLeft, dataSpace) + sizeof(dword) + 5);
            bytesLeft -= dataSpace;
        }
    }
    return true;
}

static void SendPacket(const IPX_Address *dest, const char *data, int length)
{
    int i;
    //WriteToLog("Sending packet of size %d", length);
    assert(length > 0);
    for (i = 0; i < 200 && m_sendPacket->ecb.inUseFlag; i++) {
        WriteToLog("SendPacket(): ecb in use flag != 0, impossible!!!");
        IPX_OnIdle();
    }

    memcpy(m_sendPacket->ecb.immediateAddress, dest->node, sizeof(dest->node));
    CopyAddress(&m_sendPacket->ipx.destination, dest);
    m_sendPacket->ecb.fragmentCount = 1;
    m_sendPacket->ecb.fragDesc[0].size = length + sizeof(SWOSPP_Packet) - sizeof(ECB);
    assert(m_sendPacket->ecb.fragDesc[0].size <= m_maxPacketSize);

    /* copy the data */
    memcpy(m_sendPacket->data, data, length);
    m_sendPacket->verifyStamp = 'SWPP';
    m_sendPacket->adler32Checksum = Adler32((const byte *)data, length);
    //HexDumpToLog((char *)m_sendPacket, sizeof(SWOSPP_Packet) + length, "next packet to send");

    IPX_Send((ECB *)m_sendPacket);
    if (m_sendPacket->ecb.completionCode)
        WriteToLog("Error sending packet! Code is %#02x", m_sendPacket->ecb.completionCode);
}

static char *ReceiveNextPacket(int *destSize, IPX_Address *node, int currentPacketIndex)
{
    static int packetProcessingDepth;
    char *packet;
    IPX_Listen((ECB *)m_packets[currentPacketIndex]); /* repost the ECB */

    /* prevent stack overflow in case of flooding, also limit depth because
       it affects the performance (menus become unresponsive)
       (bah, seems that IPX itself causes it... or it's just VMWare) */
    if (stackavail() < 128 || packetProcessingDepth > MAX_RECV_PACKETS * 2) {
        WriteToLog("Stack overflow prevention triggered.");
        *destSize = 0;
        return nullptr;
    }

    packetProcessingDepth++;
    packet = ReceivePacket(destSize, node);
    packetProcessingDepth--;
    return packet;
}

/** ReceivePacket

    destSize -> will contain the size of the packet after returning
    node     -> will contain sender's address after returning

    Return pointer to packet bytes allocated from the heap. Handle all incoming packet types.
    If there are no pending packets return nullptr. Also return nullptr if out of memory,
    differentiate that case by setting size to -1. Discard all invalid packets found.
*/
char *ReceivePacket(int *destSize, IPX_Address *node)
{
    dword id = 0, *currentId;
    int i, size;
    char *srcPtr, *destBuf = nullptr;

    /* ignoring malformed packets etc. */
    for (i = 0; i < MAX_RECV_PACKETS; i++)
        if (!m_packets[i]->ecb.inUseFlag && !m_packets[i]->ecb.completionCode)
            break;

    if (i >= MAX_RECV_PACKETS) {
        *destSize = 0;
        return nullptr;
    }

    size = htons(m_packets[i]->ipx.length) - sizeof(SWOSPP_Packet) + sizeof(ECB);
    /* our packets always have size limit */
    if (size < 0) {
        WriteToLog("Rejecting invalid size packet, with total size of %d bytes",
            htons(m_packets[i]->ipx.length));
        return ReceiveNextPacket(destSize, node, i);
    }

    srcPtr = m_packets[i]->data;
    /* get the sender's address and data */
    CopyAddress(node, &m_packets[i]->ipx.source);

    // can't have this if we're gonna let guys join the game
    //if (!FindClientAckId(node)) {
    //    WriteToLog("Rejecting packet from non-connected sender [%#.12s]", node);
    //    return ReceiveNextPacket(destSize, node, i);
    //}

    /* verify the stamp */
    if (m_packets[i]->verifyStamp != 'SWPP') {
        WriteToLog("Rejecting packet with invalid stamp: %08x:", m_packets[i]->verifyStamp);
        return ReceiveNextPacket(destSize, node, i);
    }

    /* this is probably overkill */
    if (m_packets[i]->adler32Checksum != Adler32((const byte *)m_packets[i]->data, size)) {
        WriteToLog("Rejecting packet with invalid Adler32 checksum (%d, expecting %d).",
            m_packets[i]->adler32Checksum, Adler32((const byte *)m_packets[i]->data, size));
        HexDumpToLog(m_packets[i]->data, size, "packet with failed checksum");
        return ReceiveNextPacket(destSize, node, i);
    }

    switch (m_packets[i]->type) {
    case UNIMPORTANT_PACKET_SIG:
        /* OK, with simple packet we're done already */
        break;

    case PARTIAL_PACKET_SIG:
    case IMPORTANT_PACKET_SIG:
        if (size < 4 + 5 * (m_packets[i]->type == PARTIAL_PACKET_SIG)) {
            WriteToLog("Malformed important/partial packet rejected, size = %d", size);
            return ReceiveNextPacket(destSize, node, i);
        }

        if (!(currentId = GetClientCurrentReceivingId(&m_packets[i]->ipx.source))) {
            WriteToLog("Rejecting important packet because client not connected.");
            HexDumpToLog(&m_packets[i]->ipx.source, sizeof(IPX_Address), "disconnected client address");
            /* maybe send them disconnect too for good measure? */
            DisconnectFrom(&m_packets[i]->ipx.source);
            return ReceiveNextPacket(destSize, node, i);
        }

        id = *(dword *)srcPtr;
        if (id > *currentId + 1) {
            /* OUT OF ORDER! YOU WILL OBEY! */
            //WriteToLog("Out of order packet detected, id = %d, expecting %d", id, *currentId + 1);
            return ReceiveNextPacket(destSize, node, i);
        }

        if (id == *currentId + 1) {
            /* cool, this is expected next packet, bump id and send it up there */
            size -= sizeof(dword);
            srcPtr += sizeof(dword);

            /* now process fragmented packet, if it is one */
            if (m_packets[i]->type == PARTIAL_PACKET_SIG) {
                byte total;
                dword fragId;

                if (size < 6) {
                    WriteToLog("Malformed partial packet rejected, size = %d.", size);
                    return ReceiveNextPacket(destSize, node, i);
                }

                fragId = *(dword *)srcPtr;
                total = srcPtr[4];

                if (total > MAX_FRAGMENTS) {
                    WriteToLog("Too many fragments for partial packet (%d).", total);
                    return ReceiveNextPacket(destSize, node, i);
                }

                size -= 5;
                srcPtr += 5;

                /* in case of memory exhaustion don't send ack, they'll send it again
                   and we'll hopefully have enough memory by then */
                if (!(destBuf = AddPacketFragment(destSize, srcPtr, size, node, fragId, total))) {
                    if (*destSize >= 0) {
                        ++*currentId;
                        SendAck(&m_packets[i]->ipx.source, id);
                    }

                    return ReceiveNextPacket(destSize, node, i);
                }

                /* we've got a fully assembled packet from fragments now */
                srcPtr = destBuf;
                size = *destSize;
            }
            ++*currentId;
            //WriteToLog("Increasing current id for node [%#.12s], id is %d now.", &m_packets[i]->ipx.source, *currentId);
        } else {
            /* we've already processed this one, get next */
            //WriteToLog("Discarding packet with ID %d (already processed, cur. id = %d)", id, *currentId);
            SendAck(&m_packets[i]->ipx.source, id);
            return ReceiveNextPacket(destSize, node, i);
        }
        break;

    case ACK_PACKET_SIG:
        if (size != 4)
            WriteToLog("Discarding ack packet with invalid size (%d)", size);
        else {
            AcknowledgePacket(*(dword *)srcPtr, &m_packets[i]->ipx.source);
        }

        return ReceiveNextPacket(destSize, node, i);

    case CANCEL_PARTIAL_SIG:
        if (size != sizeof(dword))
            WriteToLog("Received partial cancellation packet with invalid size: %d", size);
        else
            CancelFragmentedPacket(&m_packets[i]->ipx.source, *(dword *)srcPtr);
        return ReceiveNextPacket(destSize, node, i);
        break;

    default:
        WriteToLog("Invalid packet type: %#x", m_packets[i]->type);
        return ReceiveNextPacket(destSize, node, i);
    }

    if (!destBuf)
        destBuf = (char *)qAlloc(size);

    if (!destBuf) {
        *destSize = -1; /* signal memory exhaustion */
        return nullptr;
    }

    /* only ack if we're sure everything went well */
    if (m_packets[i]->type == PARTIAL_PACKET_SIG || m_packets[i]->type == IMPORTANT_PACKET_SIG)
        SendAck(&m_packets[i]->ipx.source, id);

    if (m_packets[i]->type != PARTIAL_PACKET_SIG) {
        *destSize = size;
        memcpy(destBuf, srcPtr, size);
    }

    //WriteToLog("Source address: [%#.12s]", node);
    //HexDumpToLog(m_packets[i], size + sizeof(SWOSPP_Packet), "received packet data");
    IPX_Listen((ECB *)m_packets[i]);
    return destBuf;
}

/** DismissIncomingPackets

    Discards any received packet at the same time confirming any pending packet should the
    acknowledgement arrive.
*/
void DismissIncomingPackets()
{
    int length;
    IPX_Address node;

    while (char *packet = ReceivePacket(&length, &node))
        qFree(packet);
}

void CancelPackets()
{
    WriteToLog("Canceling all packets...");

    /* cancel all pending operations */
    for (int i = 0; i < MAX_RECV_PACKETS; i++)
        if (m_packets[i]->ecb.inUseFlag && m_packets[i]->ecb.completionCode != 0xf9)
            IPX_Cancel((ECB *)m_packets[i]);

    /* cancel unAck and fragmented packets */
    FreeAllUnAck();
    ReleaseFragmentedPackets();
}

void CancelSendingPackets()
{
    if (m_sendPacket->ecb.completionCode != 0xf9)
        IPX_Cancel((ECB *)m_sendPacket);

    FreeAllUnAck();
    ReleaseFragmentedPackets();
}

/* Unacknowledged pool manipulation */


/** ResendUnacknowledgedPackets

    Goes through all unacknowledged packets that we are still sending, and resends them.
    Returns pointer to a timed-out packet, or nullptr if everything OK (no timed out packets).
    Make sure to call it every cycle.
*/
UnAckPacket *ResendUnacknowledgedPackets()
{
    UnAckPacket *ret = nullptr;
    dword currentTime = g_currentTick;
    word timeout = GetNetworkTimeout();

    for (UnAckPacket *p = m_unAckList; p; p = p->next) {
        if (p->time + timeout < currentTime)
            ret = p;
        else
            ResendTimedOutPacket(p);
    }

    return ret;
}

bool UnacknowledgedPacketsPending()
{
    return m_unAckList;
}

/* Call it with result from ResendUnacknowledgedPackets() if retrying. */
void ResendTimedOutPacket(UnAckPacket *packet)
{
    //WriteToLog("Resending packet to [%#.12s] with id %d of size %d",
    //    &packet->address, *(dword *)packet->data, packet->size);
    m_sendPacket->type = packet->type;
    SendPacket(&packet->address, (const char *)packet->data, packet->size);
}

/* Add important packet that's about to be sent to the list of unacknowledged packets. */
char *AddUnacknowledgedPacket(const IPX_Address *address, dword id, byte type, const char *data, int size, int offset)
{
    UnAckPacket *p;
    assert(offset >= 0);
    assert(size + (int)sizeof(dword) + offset <= m_maxPacketSize);
    //WriteToLog("Adding unacknowledged packet to queue, for [%#.12s], id is %d", address, id);

    if (!(p = (UnAckPacket *)qAlloc(sizeof(UnAckPacket) + size + sizeof(dword) + offset)))
        return nullptr;

    p->size = size + sizeof(dword) + offset;
    CopyAddress(&p->address, address);
    p->next = m_unAckList;
    m_unAckList = p;
    *p->data = id;
    memcpy((char *)p->data + sizeof(dword) + offset, data, size);
    p->time = g_currentTick;
    p->type = type;
    return (char *)p->data;
}

/* If dest/id in list, remove the packet */
static void AcknowledgePacket(dword id, const IPX_Address *dest)
{
    UnAckPacket *p, **prev = &m_unAckList;
    //WriteToLog("Packet with id %d for [%#.12s] confirmed.", id, dest);

    for (p = m_unAckList; p; p = p->next) {
        if (*p->data == id && AddressMatch(&p->address, dest))
            break;
        prev = &p->next;
    }

    if (p) {
        *prev = p->next;
        qFree(p);
    }
}

/* Mark whole pool free. */
void FreeAllUnAck()
{
    UnAckPacket *p = m_unAckList;
    while (p) {
        UnAckPacket *next = p->next;
        qFree(p);
        p = next;
    }

    m_unAckList = nullptr;
}

/* Delete all unconfirmed packets for this ipx address. */
static void ClearClientUnAckQueue(const IPX_Address *node)
{
    UnAckPacket *p = m_unAckList, **prev = &m_unAckList;
    //WriteToLog("Clearing unack queue for [%#.12s]", node);

    while (p) {
        UnAckPacket *next = p->next;
        if (AddressMatch(node, &p->address)) {
            *prev = next;
            qFree(p);
        } else
            prev = &p->next;
        p = next;
    }
}

/************************************

  Low level IPX routines and helpers

*************************************/

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

/* execute real mode interrupt from protected mode */
static int RM_Interrupt(int num, RM_Info *rm)
{
    union REGS regs{};
    struct SREGS sregs{};
    segread(&sregs);
    regs.w.ax = 0x0300;
    regs.w.bx = (short)num;
    regs.w.cx = 0;
    sregs.es = FP_SEG(rm);
    regs.x.edi = FP_OFF(rm);
    return int386x(0x31, &regs, &regs, &sregs);
}

/* get low memory for network driver */
void *AllocateLowMemory(int size)
{
    RM_Info rm{};
    rm.eax = 0x4800;
    rm.ebx = (size + 15) / 16;
    RM_Interrupt(0x21, &rm);
    return rm.flags & 1 ? nullptr : (void *)((rm.eax & 0xffff) << 4);
}

void FreeLowMemory(void *ptr)
{
    RM_Info rm{};
    rm.eax = 0x4900;
    rm.es = (short)((unsigned long)ptr >> 4);
    RM_Interrupt(0x21, &rm);
    if (rm.flags & 1)
        WriteToLog("Error while freeing low memory block %#0x", ptr);
}

bool32 IPX_IsInstalled()
{
    RM_Info rm{};
    rm.eax = 0x7a00;
    RM_Interrupt(0x2f, &rm);
    WriteToLog("IPX FAR entry point should be %#x:%#x", rm.es, rm.edi);
    return (rm.eax & 0xff) == 0xff;
}

/* Note: DOSBox currently doesn't support multiple networks, so we'll just get
   a local node number.
*/
IPX_Address *IPX_GetInterNetworkAddress(char *addr)
{
    RM_Info rm{};
    rm.ebx = 0x0009;
    rm.es = (uint)m_lowMemory >> 4;
    rm.esi = addr - m_lowMemory;
    RM_Interrupt(0x7a, &rm);
    return (IPX_Address *)addr;
}

int IPX_OpenSocket()
{
    RM_Info rm{};
    RM_Interrupt(0x7a, &rm);
    WriteToLog("Result from open socket: %#x, socket = %#x", rm.eax & 0xff, rm.edx);
    return rm.eax & 0xff ? -1 : rm.edx;
}

void IPX_CloseSocket(int socketNumber)
{
    RM_Info rm{};
    rm.ebx = 1;
    rm.edx = socketNumber;
    RM_Interrupt(0x7a, &rm);
    WriteToLog("Closed socket: %#x", socketNumber & 0xffff);
}

/** IPX_OnIdle

    Novell NetWare - IPX Driver - RELINQUISH CONTROL

    This call indicates that the application is idle and permits the IPX driver
    to do some work. Just an empty stub in DOSBox, but we still honour it.
*/
void IPX_OnIdle()
{
    RM_Info rm{};
    rm.ebx = 0x000a;
    RM_Interrupt(0x7a, &rm);
}

void IPX_Listen(ECB *ecb)
{
    RM_Info rm{};
    rm.es = RM_SEG(ecb);
    rm.esi = RM_OFF(ecb);
    rm.ebx = 0x0004;
    RM_Interrupt(0x7a, &rm);
    if (rm.eax & 0xff)
        WriteToLog("IPX_Listen failed, error code: %d", rm.eax & 0xff);
}

void IPX_Send(ECB *ecb)
{
    RM_Info rm{};
    rm.es = RM_SEG(ecb);
    rm.esi = RM_OFF(ecb);
    rm.ebx = 0x0003;
    RM_Interrupt(0x7a, &rm);
    if (rm.eax & 0xff)
        WriteToLog("IPX_Send failed, error code: %d", rm.eax & 0xff);
}

void IPX_Cancel(ECB *ecb)
{
    RM_Info rm{};
    rm.es = RM_SEG(ecb);
    rm.esi = RM_OFF(ecb);
    rm.ebx = 0x0006;
    RM_Interrupt(0x7a, &rm);
    if (rm.eax & 0xff)
        WriteToLog("IPX_Cancel failed, error code: %d", rm.eax & 0xff);
}

uint IPX_GetMaximumPacketSize()
{
    RM_Info rm{};
    rm.ebx = 0x001a;
    RM_Interrupt(0x7a, &rm);
    WriteToLog("IPX retry count is %hu", rm.ecx);
    return rm.eax;
}

/** IPX_Disconnect

    Documentation states: "Only use in point-to-point networks". Not completely
    sure what that means; DOSBox doesn't even implement this call, and as for
    real networks, I can only hope we'll be fine :)
*/
void IPX_Disconnect(const IPX_Address *address)
{
    RM_Info rm{};
    rm.es = RM_SEG(address);
    rm.esi = RM_OFF(address);
    rm.ebx = 0x000b;
    RM_Interrupt(0x7a, &rm);
}
