// ========================================================================================================================
// NdisPyFilter
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisPyFilterMiniport.c
//
// Created: 01/09/2007
// ========================================================================================================================

#include <ndis.h>

#include "NdisPyFilter.h"
#include "NdisPyFilterMiniport.h"
#include "NdisPyFilterProtocol.h"

// ========================================================================================================================

VOID MiniportAdapterShutdown(IN PVOID ShutdownContext)
{

}

// ========================================================================================================================

VOID MiniportCancelSendPackets(IN NDIS_HANDLE MiniportAdapterContext, IN PVOID CancelId)
{
	ADAPTER *pAdapter = (ADAPTER *)MiniportAdapterContext;

	NdisCancelSendPackets(pAdapter->BindingHandle, CancelId);
}

// ========================================================================================================================

NDIS_STATUS MiniportInitialize(OUT PNDIS_STATUS OpenErrorStatus,
							   OUT PUINT SelectedMediumIndex,
							   IN PNDIS_MEDIUM MediumArray,
							   IN UINT MediumArraySize,
							   IN NDIS_HANDLE MiniportHandle,
							   IN NDIS_HANDLE WrapperConfigurationContext)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;

	ADAPTER *pAdapter = NdisIMGetDeviceContext(MiniportHandle);
	UINT i = 0;

	do
	{
		pAdapter->MiniportHandle = MiniportHandle;

		//
		// Set Miniport Attributes
		//
		NdisMSetAttributesEx(MiniportHandle,
							 pAdapter,
							 0,
							 NDIS_ATTRIBUTE_IGNORE_PACKET_TIMEOUT |
							 NDIS_ATTRIBUTE_IGNORE_REQUEST_TIMEOUT |
							 NDIS_ATTRIBUTE_INTERMEDIATE_DRIVER |
							 NDIS_ATTRIBUTE_DESERIALIZE |
							 NDIS_ATTRIBUTE_NO_HALT_ON_SUSPEND,
							 0);

		//
		// Set Adapter Medium
		//
		for(i = 0; i < MediumArraySize; ++i)
		{
			if(MediumArray[i] == pAdapter->AdapterMedium)
			{
				*SelectedMediumIndex = i;
				break;
			}
		}

		pAdapter->MiniportDevicePowerState = NdisDeviceStateD0;

		//
		// Add Adapter Instance To Global Adapter List
		//
		NdisAcquireSpinLock(&g_Lock);
		InsertHeadList(&g_AdapterList, &pAdapter->Link);
		NdisReleaseSpinLock(&g_Lock);

		//
		// Register Device Object, If First Miniport
		//
		RegisterDevice();
	}
	while(FALSE);

	*OpenErrorStatus = ndisStatus;
	return ndisStatus;
}

// ========================================================================================================================

VOID MiniportHalt(IN NDIS_HANDLE MiniportAdapterContext)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;

	ADAPTER *pAdapter = (ADAPTER *)MiniportAdapterContext;
	PLIST_ENTRY i;

	//
	// Remove Adapter Instance From Global Adapter List
	//
	NdisAcquireSpinLock(&g_Lock);
	for(i = g_AdapterList.Flink; i != &g_AdapterList; i = i->Flink)
	{
		ADAPTER *pAdapterIter = CONTAINING_RECORD(i, ADAPTER, Link);
		if(pAdapter == pAdapterIter)
		{
			RemoveEntryList(i);
			break;
		}
	}
	NdisReleaseSpinLock(&g_Lock);

	//
	// Deregister Device Object, If Last Miniport
	//
	DeregisterDevice();

	//
	// Close Adapter Handle
	//
	if(pAdapter->BindingHandle != NULL)
	{
		NdisResetEvent(&pAdapter->CompletionEvent);

		NdisCloseAdapter(&ndisStatus, pAdapter->BindingHandle);
		if(ndisStatus == NDIS_STATUS_PENDING)
		{
			NdisWaitEvent(&pAdapter->CompletionEvent, 0);
			ndisStatus = pAdapter->CompletionStatus;
		}

		if(ndisStatus != NDIS_STATUS_SUCCESS)
		{
			DbgPrint("<NdisPyFilter> !! NdisCloseAdapter() Failed [%x].\n", ndisStatus);
		}
	}

	if(pAdapter->RecvPacketPoolHandle != NULL)
	{
		NdisFreePacketPool(pAdapter->RecvPacketPoolHandle);
		pAdapter->RecvPacketPoolHandle = NULL;
	}
	if(pAdapter->SendPacketPoolHandle != NULL)
	{
		NdisFreePacketPool(pAdapter->SendPacketPoolHandle);
		pAdapter->SendPacketPoolHandle = NULL;
	}
	if(pAdapter->RecvBufferPoolHandle != NULL)
	{
		NdisFreeBufferPool(pAdapter->RecvBufferPoolHandle);
		pAdapter->RecvBufferPoolHandle = NULL;
	}
	if(pAdapter->SendBufferPoolHandle != NULL)
	{
		NdisFreeBufferPool(pAdapter->SendBufferPoolHandle);
		pAdapter->SendBufferPoolHandle = NULL;
	}

	NdisFreeSpinLock(&pAdapter->AdapterLock);
	NdisFreeMemory(pAdapter, 0, 0);
}

