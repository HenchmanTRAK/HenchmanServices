/// <summary>
/// Main Service for HenchmanTRAK Entry Point.
/// This script has the role of installing, controlling and managing the HenchmanService.
/// It must;
///  - allow the Service to be installed
///  - allow the Service to be started
///  - allow the Service to be stopped
///  - allow the Service to be deleted
///  - connect the various features from other scripts into a centralised point
///  - run the main Service Function
/// </summary>

#include "HenchmanService.h"

using namespace std;

stringstream HenchmanService::logx;
SOCKET HenchmanService::mailSocket = INVALID_SOCKET;
SSL_CTX* HenchmanService::ctx;
SSL* HenchmanService::ssl;
struct addrinfo* HenchmanService::mailAddrInfo = NULL;
string HenchmanService::app_path = "";
string HenchmanService::mail_username = "";
string HenchmanService::mail_password = "";
SQLite_Manager* HenchmanService::SQLiteM;
TRAKManager* TrakM;

QCoreApplication* a;

string ShowCerts(SSL* ssl)
{
	X509* cert;
	char* line = {};

	cert = SSL_get_peer_certificate(ssl);	// get the server's certificate
	if (cert != NULL)
	{
		string log = "Server certificates:\n";
		line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
		log.append("Subject: ").append(line);
		//free(line);							// free the malloc'ed string
		line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
		log.append("\nIssuer: ").append(line);
		//free(line);							// free the malloc'ed string
		X509_free(cert);					// free the malloc'ed certificate copy
		return log;
	}
	else {
		return "No certificates.\n";
	}
}

// InitCTX - initialize the SSL engine.
SSL_CTX* InitCTX(void)
{
	const SSL_METHOD* method;
	SSL_CTX* ctx;

	SSL_library_init();
	SSL_load_error_strings();			// Bring in and register error messages
	OpenSSL_add_all_algorithms();		// Load cryptos

	method = SSLv23_client_method();	// Create new client-method instance
	ctx = SSL_CTX_new(method);			// Create new context
	if (ctx == NULL)
	{
		ERR_print_errors_fp(stderr);
		// abort();
	}
	return ctx;
}

void sslError(SSL* ssl, int received, stringstream& logi)
{
	const int err = SSL_get_error(ssl, received);
	string microtime = to_string(microseconds());
	// const int st = ERR_get_error();
	if (err == SSL_ERROR_NONE) {
		// OK send
		// cout<<"SSL_ERROR_NONE:"<<SSL_ERROR_NONE<<endl;
		// SSL_shutdown(ssl);        
	}
	else if (err == SSL_ERROR_WANT_READ) {
		logi << "[SSL_ERROR_WANT_READ]" << SSL_ERROR_WANT_READ << endl;
		logi << logi.str() << "--[" << microtime << "]--" << endl;
		cerr << logi.str() << endl;
		//WriteToError(logi.str());
		SSL_shutdown(ssl);
		//kill(getpid(), SIGKILL);
	}
	else if (SSL_ERROR_SYSCALL) {
		logi << errno << " Received " << received << endl;
		logi << "[SSL_ERROR_SYSCALL] " << SSL_ERROR_SYSCALL << endl;
		logi << logi.str() << "--[" << microtime << "]--" << endl;
		//WriteToError(logi.str());
		cerr << logi.str() << endl;
		SSL_shutdown(ssl);
		//kill(getpid(), SIGKILL);
	}
}

const char* GetMimeTypeFromFileName(char* szFileExt)
{
	// cout << "EXT " << szFileExt;
	for (unsigned int i = 0; i < sizeof(MimeTypes) / sizeof(MimeTypes[0]); i++)
	{
		if (strcmp(MimeTypes[i][0], szFileExt) == 0)
		{
			return MimeTypes[i][1];
		}
	}
	return MimeTypes[0][1];   //if does not match any,  "application/octet-stream" is returned
}

bool ProcessExists(string& exeFileName)
{
	HANDLE SnapshotHandle;
	SnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 ProcessEntry32;
	ZeroMemory(&ProcessEntry32, sizeof(ProcessEntry32));
	ProcessEntry32.dwSize = sizeof(ProcessEntry32);
	bool ContinueLoop = Process32First(SnapshotHandle, &ProcessEntry32);
	bool result = false;
	while (ContinueLoop) {
		string targetEXE = exeFileName;
		transform(targetEXE.begin(), targetEXE.end(), targetEXE.begin(), ::toupper);
		string processEXE = QString::fromWCharArray(ProcessEntry32.szExeFile).toStdString();
		transform(processEXE.begin(), processEXE.end(), processEXE.begin(), ::toupper);
		string processEXEFileName = fileBasename(QString::fromWCharArray(ProcessEntry32.szExeFile).toStdString());
		transform(processEXEFileName.begin(), processEXEFileName.end(), processEXEFileName.begin(), ::toupper);
		if ((processEXEFileName == targetEXE) || (processEXE == targetEXE)) {
			result = true;
		}
		else {
			ContinueLoop = Process32Next(SnapshotHandle, &ProcessEntry32);
		}
	}
	CloseHandle(SnapshotHandle);
	return result;
}

