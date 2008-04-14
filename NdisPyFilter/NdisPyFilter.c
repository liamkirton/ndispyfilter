// ========================================================================================================================
// NdisPyFilter
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisPyFilter.c
//
// Created: 01/09/2007
// ========================================================================================================================

#include <ndis.h>

#include "NdisPyFilter.h"
#include "NdisPyFilterMiniport.h"
#include "NdisPyFilterProtocol.h"

// ========================================================================================================================

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath);
VOID DriverUnload(IN PDRIVER_OBJECT pDriverObject);

#pragma NDIS_INIT_FUNCTION(DriverEntry)

// ========================================================================================================================

NDIS_HANDLE g_NdisDeviceHandle;
NDIS_HANDLE g_NdisDriverHandle;
NDIS_HANDLE g_NdisProtocolHandle;
NDIS_HANDLE g_NdisWrapperHandle;
PDEVICE_OBJECT g_ControlDeviceObject;

NDIS_SPIN_LOCK g_Lock;
KSPIN_LOCK g_IoReadQueueListLock;
KSPIN_LOCK g_PktRecvQueueListLock;
KSPIN_LOCK g_PktSendQueueListLock;

IO_CSQ g_IoCsReadQueue;

LIST_ENTRY g_AdapterList;
LIST_ENTRY g_IoReadQueueList;
LIST_ENTRY g_PktRecvQueueList;
LIST_ENTRY g_PktSendQueueList;

ULONG g_CtrlHandleCount = 0;
ULONG g_MiniportCount = 0;

