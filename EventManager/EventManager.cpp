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

CEventManager::CEventManager()
	:eventSource(SERVICE_NAME),
	hEventSource(RegisterEventSource(NULL, SERVICE_NAME))
{

	/*RegistryManager::CRegistryManager rmEvent(HKEY_LOCAL_MACHINE, tstring("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\").append(SERVICE_NAME).data());

	DWORD size = MAX_PATH;
	std::vector<TCHAR> buffer(size);
	rmEvent.GetValSize("EventMessageFile", REG_SZ, &size, &buffer);

	if(size > 0)
		rmEvent.GetVal("EventMessageFile", REG_SZ, buffer.data(), &size);

	tstring eventMessageFile(buffer.data());*/


}

CEventManager::~CEventManager()
{
	eventSource = NULL;
	if (hEventSource) {
		DeregisterEventSource(hEventSource);
		hEventSource = NULL;
	}
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

DWORD CEventManager::EventMessage(const LPCTSTR& lpszFunction, const LPCTSTR& lpszMsg, const DWORD& errorCode, std::vector<TCHAR>& buffer) const
{
	// Retrieve the system error message for the last-error code
	//DWORD dw = GetLastError();

	DWORD size = (lstrlen(lpszFunction) + errorCode + lstrlen(lpszMsg) + 25) * sizeof(TCHAR);

	std::vector<TCHAR> vcMsgBuffer(size);

	StringCchPrintf(vcMsgBuffer.data(),
		vcMsgBuffer.size(),
		TEXT("%s returned with %d: %s\0"),
		lpszFunction, errorCode, lpszMsg
	);

	vcMsgBuffer.swap(buffer);

	vcMsgBuffer.clear();

	return buffer.size();
}

DWORD CEventManager::EventMessage(const LPCTSTR& lpszFunction, const LPCTSTR& lpszMsg, const DWORD& errorCode, LPCTSTR buffer, const DWORD& bufferSize) const
{
	// Retrieve the system error message for the last-error code
	//DWORD dw = GetLastError();
	//std::cout << lpszFunction << " logged with message: " << msg << std::endl;
	//MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)

	std::vector<TCHAR> strBuffer(bufferSize);
	strBuffer.resize((lstrlen(lpszMsg) + lstrlen(lpszFunction) + errorCode + 25) * sizeof(TCHAR));

	StringCchPrintf(strBuffer.data(),
		strBuffer.size(),
		TEXT("%s returned with %d: %s\0"),
		lpszFunction, errorCode, lpszMsg
	);

	buffer = strBuffer.data();

	strBuffer.clear();

	return lstrlen(buffer);
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
		/*TCHAR buffer[1024] = "\0";
		DWORD buffSize = 1024;*/
		std::vector<TCHAR> buffer;

		lpszStrings[0] = eventSource;
		const DWORD retSize = EventMessage(function, msg, errorCode ? errorCode : GetLastError(), buffer);
		//std::cout << "returned buffer" << buffer.data() << "\n";
		tstring refVal(buffer.data());
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
		/*if (hEventSource)
			DeregisterEventSource(hEventSource);*/
	}

}