// ========================================================================================================================

VOID MiniportPnPEventNotify(IN NDIS_HANDLE MiniportAdapterContext,
							IN NDIS_DEVICE_PNP_EVENT PnPEvent,
							IN PVOID InformationBuffer,
							IN ULONG InformationBufferLength)
{

}

// ========================================================================================================================

NDIS_STATUS MiniportQueryInformation(IN NDIS_HANDLE MiniportAdapterContext,
									 IN NDIS_OID Oid,
									 IN PVOID InformationBuffer,
									 IN ULONG InformationBufferLength,
									 OUT PULONG BytesWritten,
									 OUT PULONG BytesNeeded)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_FAILURE;

	ADAPTER *pAdapter = (ADAPTER *)MiniportAdapterContext;

	do
	{
		if(Oid == OID_PNP_QUERY_POWER)
		{
			ndisStatus = NDIS_STATUS_SUCCESS;
			break;
		}
		else if(Oid == OID_GEN_SUPPORTED_GUIDS)
		{
			ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
			break;
		}
		else if(pAdapter->MiniportDevicePowerState > NdisDeviceStateD0)
		{
			ndisStatus = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// Pass Query To Lower Bound Miniport
		//
		pAdapter->Request.RequestType = NdisRequestQueryInformation;
		pAdapter->Request.DATA.QUERY_INFORMATION.Oid = Oid;
		pAdapter->Request.DATA.QUERY_INFORMATION.InformationBuffer = InformationBuffer;
		pAdapter->Request.DATA.QUERY_INFORMATION.InformationBufferLength = InformationBufferLength;
		pAdapter->RequestBytesNeeded = BytesNeeded;
		pAdapter->RequestBytesReadOrWritten = BytesWritten;

		NdisRequest(&ndisStatus,
					pAdapter->BindingHandle,
					&pAdapter->Request);
		if(ndisStatus != NDIS_STATUS_PENDING)
		{
			ProtocolRequestComplete(pAdapter->BindingHandle, &pAdapter->Request, ndisStatus);
			ndisStatus = NDIS_STATUS_PENDING;
		}
	}
	while(FALSE);

	return ndisStatus;
}

// ========================================================================================================================

VOID MiniportReturnPacket(IN NDIS_HANDLE MiniportAdapterContext, IN PNDIS_PACKET Packet)
{
	ADAPTER *pAdapter = (ADAPTER *)MiniportAdapterContext;

	if(NdisGetPoolFromPacket(Packet) != pAdapter->RecvPacketPoolHandle)
	{
		NdisReturnPackets(&Packet, 1);
	}
	else
	{
		PNDIS_PACKET pRetPacket;
		RECV_RSVD *pRecvRsvd;

		pRecvRsvd = (RECV_RSVD *)Packet->MiniportReserved;
		pRetPacket = pRecvRsvd->Original;
		if((pRecvRsvd != NULL) && (pRetPacket != NULL))
		{
			NdisFreePacket(Packet);
			NdisReturnPackets(&pRetPacket, 1);
		}
	}
}

// ========================================================================================================================