// ========================================================================================================================

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;

	NDIS_MINIPORT_CHARACTERISTICS ndisMiniportCharacteristics;
	NDIS_PROTOCOL_CHARACTERISTICS ndisProtocolCharacteristics;
	NDIS_STRING ndisProtocolName;

	DbgPrint("NdisPyFilter 0.1.1\n");
	DbgPrint("\n");
	DbgPrint("Copyright (C)2007 Liam Kirton <liam@int3.ws>\n");
	DbgPrint("\n");
	DbgPrint("Built at %s on %s\n", __TIME__, __DATE__);
	DbgPrint("\n");

	//
	// Initialize Globals
	//
	InitializeListHead(&g_AdapterList);
	InitializeListHead(&g_IoReadQueueList);
	InitializeListHead(&g_PktRecvQueueList);
	InitializeListHead(&g_PktSendQueueList);
	
	NdisAllocateSpinLock(&g_Lock);
	KeInitializeSpinLock(&g_IoReadQueueListLock);
	KeInitializeSpinLock(&g_PktSendQueueListLock);
	
	NdisMInitializeWrapper(&g_NdisWrapperHandle, pDriverObject, pRegistryPath, NULL);
	NdisMRegisterUnloadHandler(g_NdisWrapperHandle, DriverUnload);

	IoCsqInitialize(&g_IoCsReadQueue,
					CsReadQueueInsertIrp,
					CsReadQueueRemoveIrp,
					CsReadQueuePeekNextIrp,
					CsReadQueueAcquireLock,
					CsReadQueueReleaseLock,
					CsReadQueueCompleteCanceledIrp);

	do
	{
		//
		// Register Miniport With NDIS
		//
		NdisZeroMemory(&ndisMiniportCharacteristics, sizeof(NDIS_MINIPORT_CHARACTERISTICS));
		ndisMiniportCharacteristics.MajorNdisVersion = 5;
		ndisMiniportCharacteristics.MinorNdisVersion = 1;
		
		ndisMiniportCharacteristics.AdapterShutdownHandler = MiniportAdapterShutdown;
		ndisMiniportCharacteristics.CancelSendPacketsHandler = MiniportCancelSendPackets;
		ndisMiniportCharacteristics.CheckForHangHandler = NULL;
		ndisMiniportCharacteristics.HaltHandler = MiniportHalt;
		ndisMiniportCharacteristics.InitializeHandler = MiniportInitialize;
		ndisMiniportCharacteristics.PnPEventNotifyHandler = MiniportPnPEventNotify;
		ndisMiniportCharacteristics.QueryInformationHandler = MiniportQueryInformation;
		ndisMiniportCharacteristics.ResetHandler = NULL;
		ndisMiniportCharacteristics.ReturnPacketHandler = MiniportReturnPacket;
		ndisMiniportCharacteristics.SendHandler = NULL;
		ndisMiniportCharacteristics.SendPacketsHandler = MiniportSendPackets;
		ndisMiniportCharacteristics.SetInformationHandler = MiniportSetInformation;
		ndisMiniportCharacteristics.TransferDataHandler = MiniportTransferData;

		ndisStatus = NdisIMRegisterLayeredMiniport(g_NdisWrapperHandle,
												   &ndisMiniportCharacteristics,
												   sizeof(NDIS_MINIPORT_CHARACTERISTICS),
												   &g_NdisDriverHandle);
		if(ndisStatus != NDIS_STATUS_SUCCESS)
		{
			DbgPrint("<NdisPyFilter> !! NdisIMRegisterLayeredMiniport() Failed [%x].\n", ndisStatus);
			break;
		}

		//
		// Register Protocol With NDIS
		//
		NdisZeroMemory(&ndisProtocolCharacteristics, sizeof(NDIS_PROTOCOL_CHARACTERISTICS));
		ndisProtocolCharacteristics.MajorNdisVersion = 5;
		ndisProtocolCharacteristics.MinorNdisVersion = 1;

		NdisInitUnicodeString(&ndisProtocolName, L"NdisPyFilter");
		ndisProtocolCharacteristics.Name = ndisProtocolName;

		ndisProtocolCharacteristics.BindAdapterHandler = ProtocolBindAdapter;
		ndisProtocolCharacteristics.CloseAdapterCompleteHandler = ProtocolCloseAdapterComplete;
		ndisProtocolCharacteristics.OpenAdapterCompleteHandler = ProtocolOpenAdapterComplete;
		ndisProtocolCharacteristics.PnPEventHandler = ProtocolPnPEvent;
		ndisProtocolCharacteristics.ReceiveHandler = ProtocolReceive;
		ndisProtocolCharacteristics.ReceiveCompleteHandler = ProtocolReceiveComplete;
		ndisProtocolCharacteristics.ReceivePacketHandler = ProtocolReceivePacket;
		ndisProtocolCharacteristics.RequestCompleteHandler = ProtocolRequestComplete;
		ndisProtocolCharacteristics.ResetCompleteHandler = ProtocolResetComplete;
		ndisProtocolCharacteristics.SendCompleteHandler = ProtocolSendComplete;
		ndisProtocolCharacteristics.StatusHandler = ProtocolStatus;
		ndisProtocolCharacteristics.StatusCompleteHandler = ProtocolStatusComplete;
		ndisProtocolCharacteristics.TransferDataCompleteHandler = ProtocolTransferDataComplete;
		ndisProtocolCharacteristics.UnbindAdapterHandler = ProtocolUnbindAdapter;
		ndisProtocolCharacteristics.UnloadHandler = ProtocolUnload;

		NdisRegisterProtocol(&ndisStatus,
							 &g_NdisProtocolHandle,
							 &ndisProtocolCharacteristics,
							 sizeof(NDIS_PROTOCOL_CHARACTERISTICS));
		if(ndisStatus != NDIS_STATUS_SUCCESS)
		{
			DbgPrint("<NdisPyFilter> !! NdisRegisterProtocol() Failed [%x].\n", ndisStatus);

			NdisIMDeregisterLayeredMiniport(&g_NdisDriverHandle);
			break;
		}

		NdisIMAssociateMiniport(g_NdisDriverHandle, &g_NdisProtocolHandle);
	}
	while(FALSE);

	if(ndisStatus != NDIS_STATUS_SUCCESS)
	{
		NdisTerminateWrapper(g_NdisWrapperHandle, NULL);
	}

	return ndisStatus;
}

// ========================================================================================================================

