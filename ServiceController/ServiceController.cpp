
#include "ServiceController.h"


//int InstallMySQL()
//{
//	if (ShellExecuteApp("C:\\wamp\\bin\\mysql\\mysql5.6.17\\bin\\mysqld.exe", "--install-manual wampmysql64"))
//		return 1;
//	return 0;
//}
//
//int InstallApache()
//{
//	if (ShellExecuteApp("C:\\wamp\\bin\\apache\\apache2.4.9\\bin\\httpd.exe", "-k install -n wampapache64"))
//		return 1;
//	return 0;
//}
//
//int InstallOnlineOfflineScript()
//{
//	if (ShellExecuteApp("C:\\wamp\\bin\\php\\php5.5.12\\php-win.exe", "-c C:\\wamp\\scripts\\onlineOffline.php"))
//		return 1;
//	return 0;
//}


ServiceController::ServiceController(const char* serviceName, const char* serviceDisplayName)
{
	schSCManager = nullptr;
	schService = nullptr;
	serviceDetail[0] = serviceName;
	serviceDetail[1] = serviceDisplayName;

	std::cout << serviceDetail[0] << ":" << serviceDetail[1];
	std::cout << std::endl;
}

ServiceController::~ServiceController()
{
	for(auto& s : serviceDetail)
	{
		s = nullptr;
	}
	schSCManager = nullptr;
	schService = nullptr;
}

void ServiceController::DoInstallSvc()
{
	TCHAR szUnquotedPath[MAX_PATH];

	if (!GetModuleFileName(NULL, szUnquotedPath, MAX_PATH))
	{
		printf("Cannot install service (%d)\n", GetLastError());
		return;
	}

	TCHAR szPath[MAX_PATH];
	StringCbPrintf(szPath, MAX_PATH, TEXT("\"%s\""), szUnquotedPath);
	std::string wPath(&szPath[0]);
	std::string sSZPath(wPath.begin(), wPath.end());

	schSCManager = OpenSCManagerA(
		NULL,					// local computer
		NULL,					// ServiceActive database
		SC_MANAGER_ALL_ACCESS	// full access rights
	);

	if (schSCManager == NULL) {
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}
	// SERVICE_NAME
	// SERVICE_DISPLAY_NAME
	schService = CreateServiceA(
		schSCManager,				 // SCM database 
		serviceDetail[0],				 // name of service 
		serviceDetail[1],		 // service name to display 
		SERVICE_ALL_ACCESS,			 // desired access 
		SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,	 // service type 
		SERVICE_AUTO_START,			 // start type 
		SERVICE_ERROR_NORMAL,		 // error control type 
		sSZPath.data(),				 // path to service's binary 
		NULL,						 // no load ordering group 
		NULL,						 // no tag identifier 
		NULL,						 // no dependencies 
		NULL,						 // LocalSystem account 
		""						 // no password 
	);

	if (schService == NULL) {
		printf("CreateService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}
	printf("Service installed successfully\n");
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}

void __stdcall ServiceController::DoStartSvc(const char* sService)
{

	if (!sService)
		sService = serviceDetail[0];

	SERVICE_STATUS_PROCESS ssStatus;
	ZeroMemory(&ssStatus, sizeof(ssStatus));
	DWORD dwOldCheckPoint;
	DWORD dwStartTickCount;
	DWORD dwWaitTime;
	DWORD dwBytesNeeded;

	// Get ServiceManager Handle
	schSCManager = OpenSCManagerA(
		NULL,					// local computer
		NULL,					// servicesActive database
		SC_MANAGER_ALL_ACCESS	// full access rights
	);

	if (schSCManager == NULL)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Get ServiceHandle
	schService = OpenServiceA(
		schSCManager,		// SCM database
		sService,		// Name of Service
		SERVICE_ALL_ACCESS	// Level of access
	);

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
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
		goto Exit;
		/*CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;*/
	}

	// Check service is not already running
	if (ssStatus.dwCurrentState != SERVICE_STOPPED && ssStatus.dwCurrentState != SERVICE_STOP_PENDING)
	{
		printf("Cannot start the service because it is already running\n");
		goto Exit;
		/*CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;*/
	}

	// Save tickCount and initial checkPoint
	dwStartTickCount = GetTickCount64();
	dwOldCheckPoint = ssStatus.dwCheckPoint;

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
			goto Exit;
			/*CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return;*/
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
			goto Exit;
			/*CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return;*/
		}
	}

	if (!StartServiceA(
		schService,
		0,
		NULL
	))
	{
		printf("StartService failed (%d)\n", GetLastError());
		goto Exit;
		/*DoStopSvc();
		DoDeleteSvc();*/
	}
	printf("Service start pending...\n");

	// Check the status until the service is no longer start pending. 
	if (!QueryServiceStatusEx(
		schService,                     // handle to service 
		SC_STATUS_PROCESS_INFO,         // info level
		(LPBYTE)&ssStatus,             // address of structure
		sizeof(SERVICE_STATUS_PROCESS), // size of structure
		&dwBytesNeeded))              // if buffer too small
	{
		printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
		/*CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;*/
		goto Exit;
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
		//DoDeleteSvc();
	}
	else
	{
		printf("Service started successfully.\n");
	}