bool FileInUse(string fileName) {
	HANDLE fileRes;
	//struct stat buffer;
	cout << "Checking if: " << fileName << " is being used" << endl;
	bool result = false;
	if (filesystem::exists(fileName)) {
		cout << "Target File Exists" << endl;
		fileRes = CreateFileA(fileName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		result = fileRes == INVALID_HANDLE_VALUE;
		CloseHandle(fileRes);
		return result;
	}
	cout << "Target File Does Not Exists Or Could Not Be Found" << endl;
	return result;
}

int ShellExecuteApp(string appName, string params)
{
	SHELLEXECUTEINFOA SEInfo;
	DWORD ExitCode;
	string exeFile = appName;
	string paramStr = params;
	string StartInString;

	// fine the windows handle using https://learn.microsoft.com/en-us/troubleshoot/windows-server/performance/obtain-console-window-handle

	Sleep(2500);

	//fill_n(SEInfo, sizeof(SEInfo), NULL);
	ZeroMemory(&SEInfo, sizeof(SEInfo));
	SEInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	SEInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	//SEInfo.hwnd = NULL;
	//SEInfo.lpVerb = NULL;
	//SEInfo.lpDirectory = NULL;
	SEInfo.lpFile = exeFile.c_str();
	SEInfo.lpParameters = paramStr.c_str();
	//: SW_HIDE
	SEInfo.nShow = paramStr == "" && SW_NORMAL;
	if (ShellExecuteExA(&SEInfo)) {
		stringstream exitCodeMessage;
		do {
			GetExitCodeProcess(SEInfo.hProcess, &ExitCode);
			exitCodeMessage << "Target: " << exeFile << " return exit code : " << ExitCode;
			WriteToError(exitCodeMessage.str());
			exitCodeMessage.clear();
		} while (ExitCode != STILL_ACTIVE);
		return 1;
	}

	return 0;
}

int InstallMySQL()
{
	if (ShellExecuteApp("C:\\wamp\\bin\\mysql\\mysql5.6.17\\bin\\mysqld.exe", "--install-manual wampmysql64"))
		return 1;
	return 0;
}

int InstallApache()
{
	if (ShellExecuteApp("C:\\wamp\\bin\\apache\\apache2.4.9\\bin\\httpd.exe", "-k install -n wampapache64"))
		return 1;
	return 0;
}

int InstallOnlineOfflineScript()
{
	if (ShellExecuteApp("C:\\wamp\\bin\\php\\php5.5.12\\php-win.exe", "-c C:\\wamp\\scripts\\onlineOffline.php"))
		return 1;
	return 0;
}

void DoInstallSvc()
{
	TCHAR szUnquotedPath[MAX_PATH];

	if (!GetModuleFileName(NULL, szUnquotedPath, MAX_PATH))
	{
		printf("Cannot install service (%d)\n", GetLastError());
		return;
	}

	TCHAR szPath[MAX_PATH];
	StringCbPrintf(szPath, MAX_PATH, TEXT("\"%s\""), szUnquotedPath);
	wstring wPath(&szPath[0]);
	string sSZPath(wPath.begin(), wPath.end());

	schSCManager = OpenSCManagerA(
		NULL,					// local computer
		NULL,					// ServiceActive database
		SC_MANAGER_ALL_ACCESS	// full access rights
	);

	if (schSCManager == NULL) {
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	schService = CreateServiceA(
		schSCManager,				// SCM database 
		SERVICE_NAME,				// name of service 
		SERVICE_DISPLAY_NAME,		// service name to display 
		SERVICE_ALL_ACCESS,			// desired access 
		SERVICE_WIN32_OWN_PROCESS,	// service type 
		SERVICE_AUTO_START,			// start type 
		SERVICE_ERROR_NORMAL,		// error control type 
		sSZPath.c_str(),			// path to service's binary 
		NULL,						// no load ordering group 
		NULL,						// no tag identifier 
		NULL,						// no dependencies 
		NULL,						// LocalSystem account 
		NULL						// no password 
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

void __stdcall DoStartSvc(const char* sService)
{
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

int __stdcall StartTargetSvc(const char* sService)
{
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

void __stdcall DoStopSvc(const char* sService)
{
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
	WriteToLog("Service has stopped");
stop_cleanup:
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
	return;
}

bool __stdcall StopDependentServices()
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

void __stdcall DoDeleteSvc(const char* sService)
{
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

void ReportSvcStatus(
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

DWORD GetSvcStatus(const char* sMachine, const char* sService)
{
	SERVICE_STATUS_PROCESS ssStatus;
	ZeroMemory(&ssStatus, sizeof(ssStatus));
	DWORD dwBytesNeeded;
	// Get ServiceManager Handle
	schSCManager = OpenSCManagerA(
		sMachine,			// local computer
		NULL,				// servicesActive database
		SC_MANAGER_CONNECT	// full access rights
	);
	if (schSCManager == NULL)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return -1;
	}

	// Get ServiceHandle
	schService = OpenServiceA(
		schSCManager,			// SCM database
		sService,				// Name of Service
		SERVICE_QUERY_STATUS	// Level of access
	);
	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
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
		printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return -1;
		//goto Exit;
	}
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
	return ssStatus.dwCurrentState;
}

void WINAPI SvcCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:
		ReportSvcStatus(CtrlCode, NO_ERROR, 0);

		SetEvent(g_ServiceStopEvent);
		ReportSvcStatus(g_ServiceStatus.dwCurrentState, NO_ERROR, 0);
		return;
	case SERVICE_CONTROL_INTERROGATE:
		break;
	default:
		break;
	}
}

void WINAPI SvcMain(int dwArgc, char* lpszArgv[])
{
	g_StatusHandle = RegisterServiceCtrlHandlerA(
		SERVICE_NAME,
		SvcCtrlHandler
	);

	if (!g_StatusHandle)
	{
		return;
	}

	a = new QCoreApplication(dwArgc, lpszArgv);

	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwServiceSpecificExitCode = 0;

	ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

	SvcInit();

	delete a;
}

void SvcInit()
{
	g_ServiceStopEvent = CreateEventA(
		NULL,
		TRUE,
		FALSE,
		NULL
	);

	if (g_ServiceStopEvent == NULL)
	{
		ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
		return;
	}

	ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

	// Do Work Here
	HANDLE hThread = CreateThread(NULL, 0, SvcWorkerThread, &g_ServiceStopEvent, 0, NULL);

	if (hThread)
		WaitForSingleObject(
			g_ServiceStopEvent,
			INFINITE
		);

	ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
	CloseHandle(g_ServiceStopEvent);

	return;

}

DWORD WINAPI SvcWorkerThread(LPVOID lpParam)
{

	HenchmanService service;

	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		service.MainFunction();
		WriteToLog("Service sleeping for 30000 ms...");
		Sleep(30000);
	}
	return ERROR_SUCCESS;
}

int RunAsService(int argc, char* argv[])
{
	if (lstrcmpiA(argv[1], "--install") == 0)
	{
		HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));
		char buff[MAX_PATH];
		int byteLength = GetCurrentDirectoryA(sizeof(buff), buff);
		string currDir = buff;
		currDir.resize(byteLength);
		string installDir = GetStrVal(hKey, "Install_DIR", REG_SZ);
		struct stat buffer;
		if (installDir == "" || stat(currDir.c_str(), &buffer) == 0)
		{
			installDir = currDir;
			cout << installDir << endl;
			SetStrVal(hKey, "Install_DIR", installDir, REG_SZ);
		}
		RegCloseKey(hKey);
		getchar();
		DoInstallSvc();
		ShellExecuteApp(SERVICE_NAME, " --start");
		return 0;
	}

	if (lstrcmpiA(argv[1], "--remove") == 0)
	{
		DoStopSvc();
		DoDeleteSvc();
		return 0;
	}

	if (lstrcmpiA(argv[1], "--start") == 0)
	{
		DoStartSvc();
		return 0;
	}

	if (lstrcmpiA(argv[1], "--stop") == 0)
	{
		DoStopSvc();
		return 0;
	}

	SERVICE_TABLE_ENTRYA ServiceTable[] =
	{
		{string(SERVICE_NAME).data(), (LPSERVICE_MAIN_FUNCTIONA)SvcMain},
		{NULL, NULL}
	};

	if (!StartServiceCtrlDispatcherA(ServiceTable))
	{
		std::cout << "StartServiceCtrlDispatcher Failed" << std::endl;
		/*SvcReportEvent(TEXT("StartServiceCtrlDispatcher"));
		ErrorMessage(TEXT("GetProcessId"));*/
		return 0;
	}

	return 0;
}

HenchmanService::HenchmanService()
{
	/*HenchmanService::report = false;
	HenchmanService::update = false;*/
	//CSimpleIni ini;
	ini.SetUnicode();
	logx << "Service Has Started" << endl << endl;

	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));

	char buff[MAX_PATH];
	int byteLength = GetCurrentDirectoryA(sizeof(buff), buff);
	string currDir = buff;
	currDir.resize(byteLength);
	string installDir = GetStrVal(hKey, "Install_DIR", REG_SZ);
	//struct stat buffer;
	/*if (installDir == "" || stat(currDir.c_str(), &buffer) == 0)
	{
		installDir = currDir;
		SetStrVal(hKey, "Install_DIR", installDir, REG_SZ);
	}*/
	string databaseName = GetStrVal(hKey, "DatabaseName", REG_SZ);
	string dbName = "henchmanService.db3";
	if (databaseName == "" || databaseName != dbName)
	{
		databaseName = dbName;
		SetStrVal(hKey, "DatabaseName", databaseName, REG_SZ);
	}

	SI_Error rc = ini.LoadFile((installDir + "\\service.ini").c_str());
	if (rc < 0) {
		cerr << "Failed to Load INI File" << endl;
	}

	CSimpleIniA::TNamesDepend keys;
	ini.GetAllKeys("WAMP", keys);
	for (auto const& val : keys)
	{
		string key = val.pItem;
		string value = ini.GetValue("WAMP", val.pItem, "");
		removeQuotes(value);

		GetStrVal(hKey, key.data(), REG_SZ);
		SetStrVal(hKey, key.data(), value, REG_SZ);

		key.clear();
		value.clear();
	}

	ini.GetAllKeys("TRAK", keys);
	for (auto const& val : keys)
	{
		string key = val.pItem;
		string value = ini.GetValue("TRAK", val.pItem, "");
		removeQuotes(value);

		GetStrVal(hKey, key.data(), REG_SZ);
		SetStrVal(hKey, key.data(), value, REG_SZ);

		key.clear();
		value.clear();
	}

	//cout << installDir + "\\" << endl << databaseName << endl;
	SQLiteM = new SQLite_Manager(installDir + "\\", databaseName.data());

	//SQLiteM.ToggleConsoleLogging();

	SQLiteM->InitDB();

	string tableName = "TestTable";
	vector<string> cols;
	cols.push_back("username TEXT NOT NULL");
	cols.push_back("password TEXT NOT NULL");
	SQLiteM->CreateTable(tableName, cols);

	//cout << "Reading ini file: " << string(installDir + "\\service.ini") << endl;
	string username = ini.GetValue("Email", "Username", "");
	string password = base64(ini.GetValue("Email", "Password", ""));
	//string encodedPass = base64(password);
	if (checkForInternetConnection() && isInternetConnected() && setMailLogin(username, password)) {
		cout << "Able to send mail" << endl;
		//service.ConnectWithSMTP();
	}

	tableName.clear();
	cols.clear();
	currDir.clear();

	RegCloseKey(hKey);
}

