// ========================================================================================================================
// NdisPyFilter
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// PyInstance.h
//
// Created: 01/09/2007
// ========================================================================================================================

#pragma once

// ========================================================================================================================

#include <windows.h>

#include <python.h>

#include <string>

// ========================================================================================================================

class PyInstance
{
public:
	PyInstance();
	~PyInstance();

	void Load(const std::string &path);
	void Unload();

	void RecvPacketFilter(const unsigned char *packetBuffer,
						  const unsigned int packetBufferLength,
						  bool &bPassPacket,
						  bool &bFixupPacket,
						  unsigned char **modifiedPacketBuffer,
						  unsigned int *modifiedPacketBufferLength);
	
	void SendPacketFilter(const unsigned char *packetBuffer,
						  const unsigned int packetBufferLength,
						  bool &bPassPacket,
						  bool &bFixupPacket,
						  unsigned char **modifiedPacketBuffer,
						  unsigned int *modifiedPacketBufferLength);

	static PyInstance *GetInstance();
	static PyObject *PyInstance::SetRecvPacketFilter(PyObject *dummy, PyObject *args);
	static PyObject *PyInstance::SetSendPacketFilter(PyObject *dummy, PyObject *args);

private:
	void InternalPacketFilter(PyObject * filter,
							  const unsigned char *packetBuffer,
							  const unsigned int packetBufferLength,
							  bool &bPassPacket,
							  bool &bFixupPacket,
							  unsigned char **modifiedPacketBuffer,
							  unsigned int *modifiedPacketBufferLength);

	void Lock();
	void Unlock();

	HANDLE hMutex_;
	
	PyObject *pyRecvPacketFilter_;
	PyObject *pySendPacketFilter_;
};

// ========================================================================================================================