VOID MiniportSendPackets(IN NDIS_HANDLE MiniportAdapterContext, IN PPNDIS_PACKET PacketArray, IN UINT NumberOfPackets)
{
	ADAPTER *pAdapter = (ADAPTER *)MiniportAdapterContext;

	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;
	UINT i = 0;

	for(i = 0; i < NumberOfPackets; ++i)
	{
		PNDIS_PACKET pPacket = PacketArray[i];
		PNDIS_PACKET pSendPacket = NULL;
		PNDIS_PACKET_STACK pPacketStack = NULL;
		PVOID packetMediaSpecificInfo = NULL;
		UINT packetMediaSpecificInfoSize = 0;

		BOOLEAN Remaining;

		if(pAdapter->MiniportDevicePowerState > NdisDeviceStateD0)
		{
			NdisMSendComplete(pAdapter->MiniportHandle, pPacket, NDIS_STATUS_FAILURE);
			continue;
		}

		if(g_CtrlHandleCount == 0)
		{
			NdisAllocatePacket(&ndisStatus, &pSendPacket, pAdapter->SendPacketPoolHandle);
			if(ndisStatus == NDIS_STATUS_SUCCESS)
			{
				SEND_RSVD *pSendRsvd;
				pSendRsvd = (SEND_RSVD *)pSendPacket->ProtocolReserved;
				pSendRsvd->Original = pPacket;

				NdisGetPacketFlags(pSendPacket) = NdisGetPacketFlags(pPacket);
				NDIS_PACKET_FIRST_NDIS_BUFFER(pSendPacket) = NDIS_PACKET_FIRST_NDIS_BUFFER(pPacket);
				NDIS_PACKET_LAST_NDIS_BUFFER(pSendPacket) = NDIS_PACKET_LAST_NDIS_BUFFER(pPacket);

				NdisMoveMemory(NDIS_OOB_DATA_FROM_PACKET(pSendPacket),
							   NDIS_OOB_DATA_FROM_PACKET(pPacket),
							   sizeof(NDIS_PACKET_OOB_DATA));

				NdisIMCopySendPerPacketInfo(pSendPacket, pPacket);

				NDIS_GET_PACKET_MEDIA_SPECIFIC_INFO(pPacket,
													&packetMediaSpecificInfo,
													&packetMediaSpecificInfoSize);

				if((packetMediaSpecificInfo != NULL) || (packetMediaSpecificInfoSize != 0))
				{
					NDIS_SET_PACKET_MEDIA_SPECIFIC_INFO(pSendPacket,
														packetMediaSpecificInfo,
														packetMediaSpecificInfoSize);
				}

				NdisSend(&ndisStatus, pAdapter->BindingHandle, pSendPacket);
				if(ndisStatus != NDIS_STATUS_PENDING)
				{
					NdisIMCopySendCompletePerPacketInfo(pPacket, pSendPacket);
					NdisFreePacket(pSendPacket);
				}
			}
		}
		else
		{
			QUEUED_PACKET *pQueuedPacket = NULL;

			if((pQueuedPacket = ExAllocatePoolWithTag(NonPagedPool, sizeof(QUEUED_PACKET), MemoryTag)) == NULL)
			{
				DbgPrint("<NdisPyFilter> !! ExAllocatePoolWithTag(NonPagedPool) Failed\n");
			}
			else
			{
				UINT PhysicalBufferCount = 0;
				UINT BufferCount = 0;
				
				PNDIS_BUFFER PacketBuffer = NULL;
				UINT TotalPacketLength = 0;

				PVOID VirtualAddress = NULL;
				ULONG Length = 0;

				NdisQueryPacket(pPacket, &PhysicalBufferCount, &BufferCount, &PacketBuffer, &TotalPacketLength);	
				
				if((pQueuedPacket->Buffer = ExAllocatePoolWithTag(NonPagedPool, TotalPacketLength, MemoryTag)) == NULL)
				{
					DbgPrint("<NdisPyFilter> !! ExAllocatePoolWithTag(NonPagedPool) Failed\n");
					ExFreePool(pQueuedPacket);
					pQueuedPacket = NULL;
				}
				else
				{
					KIRQL Irql;
					ULONG WrittenCount = 0;
					ULONG j = 0;

					pQueuedPacket->Type = 0x01; // Send
					pQueuedPacket->Adapter = pAdapter;
					pQueuedPacket->Length = TotalPacketLength;
					
					while(PacketBuffer != NULL)
					{
						NdisQueryBufferSafe(PacketBuffer, &VirtualAddress, &Length, NormalPagePriority);
						RtlCopyMemory(((PUCHAR)pQueuedPacket->Buffer) + WrittenCount, VirtualAddress, Length);
						WrittenCount += Length;
						PacketBuffer = PacketBuffer->Next;
					}

					KeAcquireSpinLock(&g_PktSendQueueListLock, &Irql);
					InsertTailList(&g_PktSendQueueList, &pQueuedPacket->Link);
					KeReleaseSpinLock(&g_PktSendQueueListLock, Irql);

					FlushSendPacketQueue();
					ndisStatus = NDIS_STATUS_SUCCESS;
				}
			}
		}

		if(ndisStatus != NDIS_STATUS_PENDING)
		{
			NdisMSendComplete(pAdapter->MiniportHandle, pPacket, ndisStatus);
		}
	}
}

