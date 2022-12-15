//  ***************************************************************
//  console.c - Creation date: 10/12/2022
//  -------------------------------------------------------------
//  NanoShell Copyright (C) 2022 - Licensed under GPL V3
//
//  ***************************************************************
//  Programmer(s):  iProgramInCpp (iprogramincpp@gmail.com)
//  ***************************************************************

#include <console.h>
#include <string.h>
#include <print.h>
#include <misc.h>
#include <task.h>
#include <vfs.h>

static bool s_bInitializedFDs = false;
extern Console* g_currentConsole;

bool CoInputBufferEmpty()
{
	return !CoAnythingOnInputQueue (g_currentConsole);
}

extern void KeTaskDone();

char CoGetChar()
{
	char b = 0;
	FiRead(FD_STDIN, &b, 1);
	return b;
}

void CoKickOff()
{
	CoInitAsE9Hack(&g_debugConsole);
	CoInitAsE9Hack(&g_debugSerialConsole);
}

void CoGetString(char* buffer, int max_size)
{
	int index = 0, max_length = max_size - 1;
	//index represents where the next character we type would go
	while (index < max_length)
	{
		//! has to stall
		char k = CoGetChar();
		if (k == '\n')
		{
			buffer[index++] = 0;
			return;
		}
		else if (k == '\b')
		{
			if (index > 0)
			{
				index--;
				buffer[index] = 0;
			}
		}
		else
		{
			buffer[index++] = k;
		}
	}
	buffer[index] = 0;
}


void CLogMsg (Console* pConsole, const char* fmt, ...)
{
	//allocate a buffer well sized
	char cr[8192];
	va_list list;
	va_start(list, fmt);
	vsnprintf(cr, sizeof(cr) - 2, fmt, list);
	
	sprintf (cr + strlen(cr), "\n");
	CoPrintString(pConsole, cr);
	
	va_end(list);
}

void CLogMsgNoCr (Console* pConsole, const char* fmt, ...)
{
	//allocate a buffer well sized
	char cr[8192];
	va_list list;
	va_start(list, fmt);
	vsnprintf(cr, sizeof(cr), fmt, list);
	CoPrintString(pConsole, cr);
	
	va_end(list);
}

void LogString(const char* str)
{
	if (s_bInitializedFDs)
	{
		FiWrite(FD_STDOUT, (void*)str, strlen(str));
	}
	else
	{
		CoPrintString(&g_debugConsole, str);
	}
}

void LogMsg (const char* fmt, ...)
{
	// Mf you better be in an interrupt enabled context
	KeVerifyInterruptsEnabled;
	
	//allocate a buffer well sized
	char cr[8192];
	va_list list;
	va_start(list, fmt);
	vsnprintf(cr, sizeof(cr) - 2, fmt, list);
	
	sprintf (cr + strlen(cr), "\n");
	LogString(cr);
	
	va_end(list);
}

void LogMsgNoCr (const char* fmt, ...)
{
	// Mf you better be in an interrupt enabled context
	KeVerifyInterruptsEnabled;
	
	//allocate a buffer well sized
	char cr[8192];
	va_list list;
	va_start(list, fmt);
	vsnprintf(cr, sizeof(cr), fmt, list);
	LogString(cr);
	
	va_end(list);
}

void ILogMsg (const char* fmt, ...)
{
	//allocate a buffer well sized
	char cr[8192];
	va_list list;
	va_start(list, fmt);
	vsnprintf(cr, sizeof(cr) - 2, fmt, list);
	
	sprintf (cr + strlen(cr), "\n");
	CoPrintString(&g_debugConsole, cr);
	
	va_end(list);
}

void ILogMsgNoCr (const char* fmt, ...)
{
	//allocate a buffer well sized
	char cr[8192];
	va_list list;
	va_start(list, fmt);
	vsnprintf(cr, sizeof(cr), fmt, list);
	CoPrintString(&g_debugConsole, cr);
	
	va_end(list);
}

void SLogMsg (const char* fmt, ...){
	//allocate a buffer well sized
	char cr[8192];
	va_list list;
	va_start(list, fmt);
	vsnprintf(cr, sizeof(cr) - 2, fmt, list);
	
	sprintf (cr + strlen(cr), "\n");
	CoPrintString(&g_debugSerialConsole, cr);
	
	va_end(list);
}

void SLogMsgNoCr (const char* fmt, ...)
{
	//allocate a buffer well sized
	char cr[8192];
	va_list list;
	va_start(list, fmt);
	vsnprintf(cr, sizeof(cr), fmt, list);
	CoPrintString(&g_debugSerialConsole, cr);
	
	va_end(list);
}

const char* g_uppercaseHex = "0123456789ABCDEF";

