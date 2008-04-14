// ========================================================================================================================
// NdisPyFilter
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisPyFilterProtocol.c
//
// Created: 01/09/2007
// ========================================================================================================================

#include <ndis.h>

#include "NdisPyFilter.h"
#include "NdisPyFilterMiniport.h"
#include "NdisPyFilterProtocol.h"

// ========================================================================================================================

VOID ProtocolBindAdapter(OUT PNDIS_STATUS Status,
				   IN NDIS_HANDLE BindContext,
				   IN PNDIS_STRING DeviceName,
				   IN PVOID SystemSpecific1,
				   IN PVOID SystemSpecific2)
{
	NDIS_HANDLE ndisConfigurationHandle = NULL;
	PNDIS_CONFIGURATION_PARAMETER pNdisConfigurationParameter;
	NDIS_STRING ndisKeyword =  NDIS_STRING_CONST("UpperBindings");

	ADAPTER *pAdapter = NULL;
	UINT adapterSize = 0;

	NDIS_STATUS openErrorStatus = 0;
	UINT mediumIndex = 0;

	do
	{
		//
		// Open & Read Adapter Configuration
		//
		NdisOpenProtocolConfiguration(Status, &ndisConfigurationHandle, SystemSpecific1);
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			DbgPrint("<NdisPyFilter> !! NdisOpenProtocolConfiguration() Failed [%x].\n", *Status);
			break;
		}

		NdisReadConfiguration(Status,
							  &pNdisConfigurationParameter,
							  ndisConfigurationHandle,
							  &ndisKeyword,
							  NdisParameterString);
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			DbgPrint("<NdisPyFilter> !! NdisReadConfiguration() Failed [%x].\n", *Status);
			break;
		}

		//
		// Initialize Adapter Structure
		//
		adapterSize = sizeof(ADAPTER) + pNdisConfigurationParameter->ParameterData.StringData.MaximumLength;
		NdisAllocateMemoryWithTag(&pAdapter,
								  adapterSize,
								  MemoryTag);
		if(pAdapter == NULL)
		{
			*Status = NDIS_STATUS_RESOURCES;
			break;
		}

		NdisZeroMemory(pAdapter, adapterSize);
		pAdapter->AdapterDeviceName.Buffer = (PWCHAR)((ULONG_PTR)pAdapter + sizeof(ADAPTER));
		pAdapter->AdapterDeviceName.Length = pNdisConfigurationParameter->ParameterData.StringData.Length;
		pAdapter->AdapterDeviceName.MaximumLength = pNdisConfigurationParameter->ParameterData.StringData.MaximumLength;
		NdisMoveMemory(pAdapter->AdapterDeviceName.Buffer,
					   pNdisConfigurationParameter->ParameterData.StringData.Buffer,
					   pNdisConfigurationParameter->ParameterData.StringData.MaximumLength);

		pAdapter->ProtocolDevicePowerState = NdisDeviceStateD0;

		NdisAllocateSpinLock(&pAdapter->AdapterLock);
		NdisInitializeEvent(&pAdapter->CompletionEvent);

		//
		// Allocate Packet Pools
		//
		NdisAllocatePacketPoolEx(Status,
								 &pAdapter->RecvPacketPoolHandle,
								 MinPacketPoolSize,
								 MaxPacketPoolSize - MinPacketPoolSize,
								 PROTOCOL_RESERVED_SIZE_IN_PACKET);
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			DbgPrint("<NdisPyFilter> !! NdisAllocatePacketPoolEx() Failed [%x].\n", *Status);
			break;
		}

		NdisAllocatePacketPoolEx(Status,
								 &pAdapter->SendPacketPoolHandle,
								 MinPacketPoolSize,
								 MaxPacketPoolSize - MinPacketPoolSize,
								 sizeof(SEND_RSVD));
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			DbgPrint("<NdisPyFilter> !! NdisAllocatePacketPoolEx() Failed [%x].\n", *Status);
			break;
		}

		//
		// Allocate Buffer Pools
		//

		NdisAllocateBufferPool(Status, &pAdapter->RecvBufferPoolHandle, MinPacketPoolSize);
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			DbgPrint("<NdisPyFilter> !! NdisAllocateBufferPool() Failed [%x].\n", *Status);
			break;
		}

		NdisAllocateBufferPool(Status, &pAdapter->SendBufferPoolHandle, MinPacketPoolSize);
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			DbgPrint("<NdisPyFilter> !! NdisAllocateBufferPool() Failed [%x].\n", *Status);
			break;
		}

		//
		// Open Adapter
		//
		NdisOpenAdapter(Status,
						&openErrorStatus,
						&pAdapter->BindingHandle,
						&mediumIndex,
						SupportedMediumArray,
						sizeof(SupportedMediumArray) / sizeof(NDIS_MEDIUM),
						g_NdisProtocolHandle,
						pAdapter,
						DeviceName,
						0,
						NULL);
		if(*Status == NDIS_STATUS_PENDING)
		{
			NdisWaitEvent(&pAdapter->CompletionEvent, 0);
			*Status = pAdapter->CompletionStatus;
		}
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			DbgPrint("<NdisPyFilter> !! NdisOpenAdapter() Failed [%x].\n", *Status);
			break;
		}

		pAdapter->AdapterMedium = SupportedMediumArray[mediumIndex];

		//
		// Initialize Device Instance
		//
		*Status = NdisIMInitializeDeviceInstanceEx(g_NdisDriverHandle, &pAdapter->AdapterDeviceName, pAdapter);
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			DbgPrint("<NdisPyFilter> !! NdisIMInitializeDeviceInstanceEx() Failed [%x].\n", *Status);
			break;
		}
	}
	while(FALSE);

	if(ndisConfigurationHandle != NULL)
	{
		NdisCloseConfiguration(ndisConfigurationHandle);
		ndisConfigurationHandle = NULL;
	}
}