VOID DriverUnload(IN PDRIVER_OBJECT pDriverObject)
{
	//
	// Deregister Protocol
	//
	ProtocolUnload();

	//
	// Deregister Miniport
	//
	NdisIMDeregisterLayeredMiniport(g_NdisDriverHandle);

	//
	// Destroy Globals
	//
	NdisFreeSpinLock(&g_Lock);
	NdisFreeSpinLock(&g_PktRecvQueueListLock);
	NdisFreeSpinLock(&g_PktSendQueueListLock);
}

// ========================================================================================================================

NDIS_STATUS RegisterDevice(VOID)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;
	PDRIVER_DISPATCH dispatchTable[IRP_MJ_MAXIMUM_FUNCTION + 1];
	UNICODE_STRING deviceName;
	UNICODE_STRING deviceLink;
	
	BOOLEAN bRegisterDevice = FALSE;

	NdisAcquireSpinLock(&g_Lock);
	bRegisterDevice = (++g_MiniportCount == 1);
	NdisReleaseSpinLock(&g_Lock);

	if(bRegisterDevice)
	{
		//
		// Register Device Object
		//
		NdisZeroMemory(&dispatchTable, sizeof(dispatchTable));
		dispatchTable[IRP_MJ_CREATE] = DeviceDispatch;
		dispatchTable[IRP_MJ_CLEANUP] = DeviceDispatch;
		dispatchTable[IRP_MJ_CLOSE] = DeviceDispatch;
		dispatchTable[IRP_MJ_DEVICE_CONTROL] = DeviceDispatch;
		dispatchTable[IRP_MJ_READ] = DeviceReadDispatch;
		dispatchTable[IRP_MJ_WRITE] = DeviceWriteDispatch;

		NdisInitUnicodeString(&deviceName, L"\\DosDevices\\NdisPyFilter");
		NdisInitUnicodeString(&deviceLink, L"\\Device\\NdisPyFilter");

		ndisStatus = NdisMRegisterDevice(g_NdisWrapperHandle,
										 &deviceName,
										 &deviceLink,
										 &dispatchTable[0],
										 &g_ControlDeviceObject,
										 &g_NdisDeviceHandle);
		if(ndisStatus != NDIS_STATUS_SUCCESS)
		{
			DbgPrint("<NdisPyFilter> !! NdisMRegisterDevice() Failed [%x].\n", ndisStatus);
		}

		g_ControlDeviceObject->Flags |= DO_DIRECT_IO;
	}

	return ndisStatus;
}

// ========================================================================================================================

NDIS_STATUS DeregisterDevice(VOID)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;

	NdisAcquireSpinLock(&g_Lock);

	if(--g_MiniportCount == 0)
	{
		//
		// Deregister Device Object
		//
		if(g_NdisDeviceHandle != NULL)
		{
			ndisStatus = NdisMDeregisterDevice(g_NdisDeviceHandle);
			if(ndisStatus != NDIS_STATUS_SUCCESS)
			{
				DbgPrint("<NdisPyFilter> !! NdisMDeregisterDevice() Failed [%x].\n", ndisStatus);
			}
			g_NdisDeviceHandle = NULL;
		}
	}

	NdisReleaseSpinLock(&g_Lock);

	return ndisStatus;
}

// ========================================================================================================================

NTSTATUS DeviceDispatch(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PIO_STACK_LOCATION pIrpStack;

	PIRP pCancelIrp = NULL;

	//
	// Handle IRP
	//
	pIrpStack = IoGetCurrentIrpStackLocation(pIrp);

	switch(pIrpStack->MajorFunction)
	{
		case IRP_MJ_CREATE:
			InterlockedIncrement(&g_CtrlHandleCount);
			break;

		case IRP_MJ_CLEANUP:
			break;

		case IRP_MJ_CLOSE:
			InterlockedDecrement(&g_CtrlHandleCount);

			while(pCancelIrp = IoCsqRemoveNextIrp(&g_IoCsReadQueue, NULL))
			{
				pCancelIrp->IoStatus.Information = 0;
				pCancelIrp->IoStatus.Status = STATUS_CANCELLED;
				IoCompleteRequest(pCancelIrp, IO_NO_INCREMENT);
			}
			break;

		case IRP_MJ_DEVICE_CONTROL:
			ntStatus = DeviceIoControlDispatch(pIrp, pIrpStack);
			break;
	}

	pIrp->IoStatus.Status = ntStatus;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return ntStatus;
}

