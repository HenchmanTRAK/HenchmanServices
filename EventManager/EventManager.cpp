
#include "EventManager.h"

using namespace std;

EventManager::EventManager(string source)
{
	cout << "Registering Event Source: " << source << endl;
	eventSource = source;
}

EventManager::~EventManager()
{
	cout << "Deregistering Event Source: " << eventSource << endl;
	
	eventSource.clear();
	GlobalFree(lpMsgBuf);
	GlobalFree(lpDisplayBuf);
}

const char * EventManager::EventMessage(const char *lpszFunction, string msg)
{
	// Retrieve the system error message for the last-error code
	/*LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;*/
	DWORD dw = GetLastError();
	cout << lpszFunction << "logged with message: " << msg << endl;

	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		(msg == "" ? FORMAT_MESSAGE_FROM_SYSTEM : FORMAT_MESSAGE_FROM_STRING) | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		msg == "" ? NULL : msg.data(),
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, 
		NULL
	);

	// Display the error message and exit the process
	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlenA((LPCTSTR)lpMsgBuf) + lstrlenA(lpszFunction) + 40) * sizeof(TCHAR));
	StringCchPrintfA((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s returned with %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	//MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);
	//LocalFree(lpMsgBuf);
	//WriteToLog((char*)lpDisplayBuf);
	//LocalFree(lpDisplayBuf);
	//ExitProcess(dw);
	return (LPCTSTR)lpDisplayBuf;
}

void EventManager::ReportCustomEvent(const char *function, std::string msg, int type)
{
	LPCTSTR lpszStrings[2];
	//DWORD errorCode = GetLastError();
	WORD EventType;
	DWORD EventId;
	//bool isError = errorCode != NO_ERROR;
	//TCHAR Buffer[80];
	hEventSource = RegisterEventSourceA(NULL, eventSource.data());

	if (NULL != hEventSource)
	{
		//StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

		lpszStrings[0] = eventSource.c_str();
		lpszStrings[1] = EventMessage(function, msg);
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

		/*if (errorCode == 0 || eventType == 0) {
			lpszStrings[1] = SuccessMessage(szFunction, msg);
		}
		else if (errorCode > 0 && msg == "")
		{
			lpszStrings[1] = ErrorMessage(szFunction);
			EventType = EVENTLOG_ERROR_TYPE;
			
		}
		else if (eventType == 2) {
			lpszStrings[1] = ErrorMessage(szFunction);
			EventType = EVENTLOG_WARNING_TYPE;
			EventId = SVC_WARNING;
		}
		else {
			lpszStrings[1] = InformationMessage(szFunction, msg);
			EventType = EVENTLOG_INFORMATION_TYPE;
			EventId = SVC_INFORMATION;
		}*/



		ReportEventA(
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
		DeregisterEventSource(hEventSource);
	}
}