// ========================================================================================================================

VOID ProtocolCloseAdapterComplete(IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;

	//
	// Complete Adapter Close Operation
	//
	pAdapter->CompletionStatus = Status;
	NdisSetEvent(&pAdapter->CompletionEvent);
}

// ========================================================================================================================

VOID ProtocolOpenAdapterComplete(IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status, IN NDIS_STATUS OpenErrorStatus)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;

	//
	// Complete Adapter Open Operation
	//
	pAdapter->CompletionStatus = Status;
	NdisSetEvent(&pAdapter->CompletionEvent);
}

// ========================================================================================================================

NDIS_STATUS ProtocolPnPEvent(IN NDIS_HANDLE ProtocolBindingContext, IN PNET_PNP_EVENT NetPnPEvent)
{
	return NDIS_STATUS_SUCCESS;
}

// ========================================================================================================================

VOID ProtocolReceiveComplete(IN NDIS_HANDLE ProtocolBindingContext)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;

	NdisMEthIndicateReceiveComplete(pAdapter->MiniportHandle);
}

// ========================================================================================================================

NDIS_STATUS ProtocolReceive(IN NDIS_HANDLE ProtocolBindingContext,
							IN NDIS_HANDLE MacReceiveContext,
							IN PVOID HeaderBuffer,
							IN UINT HeaderBufferSize,
							IN PVOID LookAheadBuffer,
							IN UINT LookAheadBufferSize,
							IN UINT PacketSize)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;

	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;

	if((pAdapter->MiniportHandle == NULL) || (pAdapter->MiniportDevicePowerState > NdisDeviceStateD0))
	{
		ndisStatus = NDIS_STATUS_FAILURE;
	}
	else
	{
		PNDIS_PACKET pPacket = NdisGetReceivedPacket(pAdapter->BindingHandle, MacReceiveContext);
		if(pPacket != NULL)
		{
			NdisMEthIndicateReceive(pAdapter->MiniportHandle,
									MacReceiveContext,
									HeaderBuffer,
									HeaderBufferSize,
									LookAheadBuffer,
									LookAheadBufferSize,
									PacketSize);
		}
	}

	return ndisStatus;
}

// ========================================================================================================================

