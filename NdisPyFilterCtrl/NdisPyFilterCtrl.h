// ========================================================================================================================
// NdisPyFilterCtrl
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisPyFilterCtrl.cpp
//
// Created: 01/09/2007
// ========================================================================================================================

#pragma once

// ========================================================================================================================

extern CRITICAL_SECTION g_ConsoleCriticalSection;

extern HANDLE g_hDevice;
extern HANDLE g_hExitEvent;

// ========================================================================================================================

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;

// ========================================================================================================================

#pragma pack(push, 1)

// ========================================================================================================================

typedef struct _EthernetFrameHeader
{
	u_char DestinationMac[6];
	u_char SourceMac[6];
	u_short Type;
} EthernetFrameHeader;

// ------------------------------------------------------------------------------------------------------------------------

typedef struct _ArpPacketHeader
{
	u_short HardwareAddressSpace;
	u_short ProtocolAddressSpace;
	u_char HardwareAddressLength;
	u_char ProtocolAddressLength;
	u_short Operation;
	u_char SenderHardwareAddress[6];
	u_int SenderProtocolAddress;
	u_char TargetHardwareAddress[6];
	u_int TargetProtocolAddress;
} ArpPacketHeader;

// ------------------------------------------------------------------------------------------------------------------------

typedef struct _IpPacketHeader
{
	u_char VersionInternetHeaderLength;
	u_char TypeOfService;
	u_short TotalLength;
	u_short Identification;
	u_short FlagsFragmentOffset;
	u_char TimeToLive;
	u_char Protocol;
	u_short Crc;
	u_int SourceAddress;
	u_int DestinationAddress;
} IpPacketHeader;

// ------------------------------------------------------------------------------------------------------------------------

typedef struct _IcmpPacketHeader
{
	u_char Type;
	u_char Code;
	u_short Checksum;
	u_short Id;
	u_short Sequence;
} IcmpPacketHeader;

// ------------------------------------------------------------------------------------------------------------------------

typedef struct _ChecksumPseudoHeader
{
	u_int SourceAddress;
	u_int DestinationAddress;
	u_char Zero;
	u_char Protocol;
	u_short Length;
} ChecksumPseudoHeader;

// ------------------------------------------------------------------------------------------------------------------------

typedef struct _TcpPacketHeader
{
	u_short SourcePort;
	u_short DestinationPort;
	u_int SequenceNumber;
	u_int AcknowledgementNumber;
	u_char DataOffset;
	u_char Flags;
	u_short Window;
	u_short Checksum;
	u_short UrgentPointer;
} TcpPacketHeader;

// ------------------------------------------------------------------------------------------------------------------------

typedef struct _UdpPacketHeader
{
	u_short SourcePort;
	u_short DestinationPort;
	u_short Length;
	u_short Checksum;
} UdpPacketHeader;

// ========================================================================================================================

#pragma pack(pop)

// ========================================================================================================================

typedef enum _EtherType
{
	EtherTypeIp = 0x0008,
	EtherTypeArp = 0x0608
} EtherType;

// ------------------------------------------------------------------------------------------------------------------------

typedef enum _ArpOperation
{
	ArpOperationWhoHas = 0x0100,
	ArpOperationIsAt = 0x0200
} ArpOperation;

// ------------------------------------------------------------------------------------------------------------------------

typedef enum _IpProtocol
{
	IpProtocolIcmp = 1,
	IpProtocolTcp = 6,
	IpProtocolUdp = 17
} IpProtocol;

// ------------------------------------------------------------------------------------------------------------------------

typedef enum _TcpFlag
{
	TcpFlagFin = 0x01,
	TcpFlagSyn = 0x02,
	TcpFlagRst = 0x04,
	TcpFlagPsh = 0x08,
	TcpFlagAck = 0x10,
	TcpFlagUrg = 0x20,
	TcpFlagEce = 0x40,
	TcpFlagCwr = 0x80
} TcpFlag;

// ========================================================================================================================

inline void InitialiseChecksum(u_int &checksum)
{
	checksum = 0;
}

// ------------------------------------------------------------------------------------------------------------------------

inline void UpdateChecksum(u_int &checksum, u_short *dataWords, u_int dataWordCount)
{
	while(dataWordCount-- > 0)
	{
		checksum += *(dataWords++);
	}
}

// ------------------------------------------------------------------------------------------------------------------------

inline u_short FinaliseChecksum(u_int &checksum)
{
	checksum = (checksum >> 16) + (checksum & 0xFFFF);
	checksum += (checksum >> 16);
	return static_cast<u_short>(~checksum);
}

// ========================================================================================================================
