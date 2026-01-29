// ServiceController.cpp : Defines the exported functions for the DLL.
//

#include "ServiceController.h"


// This is an example of an exported variable
//SERVICECONTROLLER_API int nServiceController=0;

// This is an example of an exported function.
//SERVICECONTROLLER_API int fnServiceController(void)
//{
//    return 0;
//}

using namespace ServiceController;
// This is the constructor of a class that has been exported.

//SERVICE_STATUS gServiceStatus;
//SERVICE_STATUS_HANDLE gSvcStatusHandle;
//HANDLE gServiceStopEvent ;

//SService gService;

CServiceController::CServiceController(const SService& serviceDetails, bool testing)
	: mService(serviceDetails), mTaskScheduler()
{
	//std::cout << mService.serviceName << ":" << mService.displayName << std::endl;
	
	if (testing)
		pTesting = !pTesting;

	/*gService.serviceStatus = mService.serviceStatus;
	gService.serviceStatusHandle = mService.serviceStatusHandle;
	gService.serviceStopEvent = mService.serviceStopEvent;*/

	return;
}

CServiceController::~CServiceController()
{
	schSCManager = nullptr;
	schService = nullptr;
}

void CServiceController::DoInstallSvc(bool disableTask)
{
	if (disableTask)
		disableTaskCreation = true;
	//std::cout << "Installing: " << mService.serviceName << ":" << mService.displayName << std::endl;
	
	TCHAR szUnquotedPath[MAX_PATH];

	if (!GetModuleFileName(NULL, szUnquotedPath, MAX_PATH))
	{
		printf("Cannot install service (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("Cannot install service ("+ std::to_string(GetLastError()) +")\n");
		return;
	}

	TCHAR szPath[MAX_PATH];
	StringCbPrintf(szPath, MAX_PATH, TEXT("\"%s\""), szUnquotedPath);
	
	//tstring wPath(&szPath[0]);
#ifdef UNICODE
	std::wstring wPath(&szPath[0]);
#else
	std::string wPath(&szPath[0]);
#endif
	
	//std::string sSZPath(wPath.begin(), wPath.end());

	if (pTesting) 
	{
		goto add_new_task;
	}

	schSCManager = OpenSCManager(
		NULL,					// local computer
		NULL,					// ServiceActive database
		SC_MANAGER_ALL_ACCESS	// full access rights
	);

	if (schSCManager == NULL) {
		printf("OpenSCManager failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("OpenSCManager failed (" + std::to_string(GetLastError()) + ")\n");

		return;
	}

	if (OpenService(
		schSCManager,				// SCM database
		TEXT("ServiceHenchman"),	// Name of Service
		SERVICE_ALL_ACCESS			// Level of access
	))
	{
		DoStopSvc(TEXT("ServiceHenchman"));
		DoDeleteSvc(TEXT("ServiceHenchman"));
	}

	// SERVICE_NAME
	// SERVICE_DISPLAY_NAME
	schService = CreateService(
		schSCManager,					// SCM database 
		mService.serviceName,			// name of service 
		mService.displayName,			// service name to display 
		SERVICE_ALL_ACCESS,				// desired access 
		SERVICE_WIN32_OWN_PROCESS
		| SERVICE_INTERACTIVE_PROCESS,	// service type 
		SERVICE_AUTO_START,				// start type 
		SERVICE_ERROR_NORMAL, 			// error control type 
		wPath.c_str(), 					// path to service's binary 
		NULL, 							// no load ordering group 
		NULL,							// no tag identifier 
		NULL,							// no dependencies 
		mService.localUser,				// LocalSystem account 
		mService.localPass				// no password 
	);

	if (schService == NULL) {
		printf("CreateService failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("CreateService failed (" + std::to_string(GetLastError()) + ")\n");

	}
	else {
		printf("Service installed successfully\n");
		ServiceHelper().WriteToLog("Service installed successfully\n");
		CloseServiceHandle(schService);
	}
	CloseServiceHandle(schSCManager);
	
add_new_task:
	try {
		if (disableTaskCreation)
			return;
		mTaskScheduler.addNewTask(ServiceHelper().s2ws(mService.serviceName).c_str(), ServiceHelper().s2ws(mService.servicePath));
	}
	catch (const std::exception& e) {
		ServiceHelper().WriteToError(e.what());
	}
}

void CServiceController::DoStartSvc(const TCHAR* sService)
{
	if (!sService)
		sService = mService.serviceName;

	SERVICE_STATUS_PROCESS ssStatus;
	ZeroMemory(&ssStatus, sizeof(ssStatus));
	DWORD dwOldCheckPoint;
	DWORD dwStartTickCount;
	DWORD dwWaitTime;
	DWORD dwBytesNeeded;

	schSCManager = OpenSCManager(
		NULL,					// local computer
		NULL,					// servicesActive database
		SC_MANAGER_ALL_ACCESS	// full access rights
	);
	if (schSCManager == NULL)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("OpenSCManager failed (" + std::to_string(GetLastError()) + ")\n");
		return;
	}

	// Get ServiceHandle
	schService = OpenService(
		schSCManager,		// SCM database
		sService,			// Name of Service
		SERVICE_ALL_ACCESS	// Level of access
	);
	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("OpenService failed (" + std::to_string(GetLastError()) + ")\n");
		CloseServiceHandle(schSCManager);
		return;
	}

	// Check service status to ensure it is not stopped
	if (!QueryServiceStatusEx(
		schService,						// service handler
		SC_STATUS_PROCESS_INFO,			// information level
		(LPBYTE)&ssStatus,				// address of structure
		sizeof(SERVICE_STATUS_PROCESS),	// size of structure
		&dwBytesNeeded					// size needed if buffer is too small
	))
	{
		printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("QueryServiceStatusEx failed (" + std::to_string(GetLastError()) + ")\n");
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;
	}

	// Check service is not already running
	if (ssStatus.dwCurrentState != SERVICE_STOPPED && ssStatus.dwCurrentState != SERVICE_STOP_PENDING)
	{
		printf("Cannot start the service because it is already running\n");
		ServiceHelper().WriteToLog("Cannot start the service because it is already running\n");
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;
	}

	// Save tickCount and initial checkPoint
	dwStartTickCount = GetTickCount64();
	dwOldCheckPoint = ssStatus.dwCheckPoint;
	std::cout << ssStatus.dwCurrentState << "\n";
	// Wait for service to stop
	while (ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
	{
		// Ensure wait time does not exceed waitHint. Recommended 1/10 of waitHint
		dwWaitTime = ssStatus.dwWaitHint / 10;

		// Ensure wait time is no shorter than 1 second or creater than 10.
		if (dwWaitTime < 1000)
			dwWaitTime = 1000;
		else if (dwWaitTime > 10000)
			dwWaitTime = 10000;
		Sleep(dwWaitTime);

		// Check if service has stopped pending.
		if (!QueryServiceStatusEx(
			schService,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssStatus,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded
		))
		{
			printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
			ServiceHelper().WriteToLog("QueryServiceStatusEx failed (" + std::to_string(GetLastError()) + ")\n");
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return;
		}
		// update tickCount and checkPoint
		if (ssStatus.dwCheckPoint > dwOldCheckPoint)
		{
			dwStartTickCount = GetTickCount64();
			dwOldCheckPoint = ssStatus.dwCheckPoint;
		}
		if (GetTickCount64() - dwStartTickCount > ssStatus.dwWaitHint)
		{
			printf("Timeout: Waiting for service to stop\n");
			ServiceHelper().WriteToLog("Timeout: Waiting for service to stop\n");
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return;
		}
	}

	if (!StartService(
		schService,
		0,
		NULL
	))
	{
		printf("StartService failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("StartService failed (" + std::to_string(GetLastError()) + ")\n");
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;
	}
	printf("Service start pending...\n");
	ServiceHelper().WriteToLog("Service start pending...\n");

	// Check the status until the service is no longer start pending. 
	if (!QueryServiceStatusEx(
		schService,						// handle to service 
		SC_STATUS_PROCESS_INFO,			// info level
		(LPBYTE)&ssStatus,				// address of structure
		sizeof(SERVICE_STATUS_PROCESS), // size of structure
		&dwBytesNeeded))				// if buffer too small
	{
		printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("QueryServiceStatusEx failed (" + std::to_string(GetLastError()) + ")\n");
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;
	}

	// update tickCount and checkPoint
	dwStartTickCount = GetTickCount64();
	dwOldCheckPoint = ssStatus.dwCheckPoint;

	while (ssStatus.dwCurrentState == SERVICE_START_PENDING)
	{
		dwWaitTime = ssStatus.dwWaitHint / 10;
		if (dwWaitTime < 1000)
			dwWaitTime = 1000;
		else if (dwWaitTime > 10000)
			dwWaitTime = 10000;
		Sleep(dwWaitTime);

		if (!QueryServiceStatusEx(
			schService,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssStatus,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded
		))
		{
			printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
			ServiceHelper().WriteToLog("QueryServiceStatusEx failed (" + std::to_string(GetLastError()) + ")\n");
			break;
		}

		// update tickCount and checkPoint
		if (ssStatus.dwCheckPoint > dwOldCheckPoint)
		{
			dwStartTickCount = GetTickCount64();
			dwOldCheckPoint = ssStatus.dwCheckPoint;
		}
		if (GetTickCount64() - dwStartTickCount > ssStatus.dwWaitHint)
		{
			break;
		}
	}

	// Check if the service is running.
	if (ssStatus.dwCurrentState != SERVICE_RUNNING)
	{
		printf("Service not started. \n");
		printf("  Current State: %d\n", ssStatus.dwCurrentState);
		printf("  Exit Code: %d\n", ssStatus.dwWin32ExitCode);
		printf("  Check Point: %d\n", ssStatus.dwCheckPoint);
		printf("  Wait Hint: %d\n", ssStatus.dwWaitHint);
		ServiceHelper().WriteToLog("Service not started. \n  Current State: "+ std::to_string(ssStatus.dwCurrentState) + "\n  Exit State: " + std::to_string(ssStatus.dwWin32ExitCode) + "\n  Check Point: " + std::to_string(ssStatus.dwCheckPoint) + "\n  Wait Hint: " + std::to_string(ssStatus.dwWaitHint) + "\n");
	}
	else
	{
		printf("Service started successfully.\n");
		ServiceHelper().WriteToLog("Service started successfully.\n");

	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);

add_new_task:
	try {
		if (disableTaskCreation || pTesting);
			return;
		mTaskScheduler.addNewTask(ServiceHelper().s2ws(mService.serviceName).c_str(), ServiceHelper().s2ws(mService.servicePath));
	}
	catch (const std::exception& e) {
		ServiceHelper().WriteToError(e.what());
	}

	return;
}

int CServiceController::StartTargetSvc(const TCHAR* sService)
{
	if (!sService)
		sService = mService.serviceName;

	// Get ServiceManager Handle
	schSCManager = OpenSCManager(
		NULL,					// local computer
		NULL,					// servicesActive database
		SC_MANAGER_ALL_ACCESS	// full access rights
	);

	if (schSCManager == NULL)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("OpenSCManager failed (" + std::to_string(GetLastError()) + ")\n");
		return 0;
	}


	schService = OpenService(
		schSCManager,		// SCM database
		sService,			// Name of Service
		SERVICE_ALL_ACCESS	// Level of access
	);

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("OpenService failed (" + std::to_string(GetLastError()) + ")\n");
		CloseServiceHandle(schSCManager);
		return 0;
	}

	if (!ChangeServiceConfig(
		schService,				// Service Handle
		SERVICE_NO_CHANGE,		// Service Type
		SERVICE_NO_CHANGE,		// Service Start Condition
		SERVICE_NO_CHANGE,		// Service Error Response
		NULL,					// Target Service Path
		NULL,					// Load Order Group
		NULL,					// Tag ID
		NULL,					// Dependencies
		NULL,					// Service Account Name
		NULL,					// Service Account Password
		NULL					// Service Display Name
	))
	{
		printf("Failed to change target service settings (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("Failed to change target service settings (" + std::to_string(GetLastError()) + ")\n");
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return 0;
	}

	SERVICE_DELAYED_AUTO_START_INFO DelayedStartInfo;
	DelayedStartInfo.fDelayedAutostart = true;

	if (!ChangeServiceConfig2(
		schService,
		SERVICE_CONFIG_DELAYED_AUTO_START_INFO,
		&DelayedStartInfo
	))
	{
		printf("Failed to update target service to delayed auto start (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("Failed to update target service to delayed auto start (" + std::to_string(GetLastError()) + ")\n");
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return 0;
	}

	if (!StartService(
		schService,
		0,
		NULL
	))
	{
		printf("StartService failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("StartService failed (" + std::to_string(GetLastError()) + ")\n");
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return 0;
		/*DoStopSvc();
		DoDeleteSvc();*/
	}
	printf("Service start pending...\n");
	ServiceHelper().WriteToLog("Service start pending...\n");
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
	return 1;
}

void CServiceController::DoStopSvc(const TCHAR* sService)
{
	if (!sService)
		sService = mService.serviceName;

	SERVICE_STATUS_PROCESS ssp;
	ZeroMemory(&ssp, sizeof(ssp));
	DWORD dwStartTime = GetTickCount64();
	DWORD dwBytesNeeded;
	DWORD dwTimeout = (30 * 1000); // 30-second time-out
	DWORD dwWaitTime;


	// Get a handle to the SCM database. 
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("OpenSCManager failed (" + std::to_string(GetLastError()) + ")\n");
		return;
	}

	// Get a handle to the service.
	schService = OpenService(
		schSCManager,					// SCM database 
		sService,						// name of service 
		SERVICE_STOP |
		SERVICE_QUERY_STATUS |
		SERVICE_ENUMERATE_DEPENDENTS
	);

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("OpenService failed (" + std::to_string(GetLastError()) + ")\n");
		CloseServiceHandle(schSCManager);
		return;
	}

	// Make sure the service is not already stopped.
	if (!QueryServiceStatusEx(
		schService,
		SC_STATUS_PROCESS_INFO,
		(LPBYTE)&ssp,
		sizeof(SERVICE_STATUS_PROCESS),
		&dwBytesNeeded
	))
	{
		printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("QueryServiceStatusEx failed (" + std::to_string(GetLastError()) + ")\n");
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;
	}

	if (ssp.dwCurrentState == SERVICE_STOPPED)
	{
		printf("Service is already stopped.\n");
		ServiceHelper().WriteToLog("Service is already stopped.\n");
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;
	}

	// If a stop is pending, wait for it.
	while (ssp.dwCurrentState == SERVICE_STOP_PENDING)
	{
		printf("Service stop pending...\n");
		ServiceHelper().WriteToLog("Service stop pending...\n");

		dwWaitTime = ssp.dwWaitHint / 10;
		if (dwWaitTime < 1000)
			dwWaitTime = 1000;
		else if (dwWaitTime > 10000)
			dwWaitTime = 10000;
		Sleep(dwWaitTime);

		if (!QueryServiceStatusEx(
			schService,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded
		))
		{
			printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
			ServiceHelper().WriteToLog("QueryServiceStatusEx failed (" + std::to_string(GetLastError()) + ")\n");
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return;
		}

		if (ssp.dwCurrentState == SERVICE_STOPPED)
		{
			printf("Service stopped successfully.\n");
			ServiceHelper().WriteToLog("Service stopped successfully.\n");
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return;
		}

		if (GetTickCount64() - dwStartTime > dwTimeout)
		{
			printf("Service stop timed out.\n");
			ServiceHelper().WriteToLog("Service stop timed out\n");
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return;
		}
	}

	// If the service is running, dependencies must be stopped first.
	StopDependentServices();

	// Send a stop code to the service.
	if (!ControlService(
		schService,
		SERVICE_CONTROL_STOP,
		(LPSERVICE_STATUS)&ssp
	))
	{
		printf("ControlService failed (%d)\n", GetLastError());
		ServiceHelper().WriteToLog("ControlService failed (" + std::to_string(GetLastError()) + ")\n");
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;
	}

	// Wait for the service to stop.
	while (ssp.dwCurrentState != SERVICE_STOPPED)
	{
		Sleep(ssp.dwWaitHint / 10);
		if (!QueryServiceStatusEx(
			schService,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded
		))
		{
			printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
			ServiceHelper().WriteToLog("ControlService failed (" + std::to_string(GetLastError()) + ")\n");
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return;
		}

		if (ssp.dwCurrentState == SERVICE_STOPPED)
			break;

		if (GetTickCount64() - dwStartTime > dwTimeout)
		{
			printf("Wait timed out\n");
			ServiceHelper().WriteToLog("Wait timed out\n");
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return;
		}
	}

	printf("Service stopped successfully\n");
	ServiceHelper().WriteToLog("Service stopped successfully\n");

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);

	return;
}

bool CServiceController::StopDependentServices()
{
	DWORD dwBytesNeeded;
	DWORD dwStartTime = GetTickCount64();
	DWORD dwTimeout = (30 * 1000); // 30-second time-out
	DWORD dwCount;
	DWORD i;

	SERVICE_STATUS_PROCESS  ssp;
	LPENUM_SERVICE_STATUS   lpDependencies = NULL;
	ENUM_SERVICE_STATUS     ess;
	SC_HANDLE               hDepService;
	ZeroMemory(&ssp, sizeof(ssp));

	bool result = TRUE;


	// Pass a zero-length buffer to get the required buffer size.
	if (EnumDependentServices(schService, SERVICE_ACTIVE,
		lpDependencies, 0, &dwBytesNeeded, &dwCount))
	{
		// If the Enum call succeeds, then there are no dependent
		// services, so do nothing.
		return result;
	}
	else
	{
		if (GetLastError() != ERROR_MORE_DATA)
			return FALSE; // Unexpected error

		// Allocate a buffer for the dependencies.
		lpDependencies = (LPENUM_SERVICE_STATUS)HeapAlloc(
			GetProcessHeap(), HEAP_ZERO_MEMORY, dwBytesNeeded);

		if (!lpDependencies)
			return !result;

		__try {
			// Enumerate the dependencies.
			if (!EnumDependentServices(schService, SERVICE_ACTIVE,
				lpDependencies, dwBytesNeeded, &dwBytesNeeded,
				&dwCount))
			{
				//return FALSE;
				result = FALSE;
				__leave;
			}

			for (i = 0; i < dwCount; i++)
			{
				ess = *(lpDependencies + i);
				// Open the service.
				hDepService = OpenService(schSCManager,
					ess.lpServiceName,
					SERVICE_STOP | SERVICE_QUERY_STATUS);

				if (!hDepService)
				{
					//return FALSE;
					result = FALSE;
					__leave;
				}

				__try {
					// Send a stop code.
					if (!ControlService(hDepService,
						SERVICE_CONTROL_STOP,
						(LPSERVICE_STATUS)&ssp))
					{
						//return FALSE;
						result = FALSE;
						__leave;
					}

					// Wait for the service to stop.
					while (ssp.dwCurrentState != SERVICE_STOPPED)
					{
						Sleep(ssp.dwWaitHint);
						if (!QueryServiceStatusEx(
							hDepService,
							SC_STATUS_PROCESS_INFO,
							(LPBYTE)&ssp,
							sizeof(SERVICE_STATUS_PROCESS),
							&dwBytesNeeded))
						{
							//return FALSE;
							result = FALSE;
							__leave;
						}

						if (ssp.dwCurrentState == SERVICE_STOPPED)
							break;

						if (GetTickCount64() - dwStartTime > dwTimeout)
						{
							//return FALSE;
							result = FALSE;
							__leave;
						}
					}
				}
				__finally
				{
					// Always release the service handle.
					CloseServiceHandle(hDepService);
				}
			}
		}
		__finally
		{
			// Always free the enumeration buffer.
			HeapFree(GetProcessHeap(), 0, lpDependencies);
		}
	}
	return result;
}

void CServiceController::DoDeleteSvc(const TCHAR* sService)
{
	if (!sService)
		sService = mService.serviceName;

	SERVICE_STATUS ssStatus;
	ZeroMemory(&ssStatus, sizeof(ssStatus));

	// Get a handle to the SCM database. 
	schSCManager = OpenSCManager(
		NULL,					// local computer
		NULL,					// ServicesActive database 
		SC_MANAGER_ALL_ACCESS	// full access rights 
	);

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Get a handle to the service.
	schService = OpenService(
		schSCManager,	// SCM database 
		sService,		// name of service 
		DELETE			// need delete access 
	);

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}

	// Delete the service.
	if (!DeleteService(schService))
	{
		printf("DeleteService failed (%d)\n", GetLastError());
	}
	else printf("Service deleted successfully\n");

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);

	mTaskScheduler.removeTask(std::string(mService.serviceName));

}