INT ProtocolReceivePacket(IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_PACKET Packet)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;
	
	if((pAdapter->MiniportHandle == NULL) || (pAdapter->MiniportDevicePowerState > NdisDeviceStateD0))
	{
		return 0;
	}

	if(g_CtrlHandleCount == 0)
	{
		NDIS_STATUS ndisStatus;
		PNDIS_PACKET pRecvPacket;

		NdisAllocatePacket(&ndisStatus, &pRecvPacket, pAdapter->RecvPacketPoolHandle);
		if(ndisStatus == NDIS_STATUS_SUCCESS)
		{
			((RECV_RSVD *)pRecvPacket->MiniportReserved)->Original = NULL;
			
			NDIS_PACKET_FIRST_NDIS_BUFFER(pRecvPacket) = NDIS_PACKET_FIRST_NDIS_BUFFER(Packet);
			
			NdisGetPacketFlags(pRecvPacket) = NdisGetPacketFlags(Packet);
			NDIS_SET_PACKET_STATUS(pRecvPacket, NDIS_STATUS_RESOURCES);

			NdisMIndicateReceivePacket(pAdapter->MiniportHandle, &pRecvPacket, 1);
			NdisFreePacket(pRecvPacket);
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
			PNDIS_BUFFER FirstBuffer = NULL;
			UINT TotalPacketLength = 0;
			PVOID VirtualAddress = NULL;
			ULONG Length = 0;

			NdisQueryPacket(Packet, &PhysicalBufferCount, &BufferCount, &FirstBuffer, &TotalPacketLength);	
			NdisQueryBufferSafe(FirstBuffer, &VirtualAddress, &Length, NormalPagePriority);

			if(VirtualAddress == NULL)
			{
				DbgPrint("<NdisPyFilter> !! NdisQueryBufferSafe() Failed\n");
			}
			else
			{
				if((pQueuedPacket->Buffer = ExAllocatePoolWithTag(NonPagedPool, Length, MemoryTag)) == NULL)
				{
					DbgPrint("<NdisPyFilter> !! ExAllocatePoolWithTag(NonPagedPool) Failed\n");
					ExFreePool(pQueuedPacket);
					pQueuedPacket = NULL;
				}
				else
				{
					KIRQL Irql;

					pQueuedPacket->Type = 0x00; // Recv
					pQueuedPacket->Adapter = pAdapter;
					pQueuedPacket->Length = Length;
					RtlCopyMemory(pQueuedPacket->Buffer, VirtualAddress, Length);

					KeAcquireSpinLock(&g_PktRecvQueueListLock, &Irql);
					InsertTailList(&g_PktRecvQueueList, &pQueuedPacket->Link);
					KeReleaseSpinLock(&g_PktRecvQueueListLock, Irql);

					NdisMEthIndicateReceiveComplete(pAdapter->MiniportHandle);
					FlushRecvPacketQueue();
				}
			}
		}
	}
	return 0;
}

// ========================================================================================================================

VOID ProtocolRequestComplete(IN NDIS_HANDLE ProtocolBindingContext,
							 IN PNDIS_REQUEST NdisRequest,
							 IN NDIS_STATUS Status)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;
	NDIS_OID Oid = pAdapter->Request.DATA.SET_INFORMATION.Oid;

	switch(NdisRequest->RequestType)
	{
		case NdisRequestQueryInformation:
			if((Oid == OID_PNP_CAPABILITIES) && (Status == NDIS_STATUS_SUCCESS))
			{
				MiniportInternalQueryPnPCapabilities(pAdapter, &Status);
			}
			
			*pAdapter->RequestBytesNeeded = NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded;
			*pAdapter->RequestBytesReadOrWritten = NdisRequest->DATA.QUERY_INFORMATION.BytesWritten;
			
			if((Oid == OID_GEN_MAC_OPTIONS) && (Status == NDIS_STATUS_SUCCESS))
			{
				*((PULONG)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer) &= ~NDIS_MAC_OPTION_NO_LOOPBACK;
			}

			NdisMQueryInformationComplete(pAdapter->MiniportHandle, Status);
			break;

		case NdisRequestSetInformation:
			*pAdapter->RequestBytesNeeded = NdisRequest->DATA.SET_INFORMATION.BytesNeeded;
			*pAdapter->RequestBytesReadOrWritten = NdisRequest->DATA.SET_INFORMATION.BytesRead;

			NdisMSetInformationComplete(pAdapter->MiniportHandle, Status);
			break;
	}
}

// ========================================================================================================================

VOID ProtocolResetComplete(IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status)
{

}

