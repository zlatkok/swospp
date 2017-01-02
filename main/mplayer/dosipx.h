#pragma once

#ifndef htons
#define htons(x) ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))
#endif

#pragma pack(push, 1)
struct RM_Info {
    long edi, esi, ebp, reserved, ebx, edx, ecx, eax;
    ushort flags, es, ds, fs, gs, ip, cs, sp, ss;
};

struct IPX_Address {
    byte network[4];    // big-endian
    byte node[6];       // big-endian
    word socket;        // big-endian
};

struct IPX_Header {
    word checkSum;
    word length;
    byte transportControl;
    byte packetType;
    IPX_Address destination;
    IPX_Address source;
};

struct ECB_Fragment {
    word address[2];            /* offset-segment */
    word size;                  /* low-high */
};

struct ECB {
    word link[2];               /* offset-segment */
    word ESRAddress[2];         /* offset-segment */
    byte inUseFlag;
    byte completionCode;
    word socketNumber;          /* high-low */
    byte IPXWorkspace[4];
    byte driverWorkspace[12];
    byte immediateAddress[6];   /* low-high */
    word fragmentCount;         /* low-high */
    ECB_Fragment fragDesc[1];   /* number of fragment descriptors will always be 1 */
};

struct SWOSPP_Packet {
    ECB ecb;
    IPX_Header ipx;
    dword verifyStamp;
    dword adler32Checksum;
    byte type;
    char data[];
};
#pragma pack(pop)

struct UnAckPacket {
    int size;
    dword time;
    IPX_Address address;
    struct UnAckPacket *next;
    byte type;
    dword data[];
};

extern "C" const char *InitializeNetwork();
extern "C" void ShutDownNetwork();

word GetNetworkTimeout();
word SetNetworkTimeout(word newTimeout);
void GetOurAddress(IPX_Address *dest);
bool AddressMatch(const IPX_Address *a, const IPX_Address *b);
void CopyAddress(IPX_Address *dest, const IPX_Address *source);

UnAckPacket *ResendUnacknowledgedPackets();
bool UnacknowledgedPacketsPending();
void ResendTimedOutPacket(UnAckPacket *packet);

bool ConnectTo(const IPX_Address *dest);
void DisconnectFrom(const IPX_Address *dest);
void SendBroadcastPacket(const char *data, int length);
void SendSimplePacket(const IPX_Address *dest, const char *data, int length);
bool SendImportantPacket(const IPX_Address *dest, const char *data, int length);
char *ReceivePacket(int *length, IPX_Address *node);
void CancelSendingPackets();
void CancelPackets();
void DismissIncomingPackets();

/* Raw IPX interface */
extern "C" {
    bool32 IPX_IsInstalled(); /* return true if IPX Network driver is installed         */
    void IPX_OnIdle();      /* give IPX driver a slice of CPU                           */
    IPX_Address *IPX_GetInterNetworkAddress(char *addr);    /* get our address          */
    void IPX_Listen(ECB *ecb);  /* put this ECB into listening queue                    */
    uint IPX_GetMaximumPacketSize();                 /* including header                */
    void IPX_Disconnect(const IPX_Address *address); /* disconnect from target machine  */
    int  IPX_OpenSocket();
    void IPX_CloseSocket(int socketNumber);
    void IPX_Send(ECB *ecb);
    void IPX_Cancel(ECB *ecb);
}