void CServiceController::ReportSvcStatus(
	DWORD dwCurrentState,
	DWORD dwWin32ExitCode,
	DWORD dwWaitHint
)
{
	static DWORD dwCheckPoint = 1;

	// Fill in the SERVICE_STATUS structure.
	//SERVICE_STATUS serviceStatus = pSvcStatus;

	mService.serviceStatus.dwCurrentState = dwCurrentState;
	mService.serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
	mService.serviceStatus.dwWaitHint = dwWaitHint;

	if (mService.serviceStatus.dwCurrentState == SERVICE_START_PENDING)
		mService.serviceStatus.dwControlsAccepted = 0;
	else mService.serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if ((mService.serviceStatus.dwCurrentState == SERVICE_RUNNING) ||
		(mService.serviceStatus.dwCurrentState == SERVICE_STOPPED))
		mService.serviceStatus.dwCheckPoint = 0;
	else mService.serviceStatus.dwCheckPoint = dwCheckPoint++;

	// Report the status of the service to the SCM.
	SetServiceStatus(mService.serviceStatusHandle, &mService.serviceStatus);
}

//void ReportSvcStatus(
//	SERVICE_STATUS_HANDLE &svcStatusHandle,
//	SERVICE_STATUS &svcStatus
//)
//{
//	static DWORD dwCheckPoint = 1;
//
//	// Fill in the SERVICE_STATUS structure.
//	//SERVICE_STATUS serviceStatus = pSvcStatus;
//
//	/*mService.serviceStatus.dwCurrentState = dwCurrentState;
//	mService.serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
//	mService.serviceStatus.dwWaitHint = dwWaitHint;*/
//
//	if (svcStatus.dwCurrentState == SERVICE_START_PENDING)
//		svcStatus.dwControlsAccepted = 0;
//	else svcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
//
//	if ((svcStatus.dwCurrentState == SERVICE_RUNNING) ||
//		(svcStatus.dwCurrentState == SERVICE_STOPPED))
//		svcStatus.dwCheckPoint = 0;
//	else svcStatus.dwCheckPoint = dwCheckPoint++;
//
//	// Report the status of the service to the SCM.
//	SetServiceStatus(svcStatusHandle, &svcStatus);
//}
//
//void ReportSvcStatus(
//	DWORD dwCurrentState,
//	DWORD dwWin32ExitCode,
//	DWORD dwWaitHint
//)
//{
//	static DWORD dwCheckPoint = 1;
//
//	// Fill in the SERVICE_STATUS structure.
//	//SERVICE_STATUS serviceStatus = pSvcStatus;
//
//	gService.serviceStatus.dwCurrentState = dwCurrentState;
//	gService.serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
//	gService.serviceStatus.dwWaitHint = dwWaitHint;
//
//	if (gService.serviceStatus.dwCurrentState == SERVICE_START_PENDING)
//		gService.serviceStatus.dwControlsAccepted = 0;
//	else gService.serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
//
//	if ((gService.serviceStatus.dwCurrentState == SERVICE_RUNNING) ||
//		(gService.serviceStatus.dwCurrentState == SERVICE_STOPPED))
//		gService.serviceStatus.dwCheckPoint = 0;
//	else gService.serviceStatus.dwCheckPoint = dwCheckPoint++;
//
//	// Report the status of the service to the SCM.
//	SetServiceStatus(gService.serviceStatusHandle, &gService.serviceStatus);
//}