Exit:
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
	return;
}

int __stdcall ServiceController::StartTargetSvc(const char* sService)
{
	if (!sService)
		sService = serviceDetail[0];

	// Get ServiceManager Handle
	schSCManager = OpenSCManagerA(
		NULL,					// local computer
		NULL,					// servicesActive database
		SC_MANAGER_ALL_ACCESS	// full access rights
	);

	if (schSCManager == NULL)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return 0;
	}


	schService = OpenServiceA(
		schSCManager,		// SCM database
		sService,			// Name of Service
		SERVICE_ALL_ACCESS	// Level of access
	);

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return 0;
	}

	if (!ChangeServiceConfigA(
		schService,				// Service Handle
		SERVICE_NO_CHANGE,		// Service Type
		SERVICE_AUTO_START,		// Service Start Condition
		SERVICE_NO_CHANGE,		// Service Error Response
		NULL,					// Target Service Path
		NULL,					// Load Order Group
		NULL,					// Tag ID
		NULL,					// Dependencies
		NULL,					// Service Account Name
		NULL,					// Service Account Password
		NULL					// Service Display Name
	)) {
		printf("Failed to change target service settings (%d)\n", GetLastError());
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return 0;
	}

	if (!StartServiceA(
		schService,
		0,
		NULL
	))
	{
		printf("StartService failed (%d)\n", GetLastError());
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return 0;
		/*DoStopSvc();
		DoDeleteSvc();*/
	}
	printf("Service start pending...\n");
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
	return 1;
}

void __stdcall ServiceController::DoStopSvc(const char* sService)
{
	if (!sService)
		sService = serviceDetail[0];

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
		return;
	}

	// Get a handle to the service.
	schService = OpenServiceA(
		schSCManager,					// SCM database 
		sService,					// name of service 
		SERVICE_STOP |
		SERVICE_QUERY_STATUS |
		SERVICE_ENUMERATE_DEPENDENTS
	);

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
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
		goto stop_cleanup;
	}

	if (ssp.dwCurrentState == SERVICE_STOPPED)
	{
		printf("Service is already stopped.\n");
		goto stop_cleanup;
	}

	// If a stop is pending, wait for it.
	while (ssp.dwCurrentState == SERVICE_STOP_PENDING)
	{
		printf("Service stop pending...\n");

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
			goto stop_cleanup;
		}

		if (ssp.dwCurrentState == SERVICE_STOPPED)
		{
			printf("Service stopped successfully.\n");
			goto stop_cleanup;
		}

		if (GetTickCount64() - dwStartTime > dwTimeout)
		{
			printf("Service stop timed out.\n");
			goto stop_cleanup;
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
		goto stop_cleanup;
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
			goto stop_cleanup;
		}

		if (ssp.dwCurrentState == SERVICE_STOPPED)
			break;

		if (GetTickCount64() - dwStartTime > dwTimeout)
		{
			printf("Wait timed out\n");
			goto stop_cleanup;
		}
	}
	printf("Service stopped successfully\n");
stop_cleanup:
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
	return;
}

bool __stdcall ServiceController::StopDependentServices()
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

void __stdcall ServiceController::DoDeleteSvc(const char* sService)
{
	if (!sService)
		sService = serviceDetail[0];

	SERVICE_STATUS ssStatus;
	ZeroMemory(&ssStatus, sizeof(ssStatus));

	// Get a handle to the SCM database. 
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Get a handle to the service.
	schService = OpenServiceA(
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
}

void ServiceController::ReportSvcStatus(
	DWORD dwCurrentState,
	DWORD dwWin32ExitCode,
	DWORD dwWaitHint
)
{
	static DWORD dwCheckPoint = 1;

	// Fill in the SERVICE_STATUS structure.

	g_ServiceStatus.dwCurrentState = dwCurrentState;
	g_ServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
	g_ServiceStatus.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		g_ServiceStatus.dwControlsAccepted = 0;
	else g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if ((dwCurrentState == SERVICE_RUNNING) ||
		(dwCurrentState == SERVICE_STOPPED))
		g_ServiceStatus.dwCheckPoint = 0;
	else g_ServiceStatus.dwCheckPoint = dwCheckPoint++;

	// Report the status of the service to the SCM.
	SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

DWORD ServiceController::GetSvcStatus(const char* sService)
{
	SERVICE_STATUS_PROCESS ssStatus;
	ZeroMemory(&ssStatus, sizeof(ssStatus));
	DWORD dwBytesNeeded;
	// Get ServiceManager Handle
	schSCManager = OpenSCManagerA(
		NULL,				// local computer
		NULL,				// servicesActive database
		SC_MANAGER_CONNECT	// full access rights
	);
	if (schSCManager == NULL)
	{
		return -1;
	}

	// Get ServiceHandle
	schService = OpenServiceA(
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


