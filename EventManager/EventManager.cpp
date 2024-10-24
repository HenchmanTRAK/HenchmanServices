
#include "EventManager.h"

using namespace std;

EventManager::EventManager(string source)
{
	cout << "Registering Event Source: " << source << endl;
	eventSource = source;
	hEventSource = NULL;
	lpMsgBuf = nullptr;
	lpDisplayBuf = nullptr;
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
	DWORD dw = GetLastError();
	cout << lpszFunction << " logged with message: " << msg << endl;

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
	
	return (LPCTSTR)lpDisplayBuf;
}

void EventManager::ReportCustomEvent(const char *function, std::string msg, int type)
{
	LPCTSTR lpszStrings[2];
	
	WORD EventType;
	DWORD EventId;
	
	hEventSource = RegisterEventSourceA(NULL, eventSource.data());

	if (NULL != hEventSource)
	{
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