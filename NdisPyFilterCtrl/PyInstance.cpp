// ========================================================================================================================
// NdisPyFilter
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// PyInstance.cpp
//
// Created: 01/09/2007
// ========================================================================================================================

#include "PyInstance.h"

#include <iostream>

#include "NdisPyFilterCtrl.h"

// ========================================================================================================================

static PyInstance g_PyInstance;

// ========================================================================================================================

static PyMethodDef NdisPyFilterMethods[] =
{
    {"set_recv_packet_filter", PyInstance::SetRecvPacketFilter, METH_VARARGS, NULL},
	{"set_send_packet_filter", PyInstance::SetSendPacketFilter, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

// ========================================================================================================================

PyInstance::PyInstance() : pyRecvPacketFilter_(NULL),
						   pySendPacketFilter_(NULL)
{
	if((hMutex_ = CreateMutex(NULL, FALSE, NULL)) == NULL)
	{
		std::cout << "Error: CreateMutex() Failed." << std::endl;
		return;
	}
}

// ========================================================================================================================

PyInstance::~PyInstance()
{
	Unload();

	if(hMutex_ != NULL)
	{
		CloseHandle(hMutex_);
		hMutex_ = NULL;
	}
}

// ========================================================================================================================

void PyInstance::Load(const std::string &path)
{
	std::cout << "Loading \"" << path << "\"." << std::endl;

	__try
	{
		Lock();
	
		Py_Initialize();
		Py_InitModule("ndispyfilter", NdisPyFilterMethods);
	
		HANDLE hPyFilter = INVALID_HANDLE_VALUE;
		if((hPyFilter = CreateFile(path.c_str(),
								   GENERIC_READ,
								   FILE_SHARE_READ,
								   NULL,
								   OPEN_EXISTING,
								   FILE_ATTRIBUTE_NORMAL,
								   NULL)) != INVALID_HANDLE_VALUE)
		{
			HANDLE hPyFilterMapping = NULL;
			if((hPyFilterMapping = CreateFileMapping(hPyFilter, NULL, PAGE_READONLY, 0, GetFileSize(hPyFilter, NULL), NULL)) != NULL)
			{
				char *pPyFilter = reinterpret_cast<char *>(MapViewOfFile(hPyFilterMapping, FILE_MAP_READ, 0, 0, 0));
				if(pPyFilter != NULL)
				{
					char *pPyBuffer = new char[GetFileSize(hPyFilter, NULL) + 1];
					RtlCopyMemory(pPyBuffer, pPyFilter, GetFileSize(hPyFilter, NULL));
					pPyBuffer[GetFileSize(hPyFilter, NULL)] = '\0';
					PyRun_SimpleString(pPyBuffer);
					delete [] pPyBuffer;
					
					if((pyRecvPacketFilter_ == NULL) || (pySendPacketFilter_ == NULL))
					{
						std::cout << "Error: Python NdisPyFilter.set_recv_packet_filter Or NdisPyFilter.set_send_packet_filter Not Called." << std::endl;
					}
	
					UnmapViewOfFile(pPyFilter);
				}
				else
				{
					std::cout << "Error: MapViewOfFile() Failed." << std::endl;
					return;
				}
				CloseHandle(hPyFilterMapping);
			}
			else
			{
				std::cout << "Error: CreateFileMapping() Failed." << std::endl;
			}
	
			CloseHandle(hPyFilter);
		}
		else
		{
			std::cout << "Error: CreateFile() Failed." << std::endl;
		}
	
		std::cout << std::endl;
	}
	__finally
	{
		Unlock();
	}
}

// ========================================================================================================================

void PyInstance::Unload()
{
	if(pyRecvPacketFilter_ != NULL)
	{
		Py_DECREF(pyRecvPacketFilter_);
		pyRecvPacketFilter_ = NULL;
	}
	if(pySendPacketFilter_ != NULL)
	{
		Py_DECREF(pySendPacketFilter_);
		pySendPacketFilter_ = NULL;
	}

	Py_Finalize();
}

// ========================================================================================================================

void PyInstance::RecvPacketFilter(const unsigned char *packetBuffer,
								  const unsigned int packetBufferLength,
								  bool &bPassPacket,
								  bool &bFixupPacket,
								  unsigned char **modifiedPacketBuffer,
								  unsigned int *modifiedPacketBufferLength)
{
	InternalPacketFilter(pyRecvPacketFilter_,
						 packetBuffer,
						 packetBufferLength,
						 bPassPacket,
						 bFixupPacket,
						 modifiedPacketBuffer,
						 modifiedPacketBufferLength);
}

// ========================================================================================================================

void PyInstance::SendPacketFilter(const unsigned char *packetBuffer,
								  const unsigned int packetBufferLength,
								  bool &bPassPacket,
								  bool &bFixupPacket,
								  unsigned char **modifiedPacketBuffer,
								  unsigned int *modifiedPacketBufferLength)
{
	InternalPacketFilter(pySendPacketFilter_,
						 packetBuffer,
						 packetBufferLength,
						 bPassPacket,
						 bFixupPacket,
						 modifiedPacketBuffer,
						 modifiedPacketBufferLength);
}

// ========================================================================================================================

PyInstance *PyInstance::GetInstance()
{
	return &g_PyInstance;
}

// ========================================================================================================================

void PyInstance::InternalPacketFilter(PyObject *filter,
									  const unsigned char *packetBuffer,
									  const unsigned int packetBufferLength,
									  bool &bPassPacket,
									  bool &bFixupPacket,
									  unsigned char **modifiedPacketBuffer,
									  unsigned int *modifiedPacketBufferLength)
{
	__try
	{
		Lock();

		if(filter == NULL)
		{
			std::cout << "WARNING: PyInstance::InternalPacketFilter() Called With filter NULL." << std::endl;
		}
		else
		{
			PyObject *arglist = Py_BuildValue("(s#,i)", packetBuffer, packetBufferLength, packetBufferLength);
			PyObject *result = PyEval_CallObject(filter, arglist);
			Py_DECREF(arglist);
			arglist = NULL;

			if(result != NULL)
			{
				if(result != Py_None)
				{
					PyObject *pReturnBuffer = NULL;
					unsigned int pReturnBufferLen = 0;
					char fillChar = '\0';

					int bInternalPassPacket = true;
					int bInternalFixupPacket = true;

					if(PyArg_ParseTuple(result, "iis#", &bInternalPassPacket, &bInternalFixupPacket, &pReturnBuffer, &pReturnBufferLen, &fillChar))
					{
						bPassPacket = (bInternalPassPacket != 0) ? true : false;
						bFixupPacket = (bInternalFixupPacket != 0)? true : false;

						*modifiedPacketBuffer = new unsigned char[pReturnBufferLen];
						*modifiedPacketBufferLength = pReturnBufferLen;

						RtlCopyMemory(*modifiedPacketBuffer, reinterpret_cast<unsigned char *>(pReturnBuffer), pReturnBufferLen);
					}
					else
					{
						bPassPacket = true;
						bFixupPacket = false;
						*modifiedPacketBuffer = NULL;
						*modifiedPacketBufferLength = 0;
						PyErr_WriteUnraisable(filter);
					}
					PyErr_Clear();
				}

				Py_DECREF(result);
			}
			else
			{
				bPassPacket = true;
				bFixupPacket = false;
				*modifiedPacketBuffer = NULL;
				*modifiedPacketBufferLength = 0;
				PyErr_WriteUnraisable(filter);
			}
		}
	}
	__finally
	{
		Unlock();
	}
}


PyObject *PyInstance::SetRecvPacketFilter(PyObject *dummy, PyObject *args)
{
	PyObject *pyResult = NULL;

	__try
	{
		g_PyInstance.Lock();

		if(PyArg_ParseTuple(args, "O", &g_PyInstance.pyRecvPacketFilter_))
		{
			if(!PyCallable_Check(g_PyInstance.pyRecvPacketFilter_))
			{
				PyErr_SetString(PyExc_TypeError, "Error: SetRecvPacketFilter() - Parameter Must Be Callable.");
			}
			else
			{
				Py_XINCREF(g_PyInstance.pyRecvPacketFilter_); 
				Py_INCREF(Py_None);
				pyResult = Py_None;
			}
		}
	}
	__finally
	{
		g_PyInstance.Unlock();
	}
    return pyResult;
}

// ========================================================================================================================

PyObject *PyInstance::SetSendPacketFilter(PyObject *dummy, PyObject *args)
{
	PyObject *pyResult = NULL;

	__try
	{
		g_PyInstance.Lock();

		if(PyArg_ParseTuple(args, "O", &g_PyInstance.pySendPacketFilter_))
		{
			if(!PyCallable_Check(g_PyInstance.pySendPacketFilter_))
			{
				PyErr_SetString(PyExc_TypeError, "Error: SetSendPacketFilter() - Parameter Must Be Callable.");
			}
			else
			{
				Py_XINCREF(g_PyInstance.pySendPacketFilter_); 
				Py_INCREF(Py_None);
				pyResult = Py_None;
			}
		}
	}
	__finally
	{
		g_PyInstance.Unlock();
	}
    return pyResult;
}

// ========================================================================================================================

void PyInstance::Lock()
{
	if(WaitForSingleObject(hMutex_, 2500) != WAIT_OBJECT_0)
	{
		std::cout << "Warning: WaitForSingleObject(hMutex_) Failed." << std::endl;
	}
}

// ========================================================================================================================

void PyInstance::Unlock()
{
	ReleaseMutex(hMutex_);
}

// ========================================================================================================================