HenchmanService::~HenchmanService()
{
	//cout << "Deconstructing HenchmanService" << endl;
	delete SQLiteM;
	delete TrakM;

	logx.clear();
}

vector<string> HenchmanService::Explode(const string& Seperator, string& s)
{
	int limit = -1;
	return Explode(Seperator, s, limit);
}

vector<string> HenchmanService::Explode(const string& Seperator, string& s, int& limit)
{
	if (s == "")
		throw HenchmanServiceException("No String was Provided");
	if (limit < 0)
		throw HenchmanServiceException("Invalid Integer Provided");

	vector<string> results;
	if (Seperator == "") {
		results.push_back(s);
	}
	else {
		size_t pos = 0;
		string token;
		while ((pos = s.find(Seperator)) != string::npos and (limit == 0 ? true : results.size() <= limit)) {
			cout << results.size() << endl;
			token = s.substr(0, pos);
			results.push_back(token);
			s.erase(0, pos + Seperator.length());
		}
		results.push_back(s);
		token.clear();
	}
	return results;
}

bool HenchmanService::setMailLogin(string& username, string& password) {
	if (username.length() <= 1 || password.length() <= 1) {
		return false;
	}
	mail_username = username;
	mail_password = password;
	string tableName = "TestTable";
	vector<string> cols;
	cols.push_back(username);
	cols.push_back(password);
	SQLiteM->AddRow(tableName, cols);
	cols.clear();
	return true;
}

