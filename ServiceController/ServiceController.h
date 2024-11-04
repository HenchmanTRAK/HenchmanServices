#ifndef SERVICE_CONTROLLER_H
#define SERVICE_CONTROLLER_H

#include <iostream>
#include <strsafe.h>
#include <Windows.h>

class ServiceController
{

	SC_HANDLE schSCManager;
	SC_HANDLE schService;

	const char * serviceDetail[2];

public:
	SERVICE_STATUS			 g_ServiceStatus = { 0 };
	SERVICE_STATUS_HANDLE	 g_StatusHandle = NULL;
	HANDLE					 g_ServiceStopEvent = INVALID_HANDLE_VALUE;

	ServiceController(const char* serviceName, const char* serviceDisplayName);
	
	~ServiceController();
	
	void DoInstallSvc();
	void __stdcall DoStartSvc(const char* sService = nullptr);
	int __stdcall StartTargetSvc(const char* sService);
	void __stdcall DoStopSvc(const char* sService = nullptr);
	bool __stdcall StopDependentServices();
	void __stdcall DoDeleteSvc(const char* sService = nullptr);
	void ReportSvcStatus(
		DWORD dwCurrentState,
		DWORD dwWin32ExitCode,
		DWORD dwWaitHint
	);
	DWORD GetSvcStatus(const char* sService = nullptr);
	void WINAPI SvcCtrlHandler(DWORD CtrlCode);

};

#endif