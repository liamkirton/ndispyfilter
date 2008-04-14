// ========================================================================================================================
// NdisPyFilterCtrl
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisPyFilterCtrl.cpp
//
// Created: 01/09/2007
// ========================================================================================================================

#include <windows.h>

#include <iostream>
#include <string>

#include "NdisPyFilterCtrl.h"

#include "PyInstance.h"

// ========================================================================================================================

BOOL WINAPI CtrlHandlerRoutine(DWORD dwCtrlType);
DWORD WINAPI NdisPyFilterThread(LPVOID lpParameter);

void FixupPacket(u_char *buffer, u_int length);
void PrintUsage();

// ========================================================================================================================

const char *g_cNdisPyFilterCtrlVersion = "0.1.1";

// ========================================================================================================================

std::string g_FilterPath;

CRITICAL_SECTION g_ConsoleCriticalSection;

HANDLE g_hCompletionPort = NULL;
HANDLE g_hFilterDevice = NULL;

HANDLE g_hExitEvent = NULL;
HANDLE g_hNdisPyFilterThreads[2];

u_char *g_ReadBuffer = NULL;
OVERLAPPED g_ReadOverlapped;
OVERLAPPED g_WriteOverlapped;

// ========================================================================================================================

int main(int argc, char *argv[])
{
	std::cout << std::endl
			  << "NdisPyFilterCtrl " << g_cNdisPyFilterCtrlVersion << std::endl
			  << "Copyright \xB8" << "2007 Liam Kirton <liam@int3.ws>" << std::endl
			  << std::endl
			  << "Built at " << __TIME__ << " on " << __DATE__ << std::endl << std::endl;

	if(argc != 2)
	{
		PrintUsage();
		return -1;
	}
	else
	{
		g_FilterPath = argv[1];
	}

	InitializeCriticalSection(&g_ConsoleCriticalSection);
	g_ReadBuffer = new u_char[65535];

	try
	{
		if((g_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
		{
			throw std::exception("CreateEvent() Failed.");
		}

		PyInstance::GetInstance()->Load(g_FilterPath);

		if((g_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) == NULL)
		{
			throw std::exception("CreateIoCompletionPort() Failed.");
		}

		if((g_hFilterDevice = CreateFile("\\\\.\\NdisPyFilter",
										 GENERIC_READ | GENERIC_WRITE,
										 0,
										 NULL,
										 OPEN_EXISTING,
										 FILE_FLAG_OVERLAPPED,
										 NULL)) == INVALID_HANDLE_VALUE)
		{
			throw std::exception("CreateFile(\"\\\\.\\NdisPyFilter\") Failed.");
		}

		if(CreateIoCompletionPort(g_hFilterDevice, g_hCompletionPort, 0, 0) == NULL)
		{
			throw std::exception("CreateIoCompletionPort(g_hDevice) Failed.");
		}

		SecureZeroMemory(&g_hNdisPyFilterThreads, sizeof(g_hNdisPyFilterThreads));
		for(u_int i = 0; i < sizeof(g_hNdisPyFilterThreads) / sizeof(HANDLE); ++i)
		{
			if((g_hNdisPyFilterThreads[i] = CreateThread(NULL, 0, NdisPyFilterThread, NULL, 0, NULL)) == NULL)
			{
				throw std::exception("CreateThread() Failed.");
			}
		}

		if(PostQueuedCompletionStatus(g_hCompletionPort, 0, 0, &g_ReadOverlapped) == 0)
		{
			throw std::exception("PostQueuedCompletionStatus() Failed.");
		}

		SetConsoleCtrlHandler(CtrlHandlerRoutine, TRUE);
		WaitForSingleObject(g_hExitEvent, INFINITE);
		SetConsoleCtrlHandler(CtrlHandlerRoutine, FALSE);
	}
	catch(const std::exception &e)
	{
		std::cout << std::endl << "Error: " << e.what() << std::endl << std::endl;
		SetEvent(g_hExitEvent);
	}

	for(u_int i = 0; i < sizeof(g_hNdisPyFilterThreads) / sizeof(HANDLE); ++i)
	{
		if(PostQueuedCompletionStatus(g_hCompletionPort, 0, 0xFFFFFFFF, NULL) == 0)
		{
			throw std::exception("PostQueuedCompletionStatus() Failed.");
		}
	}

	for(u_int i = 0; i < sizeof(g_hNdisPyFilterThreads) / sizeof(HANDLE); ++i)
	{
		if(g_hNdisPyFilterThreads[i] != NULL)
		{
			if(WaitForSingleObject(g_hNdisPyFilterThreads[i], 2500) != WAIT_OBJECT_0)
			{
				std::cout << "WARNING: WaitForSingleObject(g_hNdisPyFilterThread) Failed." << std::endl;
			}
			CloseHandle(g_hNdisPyFilterThreads[i]);
			g_hNdisPyFilterThreads[i] = NULL;
		}
	}

	if(g_hFilterDevice != NULL)
	{
		CloseHandle(g_hFilterDevice);
		g_hFilterDevice = NULL;
	}

	if(g_hExitEvent != NULL)
	{
		CloseHandle(g_hExitEvent);
		g_hExitEvent = NULL;
	}

	delete [] g_ReadBuffer;
	DeleteCriticalSection(&g_ConsoleCriticalSection);

	return 0;
}

// ========================================================================================================================

BOOL WINAPI CtrlHandlerRoutine(DWORD dwCtrlType)
{
	if(dwCtrlType == CTRL_BREAK_EVENT)
	{
		PyInstance::GetInstance()->Unload();
		PyInstance::GetInstance()->Load(g_FilterPath);
	}
	else
	{
		if(g_hExitEvent != NULL)
		{
			EnterCriticalSection(&g_ConsoleCriticalSection);
			std::cout << "Closing." << std::endl;
			LeaveCriticalSection(&g_ConsoleCriticalSection);

			SetEvent(g_hExitEvent);
		}
	}
	return TRUE;
}

// ========================================================================================================================

DWORD WINAPI NdisPyFilterThread(LPVOID lpParameter)
{
	DWORD dwNumberOfBytesTransferred = 0;
	DWORD dwCompletionKey = 0;
	LPOVERLAPPED lpOverlapped = NULL;

	while(WaitForSingleObject(g_hExitEvent, 0) != WAIT_OBJECT_0)
	{
		GetQueuedCompletionStatus(g_hCompletionPort,
								  &dwNumberOfBytesTransferred,
								  &dwCompletionKey,
								  &lpOverlapped,
								  INFINITE);

		if(dwCompletionKey == 0xFFFFFFFF)
		{
			break;
		}
		else if(lpOverlapped != NULL)
		{
			if(lpOverlapped == &g_ReadOverlapped)
			{
				if(dwNumberOfBytesTransferred != 0)
				{
					u_char *packetBuffer = new u_char[dwNumberOfBytesTransferred];
					u_int packetBufferLength = dwNumberOfBytesTransferred;

					RtlCopyMemory(packetBuffer, g_ReadBuffer, packetBufferLength);

					u_char *modifiedPacketBuffer = NULL;
					u_int modifiedPacketBufferLength = 0;

					bool bPassPacket = true;
					bool bFixupPacket = true;
					
					char opType = packetBuffer[4];
					if(opType == 0x00)
					{
						PyInstance::GetInstance()->RecvPacketFilter(packetBuffer + 5,
																	packetBufferLength - 5,
																	bPassPacket,
																	bFixupPacket,
																	&modifiedPacketBuffer,
																	&modifiedPacketBufferLength);
					}
					else if(opType == 0x01)
					{
						PyInstance::GetInstance()->SendPacketFilter(packetBuffer + 5,
																	packetBufferLength - 5,
																	bPassPacket,
																	bFixupPacket,
																	&modifiedPacketBuffer,
																	&modifiedPacketBufferLength);
					}

					if(modifiedPacketBuffer != NULL)
					{
						packetBufferLength = modifiedPacketBufferLength + 5;

						u_int dwAdapter = *reinterpret_cast<u_int *>(packetBuffer);
						delete [] packetBuffer;

						packetBuffer = new u_char[packetBufferLength + 1]; // Additional Checksum Overflow Byte
						SecureZeroMemory(packetBuffer, packetBufferLength + 1);
						*reinterpret_cast<u_int *>(packetBuffer) = dwAdapter;
						packetBuffer[4] = opType;

						RtlCopyMemory(packetBuffer + 5, modifiedPacketBuffer, packetBufferLength - 5);
						
						delete [] modifiedPacketBuffer;
						modifiedPacketBuffer = NULL;

						if(bFixupPacket)
						{
							FixupPacket(packetBuffer + 5, packetBufferLength - 5);
						}
					}

					if(bPassPacket && (packetBufferLength > 5))
					{
						DWORD dwNumberOfBytesWritten = 0;
						SecureZeroMemory(&g_WriteOverlapped, sizeof(OVERLAPPED));

						if(!WriteFile(g_hFilterDevice,
									  packetBuffer,
									  packetBufferLength,
									  &dwNumberOfBytesWritten,
									  &g_WriteOverlapped))
						{
							if(GetLastError() != ERROR_IO_PENDING)
							{
								EnterCriticalSection(&g_ConsoleCriticalSection);
								std::cout << std::endl << "Error: WriteFile(g_hDevice) Failed. [" << GetLastError() << "]" << std::endl;
								LeaveCriticalSection(&g_ConsoleCriticalSection);

								SetEvent(g_hExitEvent);
							}
						}
					}

					delete [] packetBuffer;
					packetBuffer = NULL;
				}

				DWORD dwNumberOfBytesReceived = 0;
				SecureZeroMemory(&g_ReadOverlapped, sizeof(OVERLAPPED));

				if(!ReadFile(g_hFilterDevice,
							 g_ReadBuffer,
							 32768,
							 &dwNumberOfBytesReceived,
							 &g_ReadOverlapped))
				{
					if(GetLastError() != ERROR_IO_PENDING)
					{
						EnterCriticalSection(&g_ConsoleCriticalSection);
						std::cout << std::endl << "Error: ReadFile(g_hDevice) Failed. [" << GetLastError() << "]" << std::endl;
						LeaveCriticalSection(&g_ConsoleCriticalSection);

						SetEvent(g_hExitEvent);
					}
				}
			}
		}
	}

	return 0;
}

// ========================================================================================================================

void FixupPacket(u_char *buffer, u_int length)
{
	if(length >= sizeof(EthernetFrameHeader))
	{
		EthernetFrameHeader *pktEthernetFrameHeader = reinterpret_cast<EthernetFrameHeader *>(buffer);
		if((pktEthernetFrameHeader->Type == EtherTypeIp) &&
		   (length >= sizeof(EthernetFrameHeader) + sizeof(IpPacketHeader)))
		{
			IpPacketHeader *pktIpPacketHeader = reinterpret_cast<IpPacketHeader *>(buffer + sizeof(EthernetFrameHeader));
			
			u_int ipCrc = 0;
			pktIpPacketHeader->Crc = 0;
			InitialiseChecksum(ipCrc);
			UpdateChecksum(ipCrc, reinterpret_cast<u_short *>(pktIpPacketHeader), sizeof(IpPacketHeader) / sizeof(u_short));
			pktIpPacketHeader->Crc = FinaliseChecksum(ipCrc);

			ChecksumPseudoHeader pktChecksumPseudoHeader;
			pktChecksumPseudoHeader.DestinationAddress = pktIpPacketHeader->DestinationAddress;
			pktChecksumPseudoHeader.Length = htons(ntohs(pktIpPacketHeader->TotalLength) - sizeof(IpPacketHeader));
			pktChecksumPseudoHeader.Protocol = pktIpPacketHeader->Protocol;
			pktChecksumPseudoHeader.SourceAddress = pktIpPacketHeader->SourceAddress;
			pktChecksumPseudoHeader.Zero = 0;

			if(pktIpPacketHeader->Protocol == IpProtocolIcmp)
			{
				IcmpPacketHeader *pktIcmpPacketHeader = reinterpret_cast<IcmpPacketHeader *>(buffer + sizeof(EthernetFrameHeader) + sizeof(IpPacketHeader));
				pktIcmpPacketHeader->Checksum = 0;

				u_int icmpLength = (ntohs(pktIpPacketHeader->TotalLength) - sizeof(IpPacketHeader));
				if((icmpLength % 2) == 1)
				{
					icmpLength++;
				}
				
				if((sizeof(EthernetFrameHeader) + sizeof(IpPacketHeader) + icmpLength) <= length)
				{
					u_int icmpChecksum = 0;
					InitialiseChecksum(icmpChecksum);
					UpdateChecksum(icmpChecksum, reinterpret_cast<u_short *>(pktIcmpPacketHeader), icmpLength / sizeof(u_short));
					pktIcmpPacketHeader->Checksum = FinaliseChecksum(icmpChecksum);
				}
			}
			else if(pktIpPacketHeader->Protocol == IpProtocolTcp)
			{
				TcpPacketHeader *pktTcpPacketHeader = reinterpret_cast<TcpPacketHeader *>(buffer + sizeof(EthernetFrameHeader) + sizeof(IpPacketHeader));
				pktTcpPacketHeader->Checksum = 0;

				u_int tcpLength = (ntohs(pktIpPacketHeader->TotalLength) - sizeof(IpPacketHeader));
				if((tcpLength % 2) == 1)
				{
					tcpLength++;
				}
				
				if((sizeof(EthernetFrameHeader) + sizeof(IpPacketHeader) + tcpLength) <= (length + 1))
				{
					u_int tcpChecksum = 0;
					InitialiseChecksum(tcpChecksum);
					UpdateChecksum(tcpChecksum, reinterpret_cast<u_short *>(&pktChecksumPseudoHeader), sizeof(ChecksumPseudoHeader) / sizeof(u_short));
					UpdateChecksum(tcpChecksum, reinterpret_cast<u_short *>(pktTcpPacketHeader), tcpLength / sizeof(u_short));
					pktTcpPacketHeader->Checksum = FinaliseChecksum(tcpChecksum);
				}
			}
			else if(pktIpPacketHeader->Protocol == IpProtocolUdp)
			{
				UdpPacketHeader *pktUdpPacketHeader = reinterpret_cast<UdpPacketHeader *>(buffer + sizeof(EthernetFrameHeader) + sizeof(IpPacketHeader));
				pktUdpPacketHeader->Checksum = 0;

				u_int udpLength = (ntohs(pktIpPacketHeader->TotalLength) - sizeof(IpPacketHeader));
				if((udpLength % 2) == 1)
				{
					udpLength++;
				}
				
				if((sizeof(EthernetFrameHeader) + sizeof(IpPacketHeader) + udpLength) <= (length + 1))
				{
					u_int udpChecksum = 0;
					InitialiseChecksum(udpChecksum);
					UpdateChecksum(udpChecksum, reinterpret_cast<u_short *>(&pktChecksumPseudoHeader), sizeof(ChecksumPseudoHeader) / sizeof(u_short));
					UpdateChecksum(udpChecksum, reinterpret_cast<u_short *>(pktUdpPacketHeader), udpLength / sizeof(u_short));
					pktUdpPacketHeader->Checksum = FinaliseChecksum(udpChecksum);
				}
			}
		}
	}
}

// ========================================================================================================================

void PrintUsage()
{
	std::cout << "Usage: NdisPyFilterCtrl.exe <Filter.py>"
			  << std::endl << std::endl;
}

// ========================================================================================================================