// ========================================================================================================================

VOID ProtocolSendComplete(IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_PACKET Packet, IN NDIS_STATUS Status)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;

	NDIS_HANDLE PoolHandle;
	
	PoolHandle = NdisGetPoolFromPacket(Packet);
	if(PoolHandle != pAdapter->SendPacketPoolHandle)
	{
		NdisMSendComplete(pAdapter->MiniportHandle, Packet, Status);
	}
	else
	{
		SEND_RSVD *pSendRsvd = (SEND_RSVD *)Packet->ProtocolReserved;
		PNDIS_PACKET pSentPacket = pSendRsvd->Original;
		if((pSendRsvd != NULL) && (pSentPacket != NULL))
		{
			NdisIMCopySendCompletePerPacketInfo(pSentPacket, Packet);
			NdisDprFreePacket(Packet);
			NdisMSendComplete(pAdapter->MiniportHandle, pSentPacket, Status);
		}
	}
}

// ========================================================================================================================

VOID ProtocolStatus(IN NDIS_HANDLE ProtocolBindingContext,
			  IN NDIS_STATUS GeneralStatus,
			  IN PVOID StatusBuffer,
			  IN UINT StatusBufferSize)
{

}

// ========================================================================================================================

VOID ProtocolStatusComplete(IN NDIS_HANDLE ProtocolBindingContext)
{

}

// ========================================================================================================================

VOID ProtocolTransferDataComplete(IN NDIS_HANDLE ProtocolBindingContext,
							IN PNDIS_PACKET Packet,
							IN NDIS_STATUS Status,
							IN UINT BytesTransferred)
{

}

// ========================================================================================================================

VOID ProtocolUnbindAdapter(OUT PNDIS_STATUS Status, IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_HANDLE UnbindContext)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;

	if(pAdapter->MiniportHandle != NULL)
	{
		*Status = NdisIMDeInitializeDeviceInstance(pAdapter->MiniportHandle);
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			*Status = NDIS_STATUS_FAILURE;
		}
	}
	else
	{
		//
		// Close Adapter Handle
		//
		if(pAdapter->BindingHandle != NULL)
		{
			NdisResetEvent(&pAdapter->CompletionEvent);

			NdisCloseAdapter(Status, pAdapter->BindingHandle);
			if(*Status == NDIS_STATUS_PENDING)
			{
				NdisWaitEvent(&pAdapter->CompletionEvent, 0);
				*Status = pAdapter->CompletionStatus;
			}

			if(*Status != NDIS_STATUS_SUCCESS)
			{

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

		NdisFreeSpinLock(&pAdapter->AdapterLock);
		NdisFreeMemory(pAdapter, 0, 0);
	}
}

// ========================================================================================================================

VOID ProtocolUnload(VOID)
{
	if(g_NdisProtocolHandle != NULL)
	{
		NDIS_STATUS ndisStatus;
		NdisDeregisterProtocol(&ndisStatus, g_NdisProtocolHandle);
		g_NdisProtocolHandle = NULL;
	}
}

// ========================================================================================================================

VOID ProtocolInternalIndicatePacket(IN PCHAR Buffer, IN ULONG Length)
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

		NdisAllocatePacket(&ndisStatus, &pNdisPacket, pAdapter->RecvPacketPoolHandle);
		if(ndisStatus == NDIS_STATUS_SUCCESS)
		{
			NdisAllocateBuffer(&ndisStatus, &pNdisBuffer, pAdapter->RecvBufferPoolHandle, PacketBuffer, PacketBufferLength);
			if(ndisStatus == NDIS_STATUS_SUCCESS)
			{
				((RECV_RSVD *)pNdisPacket->MiniportReserved)->Original = NULL;

				pNdisBuffer->Next = NULL;
				NdisChainBufferAtFront(pNdisPacket, pNdisBuffer);

				NDIS_SET_PACKET_HEADER_SIZE(pNdisPacket, 14);
				NDIS_SET_PACKET_STATUS(pNdisPacket, NDIS_STATUS_RESOURCES);

				NdisMIndicateReceivePacket(pAdapter->MiniportHandle, &pNdisPacket, 1);
				NdisMEthIndicateReceiveComplete(pAdapter->MiniportHandle);
				
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