void HenchmanService::ConnectWithSMTP() {
	WSADATA wsaData;
	int iResult;

	struct sockaddr_in clientService;
	ZeroMemory(&clientService, sizeof(clientService));
	vector<string> files;

	ctx = InitCTX();

	char buff[1024];
	int buffLen = sizeof(buff);

	struct addrinfo hints;
	string reply;
	int iProtocolPort;
	try {
		logx << "Socket setup started" << endl;
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != NO_ERROR) {
			printf("WSAStartup failed: %d\n", iResult);
			WriteToError("Failed To Setup Socket");
			return;
		}

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		logx << "Getting Mail Address Info" << endl;
		iResult = getaddrinfo("mail.henchmantrak.com", "smtp", &hints, &mailAddrInfo);
		if (iResult != NO_ERROR) {
			printf("getaddrinfo failed with error: %d\n", iResult);
			WriteToError("getaddrinfo failed with error: " + iResult);
			freeaddrinfo(mailAddrInfo);
			WSACleanup();
			return;
		}
		logx << "Setting up SMTP Mail Socket" << endl;
		mailSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (mailSocket == INVALID_SOCKET) {
			printf("Failed to connect to Socket: %ld\n", WSAGetLastError());
			WriteToError("Failed to connect to Socket: " + WSAGetLastError());
			freeaddrinfo(mailAddrInfo);
			WSACleanup();
			return;
		}
		logx << "Getting Mail Service Port" << endl;
		LPSERVENT lpServEntry = getservbyname("mail", 0);
		if (!lpServEntry) {
			logx << "Using IPPORT_SMTP" << endl;
			iProtocolPort = htons(IPPORT_SMTP);
			//iProtocolPort = 465;
		}
		else {
			logx << "Using port provided from lpServEntry" << endl;
			iProtocolPort = lpServEntry->s_port;
		}
		cout << "Connecting on port: " << iProtocolPort << endl;

		clientService.sin_family = AF_INET;
		//clientService.sin_addr.s_addr = inet_addr("192.168.2.36");
		clientService.sin_port = iProtocolPort;
		inet_pton(AF_INET, inet_ntoa(((struct sockaddr_in*)mailAddrInfo->ai_addr)->sin_addr), (SOCKADDR*)&clientService.sin_addr.s_addr);
		logx << "Connecting to Mail Socket" << endl;
		iResult = connect(mailSocket, (SOCKADDR*)&clientService, sizeof(clientService));
		if (iResult == SOCKET_ERROR) {
			printf("Unable to connect to server: %ld\n", WSAGetLastError());
			WriteToError("Unable to connect to server: " + WSAGetLastError());
			WSACleanup();
			freeaddrinfo(mailAddrInfo);
			return;
		}
		else {
			logx << "Connected to: " << inet_ntoa(clientService.sin_addr) << " on port: " << iProtocolPort << endl;
		}

		logx << "Initiating communication through SMTP" << endl;
		char szMsgLine[255] = "";
		sprintf(szMsgLine, "HELO %s%s", "mail.henchmantrak.com", CRLF);
		string E1 = "ehlo ";
		E1.append("mail.henchmantrak.com");
		E1.append(CRLF);
		char* hello = (char*)E1.c_str();
		char* hellotls = (char*)"STARTTLS\r\n";

		// initiate connection
		iResult = recv(mailSocket, buff, sizeof(buff), 0);
		reply = buff;
		reply.resize(iResult);
		logx << "[Server] [" << iResult << "] " << buff << endl;
		logx.adjustfield;

		memset(buff, 0, sizeof buff);
		buff[0] = '\0';

		// send ehlo command
		send(mailSocket, hello, strlen(hello), 0);
		logx << "[HELO] " << hello << " " << iResult << endl;

		iResult = recv(mailSocket, buff, sizeof(buff), 0);
		reply = buff;
		reply.resize(iResult);
		logx << "[Server] [" << iResult << "] " << reply << endl;

		while (!Contain(string(buff), "250 ")) {
			iResult = recv(mailSocket, buff, sizeof(buff), 0);
			if (Contain(string(buff), "501 ") || Contain(string(buff), "503 ")) {
				//cout << logx.str();
				WriteToError(logx.str());
				freeaddrinfo(mailAddrInfo);
				closesocket(mailSocket);
				WSACleanup();
				logx.str(string());
				return;
			}
		}

		if (!Contain(string(buff), "STARTTLS")) {
			logx << "[EXTERNAL_SERVER_NO_TLS] " << "mail.henchmantrak.com" << " " << buff << "[CLOSING_CONNECTION]" << endl;
			//cout << logx.str();
			WriteToError(logx.str());
			closesocket(mailSocket);
			freeaddrinfo(mailAddrInfo);
			WSACleanup();
			logx.str(string());
			return;
		}

		memset(buff, 0, sizeof buff);
		buff[0] = '\0';
		char buff1[1024];

		// starttls connecion
		send(mailSocket, hellotls, strlen(hellotls), 0);
		logx << "[STARTTLS] " << hellotls << endl;

		iResult = recv(mailSocket, buff1, sizeof(buff1), 0);
		reply = buff1;
		reply.resize(iResult);
		logx << "[Server] " << iResult << " " << buff << endl;

		ctx = InitCTX();
		ssl = SSL_new(ctx);
		SSL_set_fd(ssl, mailSocket);

		logx << "Connection to smtp via tls" << endl;

		if (SSL_connect(ssl) == -1) {
			// ERR_print_errors_fp(stderr);            
			logx << "[TLS_SMTP_ERROR]" << endl;
			//cout << logx.str();
			WriteToError(logx.str());
			freeaddrinfo(mailAddrInfo);
			closesocket(mailSocket);
			WSACleanup();
			logx.str(string());
			return;
		}
		else {
			// char *msg = (char*)"{\"from\":[{\"name\":\"Zenobiusz\",\"email\":\"email@eee.ddf\"}]}";
			logx << "Connected with " << SSL_get_cipher(ssl) << " encryption" << endl;
			string cert = ShowCerts(ssl);
			//cout << cert << endl;

			vector<string> attachments;
			for (const auto& entry : filesystem::directory_iterator(GetExportsPath())) {
				cout << entry.path().string() << endl;
				attachments.push_back(entry.path().string());
			}
			logx << "---" << "Generating and sending Email" << "---\r\n" << endl;
			//cout << logx.str();
			WriteToLog(logx.str());
			logx.str(string());
			SendEmail(ssl, attachments);
		}
		sslError(ssl, 1, logx);

		closesocket(mailSocket);
		SSL_CTX_free(ctx);
		freeaddrinfo(mailAddrInfo);
		WSACleanup();
	}
	catch (exception& e) {
		//cout << logx.str();
		WriteToError(logx.str());
	}
	logx.str(string());
}

