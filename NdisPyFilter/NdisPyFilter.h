// ========================================================================================================================
// NdisPyFilter
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisPyFilter.h
//
// Created: 01/09/2007
// ========================================================================================================================

#pragma once

#pragma warning(disable:4047)

// ========================================================================================================================

NDIS_STATUS RegisterDevice(VOID);
NDIS_STATUS DeregisterDevice(VOID);

NTSTATUS DeviceDispatch(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp);
NTSTATUS DeviceIoControlDispatch(IN PIRP pIrp, IN PIO_STACK_LOCATION pIrpStack);
NTSTATUS DeviceReadDispatch(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp);
NTSTATUS DeviceWriteDispatch(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp);

VOID FlushRecvPacketQueue();
VOID FlushSendPacketQueue();

VOID CsReadQueueInsertIrp(IN PIO_CSQ pCsq, IN PIRP pIrp);
VOID CsReadQueueRemoveIrp(IN PIO_CSQ pCsq, IN PIRP Irp);
PIRP CsReadQueuePeekNextIrp(IN PIO_CSQ pCsq, IN PIRP pIrp, IN PVOID pPeekContext);
VOID CsReadQueueAcquireLock(IN PIO_CSQ pCsq, OUT PKIRQL pIrql);
VOID CsReadQueueReleaseLock(IN PIO_CSQ pCsq, IN KIRQL Irql);
VOID CsReadQueueCompleteCanceledIrp(IN PIO_CSQ pCsq, IN PIRP pIrp);

// ========================================================================================================================

extern NDIS_HANDLE g_NdisDeviceHandle;
extern NDIS_HANDLE g_NdisDriverHandle;
extern NDIS_HANDLE g_NdisProtocolHandle;
extern NDIS_HANDLE g_NdisWrapperHandle;
extern PDEVICE_OBJECT g_ControlDeviceObject;

extern NDIS_SPIN_LOCK g_Lock;
extern KSPIN_LOCK g_IoReadQueueListLock;
extern KSPIN_LOCK g_PktRecvQueueListLock;
extern KSPIN_LOCK g_PktSendQueueListLock;

extern IO_CSQ g_IoCsReadQueue;

extern LIST_ENTRY g_AdapterList;
extern LIST_ENTRY g_IoReadQueueList;
extern LIST_ENTRY g_PktRecvQueueList;
extern LIST_ENTRY g_PktSendQueueList;

extern ULONG g_CtrlHandleCount;
extern ULONG g_MiniportCount;

// ========================================================================================================================

static const ULONG MemoryTag = 'NPyF';

#define MaxPacketArraySize 0x00000020
#define MinPacketPoolSize 0x000000FF
#define MaxPacketPoolSize 0x0000FFFF

static NDIS_MEDIUM SupportedMediumArray[1] =
{
	NdisMedium802_3
};

// ========================================================================================================================

typedef struct _ADAPTER
{
	LIST_ENTRY Link;
	NDIS_SPIN_LOCK AdapterLock;

	NDIS_STRING AdapterDeviceName;
	NDIS_MEDIUM AdapterMedium;
	
	NDIS_HANDLE BindingHandle;
	NDIS_HANDLE MiniportHandle;

	NDIS_DEVICE_POWER_STATE MiniportDevicePowerState;
	NDIS_DEVICE_POWER_STATE ProtocolDevicePowerState;

	NDIS_EVENT CompletionEvent;
	NDIS_STATUS CompletionStatus;

	NDIS_REQUEST Request;
	PULONG RequestBytesNeeded;
	PULONG RequestBytesReadOrWritten;

	NDIS_HANDLE RecvPacketPoolHandle;
	NDIS_HANDLE SendPacketPoolHandle;

	NDIS_HANDLE RecvBufferPoolHandle;
	NDIS_HANDLE SendBufferPoolHandle;
} ADAPTER;

// ========================================================================================================================

typedef struct _RECV_RSVD
{
	PNDIS_PACKET Original;
} RECV_RSVD;

// ========================================================================================================================

typedef struct _SEND_RSVD
{
	PNDIS_PACKET Original;
} SEND_RSVD;

// ========================================================================================================================

typedef struct _QUEUED_PACKET
{
	LIST_ENTRY Link;

	char Type;
	ADAPTER *Adapter;
	PVOID Buffer;
	ULONG Length;
} QUEUED_PACKET;

// ========================================================================================================================