// ========================================================================================================================

NDIS_STATUS MiniportSetInformation(IN NDIS_HANDLE MiniportAdapterContext,
								   IN NDIS_OID Oid,
								   IN PVOID InformationBuffer,
								   IN ULONG InformationBufferLength,
								   OUT PULONG BytesRead,
								   OUT PULONG BytesNeeded)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_FAILURE;

	ADAPTER *pAdapter = (ADAPTER *)MiniportAdapterContext;

	do
	{
		if(Oid == OID_PNP_SET_POWER)
		{
			MiniportInternalProcessSetPowerOid(&ndisStatus,
											   pAdapter,
											   InformationBuffer,
											   InformationBufferLength,
											   BytesRead,
											   BytesNeeded);
			break;
		}

		if(pAdapter->MiniportDevicePowerState > NdisDeviceStateD0)
		{
			ndisStatus = NDIS_STATUS_FAILURE;
			break;
		}
		
		//
		// Pass Set To Lower Bound Miniport
		//
		pAdapter->Request.RequestType = NdisRequestSetInformation;
		pAdapter->Request.DATA.SET_INFORMATION.Oid = Oid;
		pAdapter->Request.DATA.SET_INFORMATION.InformationBuffer = InformationBuffer;
		pAdapter->Request.DATA.SET_INFORMATION.InformationBufferLength = InformationBufferLength;
		pAdapter->RequestBytesNeeded = BytesNeeded;
		pAdapter->RequestBytesReadOrWritten = BytesRead;

		NdisRequest(&ndisStatus, pAdapter->BindingHandle, &pAdapter->Request);
		if(ndisStatus != NDIS_STATUS_PENDING)
		{
			*BytesNeeded = pAdapter->Request.DATA.SET_INFORMATION.BytesNeeded;
			*BytesRead = pAdapter->Request.DATA.SET_INFORMATION.BytesRead;
		}
	}
	while(FALSE);

	return ndisStatus;
}

// ========================================================================================================================

NDIS_STATUS MiniportTransferData(OUT PNDIS_PACKET Packet,
								 OUT PUINT BytesTransferred,
								 IN NDIS_HANDLE MiniportAdapterContext,
								 IN NDIS_HANDLE MiniportReceiveContext,
								 IN UINT ByteOffset,
								 IN UINT BytesToTransfer)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_FAILURE;

	return ndisStatus;
}

// ========================================================================================================================

VOID MiniportInternalProcessSetPowerOid(IN OUT PNDIS_STATUS pStatus,
										IN ADAPTER *pAdapter,
										IN PVOID InformationBuffer,
										IN ULONG InformationBufferLength,
										OUT PULONG BytesRead,
										OUT PULONG BytesNeeded)
{
	NDIS_DEVICE_POWER_STATE NewDevicePowerState;

	*pStatus = NDIS_STATUS_FAILURE;

	do
	{
		if(InformationBufferLength < sizeof(NDIS_DEVICE_POWER_STATE))
		{
			*pStatus = NDIS_STATUS_INVALID_LENGTH;
			break;
		}

		NewDevicePowerState = (*(PNDIS_DEVICE_POWER_STATE)InformationBuffer);
		
		if((pAdapter->MiniportDevicePowerState > NdisDeviceStateD0) && (NewDevicePowerState != NdisDeviceStateD0))
		{
			*pStatus = NDIS_STATUS_FAILURE;
			break;
		}

		pAdapter->MiniportDevicePowerState = NewDevicePowerState;
		*pStatus = NDIS_STATUS_SUCCESS;
	}
	while(FALSE);

	if(*pStatus == NDIS_STATUS_SUCCESS)
	{
		NdisMIndicateStatus(pAdapter->MiniportHandle, NDIS_STATUS_MEDIA_CONNECT, NULL, 0);
		NdisMIndicateStatusComplete(pAdapter->MiniportHandle);

		*BytesRead = sizeof(NDIS_DEVICE_POWER_STATE);
		*BytesNeeded = 0;
	}
	else
	{
		*BytesRead = 0;
		*BytesNeeded = sizeof(NDIS_DEVICE_POWER_STATE);
	}
}