// ========================================================================================================================

NTSTATUS DeviceIoControlDispatch(IN PIRP pIrp, IN PIO_STACK_LOCATION pIrpStack)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	return ntStatus;
}

// ========================================================================================================================

NTSTATUS DeviceReadDispatch(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp)
{
	static int Switch = 0;

	IoCsqInsertIrp(&g_IoCsReadQueue, pIrp, NULL);
	
	if((Switch++ % 2) == 0)
	{
		FlushRecvPacketQueue();
		FlushSendPacketQueue();
	}
	else
	{
		FlushSendPacketQueue();
		FlushRecvPacketQueue();
	}
	
	return STATUS_PENDING;
}

// ========================================================================================================================

NTSTATUS DeviceWriteDispatch(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PIO_STACK_LOCATION pIrpStack = NULL;

	PUCHAR pUserBuffer = NULL;
	ULONG UserBufferLength = 0;

	pUserBuffer = (PUCHAR)MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority);
	if(pUserBuffer != NULL)
	{
		pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
		UserBufferLength = pIrpStack->Parameters.Read.Length;

		if(UserBufferLength > 5)
		{
			if(pUserBuffer[4] == 0x00)
			{
				ProtocolInternalIndicatePacket(pUserBuffer, UserBufferLength);
			}
			else if(pUserBuffer[4] == 0x01)
			{
				MiniportInternalSendPacket(pUserBuffer, UserBufferLength);
			}
		}
	}

	pIrp->IoStatus.Information = UserBufferLength;
	pIrp->IoStatus.Status = ntStatus;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return ntStatus;
}

// ========================================================================================================================

VOID FlushRecvPacketQueue()
{
	while(TRUE)
	{
		KIRQL Irql;

		PIO_STACK_LOCATION pIrpStack = NULL;
		PIRP pIrp = NULL;

		QUEUED_PACKET *pQueuedPacket = NULL;
		
		PUCHAR pUserBuffer = NULL;
		ULONG UserBufferLength = 0;

		KeAcquireSpinLock(&g_PktRecvQueueListLock, &Irql);
		if(g_PktRecvQueueList.Flink != &g_PktRecvQueueList)
		{
			if((pIrp = IoCsqRemoveNextIrp(&g_IoCsReadQueue, NULL)) != NULL)
			{
				pQueuedPacket = CONTAINING_RECORD(g_PktRecvQueueList.Flink, QUEUED_PACKET, Link);
				RemoveEntryList(g_PktRecvQueueList.Flink);
			}
		}
		KeReleaseSpinLock(&g_PktRecvQueueListLock, Irql);

		if((pIrp == NULL) || (pQueuedPacket == NULL))
		{
			break;
		}

		pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
		pIrp->IoStatus.Information = 0;
		pIrp->IoStatus.Status = STATUS_SUCCESS;

		pUserBuffer = (PUCHAR)MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority);
		if(pUserBuffer != NULL)
		{
			UserBufferLength = pIrpStack->Parameters.Read.Length;
			pIrp->IoStatus.Information = min(pQueuedPacket->Length + 5, UserBufferLength);
			RtlFillMemory(pUserBuffer, UserBufferLength, 0x00);

			*((PULONG)pUserBuffer) = (ULONG)pQueuedPacket->Adapter;
			pUserBuffer[4] = 0x00; // Recv
			RtlCopyMemory(pUserBuffer + 5, pQueuedPacket->Buffer, pIrp->IoStatus.Information - 5);
		}

		IoCompleteRequest(pIrp, IO_NO_INCREMENT);

		ExFreePoolWithTag(pQueuedPacket->Buffer, MemoryTag);
		ExFreePoolWithTag(pQueuedPacket, MemoryTag);
	}
}

// ========================================================================================================================