void HenchmanService::SendEmail(SSL*& ssl, vector<string> attachments) {

	char buff[1024] = "\0";
	int buffLen = sizeof(buff);
	int counter = 1;
	try {
		if (SSL_connect(ssl) == -1) {
			// ERR_print_errors_fp(stderr);            
			logx << "[TLS_SMTP_ERROR]" << endl;
			//cout << logx.str();
			return;
		}
		else {
			buff[0] = '\0';
			ostringstream f0;
			f0 << "EHLO " << "mail.henchmantrak.com" << "\r\n";
			string f00 = f0.str();
			char* helo = (char*)f00.c_str();
			logx << "SEND TO SERVER " << helo << endl;
			SSL_write(ssl, helo, strlen(helo));
			int bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << endl;
			if (!Contain(string(buff), "250")) return;
			counter++;

			buff[0] = '\0';
			ostringstream f1;
			f1 << "AUTH PLAIN ";
			string f2;
			using namespace string_literals;
			f2 = mail_username + "\0"s + mail_username + "\0"s + decodeBase64(mail_password);
			f1 << base64(f2);
			f1 << " \r\n";
			string f11 = f1.str();
			char* auth = f11.data();
			logx << "SEND TO SERVER " << auth << endl;
			SSL_write(ssl, auth, strlen(auth));
			bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << endl;
			if (!Contain(string(buff), "235"))return;
			counter++;

			buff[0] = '\0';
			ostringstream f4;
			f4 << "mail from: <" << "test@henchmantrak.com" << ">\r\n";
			string f44 = f4.str();
			char* fromemail = (char*)f44.c_str();
			logx << "SEND TO SERVER " << fromemail << endl;
			SSL_write(ssl, fromemail, strlen(fromemail));
			bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << endl;
			if (!Contain(string(buff), "250"))return;
			counter++;


			buff[0] = '\0';
			string rcpt = "rcpt to: <";
			rcpt.append("wjaco.swanepoel@gmail.com").append(">\r\n");
			char* rcpt1 = (char*)rcpt.c_str();
			logx << "SEND TO SERVER " << rcpt1 << endl;
			SSL_write(ssl, rcpt1, strlen(rcpt1));
			bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << endl;
			if (!Contain(string(buff), "250"))return;
			counter++;

			buff[0] = '\0';
			char* data = (char*)"DATA\r\n";
			logx << "SEND TO SERVER " << data << endl;
			SSL_write(ssl, data, strlen(data));
			bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << endl;
			if (!Contain(string(buff), "354"))return;
			counter++;

			string Encoding = "iso-8859-2"; // charset: utf-8, utf-16, iso-8859-2, iso-8859-1
			Encoding = "utf-8";

			string subject = "Testin Mailer";
			string msg = "This is a test Message";

			// add html page layout
			stringstream msghtml;

			msghtml << "<!DOCTYPE html>\n";
			msghtml << "<HTML lang = 'en'>\n";
			msghtml << "<body>\n";
			msghtml << "	<p>Please find attched report(/ s).</p>\n";

			// 
			// add atachments
			//vector<string> files = Explode(", ", attachments);

			stringstream attachmentSection;
			vector<string>files = attachments;
			if (files.size() > 0) {
				for (unsigned int i = 0;i < files.size();i++) {
					string path = files.at(i);
					string filename = fileBasename(path);
					string fc = base64(get_file_contents(path.c_str()));
					string extension = GetFileExtension(filename);
					const char* mimetype = GetMimeTypeFromFileName((char*)extension.c_str());
					if (!(extension == "csv"))
						continue;
					attachmentSection << "--ToJestSeparator0000\r\n";
					attachmentSection << "Content-Type: " << mimetype << "; name=\"" << filename << "\"\r\n";
					attachmentSection << "Content-Disposition: attachment; filename=\"" << filename << "\"\r\n";
					attachmentSection << "Content-Transfer-Encoding: base64" << "\r\n";
					attachmentSection << "Content-ID: <" << base64(filename) << ">\r\n";
					attachmentSection << "X-Attachment-Id: " << base64(filename) << "\r\n";
					attachmentSection << "\r\n";
					attachmentSection << fc << "\r\n";
					attachmentSection << "\r\n";
				}
			}

			msghtml << "</body>\n";

			stringstream m;
			m << "X-Priority: " << "1" << "\r\n";
			m << "From: " << "willem.swanepoel@henchmantools.com" << "\r\n";
			m << "To: " << "wjaco.swanepoel@gmail.com" << "\r\n";
			m << "Subject: =?" << Encoding << "?Q?" << subject << "?=\r\n";
			m << "Reply-To: " << "willem.swanepoel@henchmantools.com" << "\r\n";
			m << "Return-Path: " << "willem.swanepoel@henchmantools.com" << "\r\n";
			m << "MIME-Version: 1.0\r\n";
			m << "Content-Type: multipart/mixed; boundary=\"ToJestSeparator0000\"\r\n\r\n";
			m << "--ToJestSeparator0000\r\n";
			m << "Content-Type: multipart/alternative; boundary=\"ToJestSeparatorZagniezdzony1111\"\r\n\r\n";

			m << "--ToJestSeparatorZagniezdzony1111\r\n";
			m << "Content-Type: text/plain; charset=\"" << Encoding << "\"\r\n";
			m << "Content-Transfer-Encoding: quoted-printable\r\n\r\n";
			m << msg << "\r\n\r\n";
			m << "--ToJestSeparatorZagniezdzony1111\r\n";
			m << "Content-Type: text/html; charset=\"" << Encoding << "\"\r\n";
			m << "Content-Transfer-Encoding: quoted-printable\r\n\r\n";
			m << msghtml.str() << "\r\n\r\n";
			m << "--ToJestSeparatorZagniezdzony1111--\r\n\r\n";
			m << attachmentSection.str();
			m << "--ToJestSeparator0000--\r\n\r\n";
			m << "\r\n.\r\n";

			// create mime message string
			string mimemsg = m.str();
			logx << "Email body being sent: " << mimemsg.data() << endl;
			char* mdata = mimemsg.data();
			SSL_write(ssl, mdata, strlen(mdata));
			bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << endl;
			counter++;

			// send log
			//cout << logx.str();
			WriteToLog(logx.str());
			if (!Contain(string(buff), "250"))return;

			char* qdata = (char*)"quit\r\n";
			SSL_write(ssl, qdata, strlen(qdata));
			bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << endl;
			if (!Contain(string(buff), "221"))return;

			SSL_free(ssl);
		}

	}
	catch (exception& e) {
		//cout << logx.str();
		WriteToError(logx.str());
	}
	logx.str(string());
}