// ========================================================================================================================

VOID MiniportInternalQueryPnPCapabilities(IN OUT ADAPTER *pAdapter, OUT PNDIS_STATUS pStatus)
{
	PNDIS_PNP_CAPABILITIES pPnPCapabilities;
	PNDIS_PM_WAKE_UP_CAPABILITIES pPmWakeUpCapabilities;

	if(pAdapter->Request.DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(NDIS_PNP_CAPABILITIES))
	{
		pPnPCapabilities = (PNDIS_PNP_CAPABILITIES)(pAdapter->Request.DATA.QUERY_INFORMATION.InformationBuffer);
		pPmWakeUpCapabilities = &pPnPCapabilities->WakeUpCapabilities;
		pPmWakeUpCapabilities->MinLinkChangeWakeUp = NdisDeviceStateUnspecified;
		pPmWakeUpCapabilities->MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
		pPmWakeUpCapabilities->MinPatternWakeUp = NdisDeviceStateUnspecified;

		*pAdapter->RequestBytesReadOrWritten = sizeof(NDIS_PNP_CAPABILITIES);
		*pAdapter->RequestBytesNeeded = 0;
		*pStatus = NDIS_STATUS_SUCCESS;
	}
	else
	{
		*pAdapter->RequestBytesNeeded = sizeof(NDIS_PNP_CAPABILITIES);
		*pStatus = NDIS_STATUS_RESOURCES;
	}
}

// ========================================================================================================================

VOID MiniportInternalSendPacket(IN PCHAR Buffer, IN ULONG Length)
{
	ADAPTER *pInAdapter = *((PULONG)Buffer);
	ADAPTER *pAdapter = NULL;

	PLIST_ENTRY i = NULL;

	PCHAR PacketBuffer = NULL;
	ULONG PacketBufferLength = 0;

	NdisAcquireSpinLock(&g_Lock);
	for(i = g_AdapterList.Flink; i != &g_AdapterList; i = i->Flink)
	{
		ADAPTER *pAdapterIter = CONTAINING_RECORD(i, ADAPTER, Link);
		if(pInAdapter == pAdapterIter)
		{
			pAdapter = pInAdapter;
			break;
		}
	}
	NdisReleaseSpinLock(&g_Lock);

	if((pAdapter != NULL) && (Length > 5))
	{
		PNDIS_BUFFER pNdisBuffer = NULL;
		PNDIS_PACKET pNdisPacket = NULL;
		NDIS_STATUS ndisStatus;

		PacketBuffer = Buffer + 5;
		PacketBufferLength = Length - 5;

		NdisAllocatePacket(&ndisStatus, &pNdisPacket, pAdapter->SendPacketPoolHandle);
		if(ndisStatus == NDIS_STATUS_SUCCESS)
		{
			NdisAllocateBuffer(&ndisStatus, &pNdisBuffer, pAdapter->SendBufferPoolHandle, PacketBuffer, PacketBufferLength);
			if(ndisStatus == NDIS_STATUS_SUCCESS)
			{
				((SEND_RSVD *)pNdisPacket->ProtocolReserved)->Original = NULL;

				pNdisBuffer->Next = NULL;
				NdisChainBufferAtFront(pNdisPacket, pNdisBuffer);

				NDIS_SET_PACKET_HEADER_SIZE(pNdisPacket, 14);
				NDIS_SET_PACKET_STATUS(pNdisPacket, NDIS_STATUS_RESOURCES);

				NdisSend(&ndisStatus, pAdapter->BindingHandle, pNdisPacket);
				NdisFreeBuffer(pNdisBuffer);
			}
			else
			{
				DbgPrint("<NdisPyFilter> !! NdisAllocateBuffer() Failed\n");
			}
			NdisFreePacket(pNdisPacket);
		}
		else
		{
			DbgPrint("<NdisPyFilter> !! NdisAllocatePacket() Failed\n");
		}
	}
	else
	{
		DbgPrint("<NdisPyFilter> !! Invalid Adapter Or No Packet Buffer Passed\n");
	}
}

// ========================================================================================================================