VOID FlushSendPacketQueue()
{
	while(TRUE)
	{
		KIRQL Irql;

		PIO_STACK_LOCATION pIrpStack = NULL;
		PIRP pIrp = NULL;

		QUEUED_PACKET *pQueuedPacket = NULL;
		
		PUCHAR pUserBuffer = NULL;
		ULONG UserBufferLength = 0;

		KeAcquireSpinLock(&g_PktSendQueueListLock, &Irql);
		if(g_PktSendQueueList.Flink != &g_PktSendQueueList)
		{
			if((pIrp = IoCsqRemoveNextIrp(&g_IoCsReadQueue, NULL)) != NULL)
			{
				pQueuedPacket = CONTAINING_RECORD(g_PktSendQueueList.Flink, QUEUED_PACKET, Link);
				RemoveEntryList(g_PktSendQueueList.Flink);
			}
		}
		KeReleaseSpinLock(&g_PktSendQueueListLock, Irql);

		if((pIrp == NULL) || (pQueuedPacket == NULL))
		{
			break;
		}

		pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
		pIrp->IoStatus.Information = 0;
		pIrp->IoStatus.Status = STATUS_SUCCESS;

		pUserBuffer = (PUCHAR)MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority);
		if(pUserBuffer != NULL)
		{
			UserBufferLength = pIrpStack->Parameters.Read.Length;
			pIrp->IoStatus.Information = min(pQueuedPacket->Length + 5, UserBufferLength);
			RtlFillMemory(pUserBuffer, UserBufferLength, 0x00);

			*((PULONG)pUserBuffer) = (ULONG)pQueuedPacket->Adapter;
			pUserBuffer[4] = 0x01; // Send
			RtlCopyMemory(pUserBuffer + 5, pQueuedPacket->Buffer, pIrp->IoStatus.Information - 5);
		}

		IoCompleteRequest(pIrp, IO_NO_INCREMENT);

		ExFreePoolWithTag(pQueuedPacket->Buffer, MemoryTag);
		ExFreePoolWithTag(pQueuedPacket, MemoryTag);
	}
}

// ========================================================================================================================

VOID CsReadQueueInsertIrp(IN PIO_CSQ pCsq, IN PIRP pIrp)
{
	InsertTailList(&g_IoReadQueueList, &pIrp->Tail.Overlay.ListEntry);
}

// ========================================================================================================================

VOID CsReadQueueRemoveIrp(IN PIO_CSQ pCsq, IN PIRP Irp)
{
	RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
}

// ========================================================================================================================

PIRP CsReadQueuePeekNextIrp(IN PIO_CSQ pCsq, IN PIRP pIrp, IN PVOID pPeekContext)
{
	PIRP pNextIrp = NULL;
	PIO_STACK_LOCATION pIrpStack = NULL;

	PLIST_ENTRY pListHead = &g_IoReadQueueList;
	PLIST_ENTRY pNextEntry = NULL;

	if(pIrp == NULL)
	{
		pNextEntry = pListHead->Flink;
	}
	else
	{
		pNextEntry = pIrp->Tail.Overlay.ListEntry.Flink;
	}

	while(pNextEntry != pListHead)
	{
		pNextIrp = CONTAINING_RECORD(pNextEntry, IRP, Tail.Overlay.ListEntry);

		if(pPeekContext)
		{
			pIrpStack = IoGetCurrentIrpStackLocation(pNextIrp);

			if(pIrpStack->FileObject == (PFILE_OBJECT)pPeekContext)
			{
				break;
			}
		}
		else
		{
			break;
		}

		pNextIrp = NULL;
		pNextEntry = pNextEntry->Flink;
	}

	return pNextIrp;
}

// ========================================================================================================================

VOID CsReadQueueAcquireLock(IN PIO_CSQ pCsq, OUT PKIRQL pIrql)
{
	KeAcquireSpinLock(&g_IoReadQueueListLock, pIrql);
}

// ========================================================================================================================

VOID CsReadQueueReleaseLock(IN PIO_CSQ pCsq, IN KIRQL Irql)
{
	KeReleaseSpinLock(&g_IoReadQueueListLock, Irql);
}

// ========================================================================================================================

VOID CsReadQueueCompleteCanceledIrp(IN PIO_CSQ pCsq, IN PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_CANCELLED;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
}

// ========================================================================================================================