bool  HenchmanService::checkForInternetConnection()
{
	bool returnState = false;
	if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) {
		INetworkListManager* pNetworkListManager = NULL;
		VARIANT_BOOL isConnected;
		if (SUCCEEDED(CoCreateInstance(CLSID_NetworkListManager, NULL, CLSCTX_ALL, IID_INetworkListManager, (LPVOID*)&pNetworkListManager))) {
			logx << "Successfully created instance of network list manager." << endl;

			if (SUCCEEDED(pNetworkListManager->get_IsConnectedToInternet(&isConnected))) {
				logx << "Confirming existence of internet connection" << endl;
				if (!isConnected) goto Exit;

				logx << "Internet Connection was Confirmed." << endl;
				returnState = isConnected;
				goto Exit;
			}
			logx << "Could not confirm existence of internet connection" << endl;
			printf("internet not connected");
			goto Exit;
		}
		logx << "Failed to create instance of network list manager." << endl;
		goto Exit;
	}
Exit:
	CoUninitialize();
	//cout << logx.str();
	WriteToLog(logx.str());
	logx.str(string());
	return returnState;
}

bool  HenchmanService::isInternetConnected()
{
	WSADATA wsaData;
	int iResult;
	SOCKET ConnectionCheck = INVALID_SOCKET;
	struct sockaddr_in clientService;
	ZeroMemory(&clientService, sizeof(clientService));
	struct addrinfo* httpAddrInfo = NULL;
	struct addrinfo hints;

	try {
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != NO_ERROR) {
			printf("WSAStartup failed: %d\n", iResult);
			WriteToError("WSAStartup failed: " + iResult);
			return false;
		}

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_protocol = IPPROTO_TCP;

		logx << "Getting Address Info" << endl;
		iResult = getaddrinfo("www.google.com", "https", &hints, &httpAddrInfo);
		if (iResult != NO_ERROR) {
			printf("getaddrinfo failed with error: %d\n", iResult);
			freeaddrinfo(httpAddrInfo);
			WriteToError("getaddrinfo failed with error: " + iResult);
			WSACleanup();
			return false;
		}

		logx << "Setting up Network Check Socket" << endl;
		ConnectionCheck = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (ConnectionCheck == INVALID_SOCKET) {
			printf("Failed to connect to Socket: %ld\n", WSAGetLastError());
			WriteToError("Failed to connect to Socket: " + WSAGetLastError());
			closesocket(ConnectionCheck);
			freeaddrinfo(httpAddrInfo);
			WSACleanup();
			return false;
		}

		clientService.sin_family = AF_INET;
		//clientService.sin_addr.s_addr = inet_addr("192.168.2.36");
		clientService.sin_port = htons(IPPORT_HTTPS);
		inet_pton(AF_INET, inet_ntoa(((struct sockaddr_in*)httpAddrInfo->ai_addr)->sin_addr), (SOCKADDR*)&clientService.sin_addr.s_addr);
		logx << "Connecting to Google.com via Socket" << endl;
		iResult = connect(ConnectionCheck, (SOCKADDR*)&clientService, sizeof(clientService));
		if (iResult == SOCKET_ERROR) {
			printf("Unable to connect to server: %ld\n", WSAGetLastError());
			WriteToError("Unable to connect to server: " + WSAGetLastError());
			closesocket(ConnectionCheck);
			freeaddrinfo(httpAddrInfo);

			WSACleanup();
			return false;
		}
		else {
			logx << "Connected to: " << inet_ntoa(clientService.sin_addr) << " on port: " << clientService.sin_port << endl;
		}
		//cout << logx.str();
		WriteToLog(logx.str());
		closesocket(ConnectionCheck);
		freeaddrinfo(httpAddrInfo);
		WSACleanup();
	}
	catch (exception& e) {
		//cout << logx.str();
		WriteToError(logx.str());
		logx.str(string());
		return false;
	}
	logx.str(string());
	return true;
}

