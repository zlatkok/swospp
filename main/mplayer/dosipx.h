#pragma once

#include "swos.h"

#ifndef htons
#define htons(x) ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))
#endif

#pragma pack(push, 1)
typedef struct RM_Info {
    long edi, esi, ebp, resv, ebx, edx, ecx, eax;
    ushort flags, es, ds, fs, gs, ip, cs, sp, ss;
} RM_Info;

typedef struct IPX_Address {
    byte network[4];    // Big-Endian
    byte node[6];       // Big-Endian
    word socket;        // Big-Endian
} IPX_Address;

typedef struct IPX_Header {
    word checkSum;
    word length;
    byte transportControl;
    byte packetType;
    IPX_Address destination;
    IPX_Address source;
} IPX_Header;

typedef struct ECB_Fragment {
    word address[2];            /* offset-segment */
    word size;                  /* low-high */
} ECB_Fragment;

typedef struct ECB {
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
} ECB;

typedef struct SWOSPP_Packet {
    ECB ecb;
    IPX_Header ipx;
    dword verifyStamp;
    dword adler32Checksum;
    byte type;
    char data[];
} SWOSPP_Packet;
#pragma pack(pop)

typedef struct UnAckPacket {
    int size;
    dword time;
    IPX_Address address;
    struct UnAckPacket *next;
    byte type;
    dword data[];
} UnAckPacket;

extern "C" const char *InitializeNetwork();
extern "C" void ShutDownNetwork();

word GetNetworkTimeout();
void SetNetworkTimeout(word newTimeout);
void GetOurAddress(IPX_Address *dest);
bool addressMatch(const IPX_Address *a, const IPX_Address *b);
void copyAddress(IPX_Address *dest, const IPX_Address *source);

UnAckPacket *ResendUnacknowledgedPackets();
void ResendTimedOutPacket(UnAckPacket *packet);

bool ConnectTo(const IPX_Address *dest);
void DisconnectFrom(const IPX_Address *dest);
void SendBroadcastPacket(const char *data, int length);
void SendSimplePacket(const IPX_Address *dest, const char *data, int length);
bool SendImportantPacket(const IPX_Address *dest, const char *data, int length);
bool SendUnaknowledged();
char *ReceivePacket(int *length, IPX_Address *node);
void CancelSendingPackets();
void CancelPackets();

/* Raw IPX interface */
extern "C" {
    bool IPX_IsInstalled(); /* return true if IPX Network driver is installed           */
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