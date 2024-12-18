// EventManager.cpp : Defines the exported functions for the DLL.
//

#include "EventManager.h"

using namespace EventManager;
// This is an example of an exported variable
//EVENTMANAGER_API int nEventManager=0;

// This is an example of an exported function.
//EVENTMANAGER_API int fnEventManager(void)
//{
//    return 0;
//}

#ifndef UNICODE
#define tstring std::string
#else
#define tstring std::wstring
#endif

// This is the constructor of a class that has been exported.
CEventManager::CEventManager(const LPCTSTR& source)
	:eventSource(source),
	hEventSource(RegisterEventSource(NULL, source))
{

	//Init();
	/*std::wcout << "current path: " << std::filesystem::current_path().wstring() << " bool check if same " << (buffer != std::filesystem::current_path().wstring()) << "\n";*/
	/*if (std::wstring(buffer) == L"" || buffer != std::filesystem::current_path())
		rgtManager->SetVal(L"INSTALL_DIR", REG_SZ, (PVOID)std::filesystem::current_path().wstring().c_str(), std::filesystem::current_path().wstring().size() * sizeof(std::filesystem::current_path().wstring().data()));*/

		/*if (!hEventSource)
			hEventSource = RegisterEventSource(NULL, source);*/


}

CEventManager::~CEventManager()
{
	eventSource = NULL;
	if (hEventSource)
		DeregisterEventSource(hEventSource);
}

//void CEventManager::Init(const LPCTSTR& eventSource)
//{
//	try {
//		RegistryManager::CRegistryManager rmEvent(HKEY_LOCAL_MACHINE, tstring("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\").append(eventSource).data());
//		RegistryManager::CRegistryManager rmSource(HKEY_LOCAL_MACHINE, tstring("SOFTWARE\\HenchmanTRAK\\").append(eventSource).data());
//
//		TCHAR eventBuff[1024];
//		DWORD eventBuffSize = 1024;
//		rmEvent.GetVal("EventMessageFile", REG_SZ, (TCHAR*)eventBuff, eventBuffSize);
//		tstring eventMessageFile(eventBuff);
//
//		TCHAR sourceBuff[1024];
//		DWORD sourceBuffSize = 1024;
//		rmSource.GetVal("INSTALL_DIR", REG_SZ, (TCHAR*)sourceBuff, sourceBuffSize);
//		tstring installDir(sourceBuff);
//
//		if (!installDir.empty() && (eventMessageFile.empty() || eventMessageFile != installDir))
//		{
//			installDir.append("\\event_log.dll");
//			rmEvent.SetVal("EventMessageFile", REG_SZ, (TCHAR*)installDir.data(), installDir.length()+1);
//			DWORD typesSupported = 7;
//			rmEvent.SetVal("TypesSupported", REG_DWORD, (DWORD*)&typesSupported, sizeof(DWORD));
//		}
//		eventMessageFile.clear();
//		installDir.clear();
//	}
//	catch (std::exception& e)
//	{
//		ServiceHelper().WriteToError(e.what());
//	}
//}

DWORD CEventManager::EventMessage(const LPCTSTR& lpszFunction, const LPCTSTR& lpszMsg, const DWORD& errorCode, const LPCTSTR& buffer, const DWORD& bufferSize) const
{
	// Retrieve the system error message for the last-error code
	//DWORD dw = GetLastError();
	//std::cout << lpszFunction << " logged with message: " << msg << std::endl;
	//MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)

	//FormatMessage(
	//	FORMAT_MESSAGE_ALLOCATE_BUFFER |
	//	(std::string(msg).length() ? FORMAT_MESSAGE_FROM_STRING : FORMAT_MESSAGE_FROM_SYSTEM) |
	//	FORMAT_MESSAGE_IGNORE_INSERTS,
	//	(std::string(msg).length() ? msg : NULL) ,
	//	errorCode,
	//	0,
	//	(LPTSTR)&lpMsgBuf,
	//	0,
	//	NULL
	//);
	//
	//// Display the error message and exit the process
	//lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
	//	(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen(lpszFunction) + 40) * sizeof(TCHAR));
	//StringCchPrintf((LPTSTR)lpDisplayBuf,
	//	LocalSize(lpDisplayBuf) / sizeof(TCHAR),
	//	TEXT("%s returned with %d: %s"),
	//	lpszFunction, errorCode, lpMsgBuf);
	//LPVOID lpMsgBuf = nullptr;
	//FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS


	StringCchPrintf((LPTSTR)buffer,
		(lstrlen(lpszMsg) + lstrlen(lpszFunction) + errorCode + 25) * sizeof(TCHAR),
		TEXT("%s returned with %d: %s\0"),
		lpszFunction, errorCode, lpszMsg
	);

	//std::wcout << (LPCTSTR)lpDisplayBuf << "\n";

	//LocalFree(lpMsgBuf);

	return (lstrlen(lpszMsg) + lstrlen(lpszFunction) + errorCode + 25) * sizeof(TCHAR);
}

void CEventManager::ReportCustomEvent(const LPCTSTR& function, const LPCTSTR& msg, int type, DWORD errorCode)
{
	LPCTSTR lpszStrings[2];

	WORD EventType;
	DWORD EventId;
	if (!hEventSource && eventSource)
		hEventSource = RegisterEventSource(NULL, eventSource);

	if (hEventSource != nullptr)
	{
		TCHAR buffer[1024];
		DWORD buffSize = 1024;

		lpszStrings[0] = eventSource;
		const DWORD retSize = EventMessage(function, msg, errorCode ? errorCode : GetLastError(), buffer, buffSize);
		tstring refVal(buffer);
		lpszStrings[1] = refVal.data();

		switch (type)
		{
		case 1:
			EventId = SVC_INFORMATION;
			EventType = EVENTLOG_INFORMATION_TYPE;
			break;
		case 2:
			EventId = SVC_WARNING;
			EventType = EVENTLOG_WARNING_TYPE;
			break;
		case 3:
			EventId = SVC_ERROR;
			EventType = EVENTLOG_ERROR_TYPE;
			break;
		default:
			EventId = SVC_SUCCESS;
			EventType = EVENTLOG_SUCCESS;
			break;
		}

		ReportEvent(
			hEventSource,	// event log handle
			EventType,		// event type
			0,				// event category
			EventId,		// event identifier
			NULL,			// no security identifier
			2,				// size of lpszStrings array
			0,				// no binary data
			lpszStrings,	// array of strings
			NULL			// no binary data
		);
		if (hEventSource)
			DeregisterEventSource(hEventSource);
	}

}