int HenchmanService::MainFunction()
{
	QTimer::singleShot(0, &*a, &QCoreApplication::quit);

	GetLogsPath();

	TrakM = new TRAKManager;

	update = TRUE;

	/*WriteToLog("Checking if WampServer is running and Starting if not");
	string wampServerManagerEXE = "wampmanager";
	if (!ProcessExists(wampServerManagerEXE)) {
		WriteToLog("WampServer was not running. Starting WampServerManager Now");
		ShellExecuteApp("C:\\wamp\\" + wampServerManagerEXE + ".exe", "");
	}*/

	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));

	int wampMySQLSvcStatus = GetSvcStatus(NULL, "wampmysqld64");
	string mysql_dir = GetStrVal(hKey, "MySQL_DIR", REG_SZ);
	cout << mysql_dir << endl;
	//wampMySQLSvcStatus = 4;
	switch (wampMySQLSvcStatus) {
	case -1: {
		WriteToError("MySQL Service errored with unknown error");
		/*if(ShellExecuteApp(mysql_dir + "\\mysqld.exe", " --remove wampmysqld64"))
			WriteToLog("Removed Local MYSQL service...");*/
		if (ShellExecuteApp(mysql_dir + "\\mysqld.exe", " --install-manual wampmysqld64"))
			WriteToLog("Local MYSQL service installed...");
		if (StartTargetSvc("wampmysqld64"))
			WriteToLog("Local MYSQL Service Started");
		else {
			if (ShellExecuteApp(mysql_dir + "\\mysqld.exe", " --remove wampmysqld64"))
				WriteToLog("Removed Local MYSQL service...");
		}
		break;

	}
	case 1: {
		WriteToLog("Local MYSQL Service has stopped...");
		if (StartTargetSvc("wampmysqld64"))
			WriteToLog("Successfully Restarted Local MYSQL Service");
		else {
			WriteToLog("Failed to start Local MYSQL Service...");
			if (ShellExecuteApp(mysql_dir + "\\mysqld.exe", " --remove wampmysqld64"))
				WriteToLog("Removed Local MYSQL service...");
		}
		break;
	}
	default: {
		WriteToLog("Local MYSQL Service has not stopped or errored");
		WriteToLog("It returned with status code: " + wampMySQLSvcStatus);
		break;
	}
	}

	int wampApacheSvcStatus = GetSvcStatus(NULL, "wampapache64");
	string apache_dir = GetStrVal(hKey, "Apache_DIR", REG_SZ);
	cout << apache_dir << endl;
	//wampApacheSvcStatus = 4;
	switch (wampApacheSvcStatus) {
	case -1: {
		WriteToError("Apache Service errored with unknown error");
		/*if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k stop -n wampapache64"))
			WriteToLog("Apache Service stopped...");
		if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k uninstall -n wampapache64"))
			WriteToLog("Apache Service uninstalled...");*/
		if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k install -n wampapache64"))
			WriteToLog("Apache Service installed...");
		if (StartTargetSvc("wampapache64"))
			WriteToLog("Apache Services started...");
		else {
			if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k stop -n wampapache64"))
				WriteToLog("Apache Service stopped...");
			if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k uninstall -n wampapache64"))
				WriteToLog("Apache Service uninstalled...");
		}
		break;

	}
	case 1: {
		WriteToLog("Apache Service has stopped...");
		if (StartTargetSvc("wampapache64"))
			WriteToLog("Successfully Restarted Apache Service");
		else {
			WriteToLog("Failed to start Apache Service...");
			if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k stop -n wampapache64"))
				WriteToLog("Apache Service stopped...");
			if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k uninstall -n wampapache64"))
				WriteToLog("Apache Service uninstalled...");
		}
		break;
	}
	default: {
		WriteToLog("Apache Service has not stopped or errored");
		WriteToLog("It returned with status code: " + wampApacheSvcStatus);
		/*if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k stop -n wampapache64"))
			WriteToLog("Apache Service stopped...");
		if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k uninstall -n wampapache64"))
			WriteToLog("Apache Service uninstalled...");*/
		break;
	}
	}

	RegCloseKey(hKey);

	if (!(checkForInternetConnection() && isInternetConnected()))
	{
		WriteToLog("Failed to confirm network connection");
		return 0;
	}
	//string targetApp = TrakM->appType.c_str();
	/*if (!connectToLocalDB(TrakM->appType))
	{
		WriteToLog("Failed to connect to Local Database.");
		return 0;
	}

	if (!connectToRemoteDB(TrakM->appType))
	{
		WriteToLog("Failed to connect to Remote Database.");
		return 0;
	}*/

	TrakM->CreateDataModule();

	//connectToRemoteDB(TrakM->appType);
	WriteToLog("Checking if TRAK is Running");
	if (!ProcessExists(TrakM->appName)) {
		WriteToError("TRAK process is not running");
		string targetExe = TrakM->appDir + TrakM->appName;
		cout << targetExe << endl;
		WriteToLog("TRAK process not running, starting with path: " + targetExe);
		if (!ShellExecuteApp(targetExe, ""))
		{
			WriteToError("Failed to start " + targetExe);
			return 0;
		}
	}


	return a->exec();
}