void LogHexDumpData (void* pData, int size)
{
	uint8_t* pDataAsNums = (uint8_t*)pData, *pDataAsText = (uint8_t*)pData;
	char c[7], d[4];
	c[5] = 0;   d[2] = ' ';
	c[6] = ' '; d[3] = 0;
	c[4] = ':';
	
	#define BYTES_PER_ROW 16
	for (int i = 0; i < size; i += BYTES_PER_ROW) {
		// print the offset
		c[0] = g_uppercaseHex[(i & 0xF000) >> 12];
		c[1] = g_uppercaseHex[(i & 0x0F00) >>  8];
		c[2] = g_uppercaseHex[(i & 0x00F0) >>  4];
		c[3] = g_uppercaseHex[(i & 0x000F) >>  0];
		LogMsgNoCr("%s", c);
		
		for (int j = 0; j < BYTES_PER_ROW; j++) {
			uint8_t p = *pDataAsNums++;
			d[0] = g_uppercaseHex[(p & 0xF0) >> 4];
			d[1] = g_uppercaseHex[(p & 0x0F) >> 0];
			LogMsgNoCr("%s", d);
		}
		LogMsgNoCr("   ");
		for (int j = 0; j < BYTES_PER_ROW; j++) {
			char c = *pDataAsText++;
			if (c < ' ') c = '.';
			LogMsgNoCr("%c",c);
		}
		LogMsg("");
	}
}

static uint32_t FsTeletypeRead(FileNode* pNode, UNUSED uint32_t offset, uint32_t size, void* pBuffer, bool bBlock)
{
	Console* pConsole = (Console*)pNode->m_implData;
	
	if (pConsole != &g_debugConsole) return 0; // we REALLY can't read from here.
	
	char* pBufferChr = (char*)pBuffer; uint32_t read = 0;
	
	while (read < size)
	{
		while (!CoAnythingOnInputQueue(pConsole))
		{
			if (!bBlock) return read;
			
			WaitDebugWrite();
		}
		
		char chr;
		*pBufferChr++ = chr = CoReadFromInputQueue(pConsole);
		
		// TODO allow disabling this
		CoPrintChar(pConsole, chr);
		
		read++;
	}
	
	return read; // Can't read anything!
}

static uint32_t FsTeletypeWrite(FileNode* pNode, UNUSED uint32_t offset, uint32_t size, void* pBuffer, UNUSED bool bBlock)
{
	Console* pConsole = (Console*)pNode->m_implData;
	const char* pText = (const char*)pBuffer;
	for (uint32_t i = 0; i < size; i++)
	{
		CoPrintChar(pConsole, *(pText++));
	}
	return size;
}
void FsRootCreateFileAt(const char* path, void* pContents, size_t sz);

// Create two files, 'ConDbg' and 'ConVid'.
void FsInitializeConsoleFiles()
{
	FsRootCreateFileAt(DEVICES_DIR "/ConVid", NULL, 0);
	FsRootCreateFileAt(DEVICES_DIR "/ConDbg", NULL, 0);
	
	FileNode* pNode;
	
	pNode = FsResolvePath(DEVICES_DIR "/ConVid"); ASSERT(pNode);
	pNode->m_refCount = NODE_IS_PERMANENT;
	pNode->m_perms  = PERM_READ | PERM_WRITE;
	pNode->m_type   = FILE_TYPE_CHAR_DEVICE;
	pNode->m_length = 0;
	pNode->Read     = FsTeletypeRead;
	pNode->Write    = FsTeletypeWrite;
	pNode->m_implData = (uint32_t)&g_debugConsole;
	
	pNode = FsResolvePath(DEVICES_DIR "/ConDbg"); ASSERT(pNode);
	pNode->m_refCount = NODE_IS_PERMANENT;
	pNode->m_perms  = PERM_READ | PERM_WRITE;
	pNode->m_type   = FILE_TYPE_CHAR_DEVICE;
	pNode->m_length = 0;
	pNode->Read     = FsTeletypeRead;
	pNode->Write    = FsTeletypeWrite;
	pNode->m_implData = (uint32_t)&g_debugSerialConsole;
	
	// Open the files as file descriptor 0, 1, 2.
	int filedes[3];
	
	filedes[0] = FiOpen(DEVICES_DIR "/ConVid", O_WRONLY);
	filedes[1] = FiOpen(DEVICES_DIR "/ConVid", O_RDONLY | O_FORCE);
	filedes[2] = FiOpen(DEVICES_DIR "/ConVid", O_RDONLY | O_FORCE);
	
	// Check if they actually were.
	
	if (filedes[0] != FD_STDIN)
	{
		SLogMsg("ERROR: filedes[0] = %d", filedes[0]);
		ASSERT(filedes[0] != FD_STDIN && "STDIN should have been opened in FD 0.");
	}
	if (filedes[1] != FD_STDOUT)
	{
		SLogMsg("ERROR: filedes[1] = %d", filedes[1]);
		ASSERT(filedes[1] != FD_STDOUT && "STDOUT should have been opened in FD 1.");
	}
	if (filedes[2] != FD_STDERR)
	{
		SLogMsg("ERROR: filedes[2] = %d", filedes[2]);
		ASSERT(filedes[2] != FD_STDERR && "STDERR should have been opened in FD 2.");
	}
	
	s_bInitializedFDs = true; // LogMsg will now redirect output through here.
}