DWORD CServiceController::GetSvcStatus(const TCHAR* sService)
{
	if (!sService)
		sService = mService.serviceName;

	SERVICE_STATUS_PROCESS ssStatus;
	ZeroMemory(&ssStatus, sizeof(ssStatus));
	DWORD dwBytesNeeded;
	// Get ServiceManager Handle
	schSCManager = OpenSCManager(
		NULL,				// local computer
		NULL,				// servicesActive database
		SC_MANAGER_CONNECT	// full access rights
	);
	if (schSCManager == NULL)
	{
		return -1;
	}

	// Get ServiceHandle
	schService = OpenService(
		schSCManager,			// SCM database
		sService,				// Name of Service
		SERVICE_ALL_ACCESS		// Level of access
	);
	if (schService == NULL)
	{
		CloseServiceHandle(schSCManager);
		return -1;
	}

	if (!QueryServiceStatusEx(
		schService,						// service handler
		SC_STATUS_PROCESS_INFO,			// information level
		(LPBYTE)&ssStatus,				// address of structure
		sizeof(SERVICE_STATUS_PROCESS),	// size of structure
		&dwBytesNeeded					// size needed if buffer is too small
	))
	{
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return -1;
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
	return ssStatus.dwCurrentState;
}

void WINAPI CServiceController::SvcCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:
		ReportSvcStatus(CtrlCode, NO_ERROR, 0);

		SetEvent(mService.serviceStopEvent);
		ReportSvcStatus(mService.serviceStatus.dwCurrentState, NO_ERROR, 0);
		return;
	case SERVICE_CONTROL_INTERROGATE:
		break;
	default:
		break;
	}
}