int main(int argc, char* argv[])
{

	//printf(TEXT("CNC_Current_Tracker_Service: Main: StartServiceCtrlDispatcher"));
	//getchar();

	/*HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));
	char buff[MAX_PATH];
	int byteLength = GetCurrentDirectoryA(sizeof(buff), buff);
	string currDir = buff;
	currDir.resize(byteLength);
	string installDir = GetStrVal(hKey, "Install_DIR", REG_SZ);
	struct stat buffer;
	if (installDir == "" || stat(currDir.c_str(), &buffer) == 0)
	{
		installDir = currDir;
		cout << installDir << endl;
		SetStrVal(hKey, "Install_DIR", installDir, REG_SZ);
	}
	RegCloseKey(hKey);

	HenchmanService service;
	a = new QCoreApplication(argc, argv);
	service.MainFunction();*/

	//cout << "Export path: " << GetExportsPath() << endl;
	//service.app_path = "C:\\FPC\\Kaptap.exe";
	//cout << "Logs path: " << GetLogsPath() << endl;
	//vector<string> explodedString;
	/*try {
		string s = "Hello World Jack Son";
		explodedString = service.Explode(" ", s, 1);
	}
	catch (const HenchmanServiceException& hse) {
		cout << "An Exception in HenchmanService Occurred:" << endl;
		cout << hse.what();
	}*/
	//for(auto i : explodedString) cout << i << endl;

	//ProcessExists("HenchmanServices") ? cout << "Yes It Does" : cout << "No It Does Not";
	//cout << endl;
	//
	//char buff[1024];
	//int byteLength = GetCurrentDirectory(sizeof(buff), buff);
	//string currDir = buff;
	//currDir.resize(byteLength);
	//string installDir = GetStrVal(hKey, "InstallDir", REG_SZ);
	//struct stat buffer;
	//if (installDir == "" || stat(currDir.c_str(), &buffer) == 0)
	//{
	//	installDir = currDir;
	//}
	//SetStrVal(hKey, "InstallDir", installDir, REG_SZ);

	//FileInUse(installDir + "\\HenchmanServices.exe") ? cout << "Yes It Is" : cout << "No It Is Not";
	//cout << endl;
	//
	//cout << "creating Table for service" << endl;
	//string tableName = "TestTable";
	//vector<string> cols;
	//cols.push_back("username TEXT NOT NULL");
	//cols.push_back("password TEXT NOT NULL");
	//SQLiteM.CreateTable(tableName, cols);

	//cout << "Reading ini file: " << string(installDir + "\\service.ini") << endl;
	//SI_Error rc = ini.LoadFile(string(installDir + "\\service.ini").c_str());
	//if (rc < 0) {
	//	cerr << "Failed to Load INI File" << endl;
	//}
	//string username = ini.GetValue("Email", "Username", "");
	//string password = ini.GetValue("Email", "Password", "");
	//
	//if (checkForInternetConnection() && isInternetConnected() && service.setMailLogin(username, password)) {
	//	cout << "Able to send mail" << endl;
	//	//service.ConnectWithSMTP();
	//}
	//ShellExecuteApp("HenchmanServices.exe", "") ? cout << "Successfully excecuted" << endl : cout << "Failed to excecute" << endl;
	/*InstallMySQL();
	InstallApache();
	InstallOnlineOfflineScript();*/

	//explodedString.clear();

	//Sleep(5000);
	//getchar();

	//delete a;
	return RunAsService(argc, argv);
	//return 0;
}