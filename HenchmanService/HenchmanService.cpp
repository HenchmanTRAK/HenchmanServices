/// <summary>
/// Main Service for HenchmanTRAK Entry Point.
/// This script has the role of installing, controlling and managing the HenchmanService.
/// </summary>

#include "HenchmanService.h"

using namespace std;
using namespace ServiceController;

#ifdef DEBUG
	bool testing = true;
#else
	bool testing = false;
#endif // DEBUG

bool logEvent = false;
//QCoreApplication* a = nullptr;

//std::unique_ptr<CServiceController> svcController = nullptr;
std::unique_ptr<SService> service = nullptr;

// ShowCerts - Prints out the given certificate.
/*
static string ShowCerts(SSL* ssl)
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
*/

// InitCTX - initialize the SSL engine.
/*
static SSL_CTX* InitCTX(void)
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
*/

// sslError - Handles errors in the SSL connection.
/*
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
*/

static bool ProcessExists(const tstring& exeFileName)
{
	HANDLE SnapshotHandle;
	SnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 ProcessEntry32;
	ZeroMemory(&ProcessEntry32, sizeof(ProcessEntry32));
	ProcessEntry32.dwSize = sizeof(ProcessEntry32);
	bool ContinueLoop = Process32First(SnapshotHandle, &ProcessEntry32);
	bool result = false;
	while (ContinueLoop) {
		tstring targetEXE = exeFileName;
		transform(targetEXE.begin(), targetEXE.end(), targetEXE.begin(), ::toupper);
		tstring processEXE = ProcessEntry32.szExeFile;
		transform(processEXE.begin(), processEXE.end(), processEXE.begin(), ::toupper);
		tstring processEXEFileName = ServiceHelper().fileBasename(ProcessEntry32.szExeFile);
		transform(processEXEFileName.begin(), processEXEFileName.end(), processEXEFileName.begin(), ::toupper);
		//WriteToLog("Checking if target exe: " + targetEXE + " is process: " + processEXEFileName);
		if ((processEXEFileName == targetEXE) || (processEXE == targetEXE)) {
			result = true;
			ContinueLoop = false;
		}
		else {
			ContinueLoop = Process32Next(SnapshotHandle, &ProcessEntry32);
		}
	}
	CloseHandle(SnapshotHandle);
	return result;
}

static bool FileInUse(string fileName) {
	HANDLE fileRes;
	//struct stat buffer;
	LOG << "Checking if: " << fileName << " is being used";
	bool result = false;
	if (filesystem::exists(fileName)) {
		LOG << "Target File Exists";
		fileRes = CreateFileA(fileName.data(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		result = fileRes == INVALID_HANDLE_VALUE;
		CloseHandle(fileRes);
		return result;
	}
	LOG << "Target File Does Not Exists Or Could Not Be Found";
	return result;
}



DWORD GetCurrentSessionId()
{
	WTS_SESSION_INFO* pSessionInfo;
	DWORD n_sessions = 0;
	BOOL ok = WTSEnumerateSessions(WTS_CURRENT_SERVER, 0, 1, &pSessionInfo, &n_sessions);
	if (!ok) {
		ServiceHelper().WriteToError("Failed to enumerate through sessions");
		return 0;
	}

	DWORD SessionId = 0;

	for (DWORD i = 0; i < n_sessions; ++i)
	{
		if (pSessionInfo[i].State == WTSActive)
		{
			SessionId = pSessionInfo[i].SessionId;
			break;
		}
	}

	WTSFreeMemory(pSessionInfo);
	return SessionId;
}

static int CreateTargetProcess(string lpAppName)
{
	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInformation;

	ZeroMemory(&startupInfo, sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);
	ZeroMemory(&processInformation, sizeof(processInformation));

	try {
		if (!CreateProcess(
			lpAppName.data(),
			GetCommandLine(),
			NULL,
			NULL,
			false,
			0,
			NULL,
			NULL,
			&startupInfo,
			&processInformation
		))
		{
			// HenchmanServiceException
			throw HenchmanServiceException("Failed to create process for: " + lpAppName);
		}
		ServiceHelper().WriteToLog("Successfully created process for: " + lpAppName);
		CloseHandle(processInformation.hProcess);
		CloseHandle(processInformation.hThread);
	}
	catch (exception &e)
	{
		ServiceHelper().WriteToError(e.what());
		return 0;
	}
	return 1;
}

bool LaunchProcess(const TCHAR* process_path)
{

	ServiceHelper().WriteToLog("Attempting to launch process: " + std::string(process_path));
	DWORD SessionId = GetCurrentSessionId();
	if (SessionId == 0) {   // no-one logged in
		ServiceHelper().WriteToError("No session id could be found");
		return false;
	}
	ServiceHelper().WriteToLog(std::string("Retrieved session id: " + SessionId));

	HANDLE hToken;
	BOOL ok = WTSQueryUserToken(SessionId, &hToken);
	//BOOL ok = ImpersonateLoggedOnUser(&hToken);
	if (!ok) {
		ServiceHelper().WriteToError("Could not get user token");
		return false;
	}

	void* environment = NULL;
	ok = CreateEnvironmentBlock(&environment, hToken, TRUE);

	if (!ok)
	{
		ServiceHelper().WriteToError("Could not create environment block");
		CloseHandle(hToken);
		return false;
	}

	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	//si.lpDesktop = (char *)"winsta0\\default";

	// Do NOT want to inherit handles here
	DWORD dwCreationFlags = NORMAL_PRIORITY_CLASS | DETACHED_PROCESS | CREATE_UNICODE_ENVIRONMENT;

	string path = string(process_path).substr(0, string(process_path).find_last_of('\\')+1);

	ok = CreateProcessAsUser(
		hToken, 
		process_path, 
		NULL, 
		NULL, 
		NULL, 
		FALSE,
		dwCreationFlags, 
		environment, 
		path.c_str(),
		&si, 
		&pi
	);

	DestroyEnvironmentBlock(environment);
	CloseHandle(hToken);

	if (!ok) {
		ServiceHelper().WriteToError("Failed to create process as user");
		return false;
	}
	ServiceHelper().WriteToLog("Seccessfully created process as user");
	
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return true;
}

void WINAPI SvCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:
		getServiceController()->ReportSvcStatus(CtrlCode, NO_ERROR, 0);

		SetEvent(getServiceController()->mService.serviceStopEvent);
		getServiceController()->ReportSvcStatus(getServiceController()->mService.serviceStatus.dwCurrentState, NO_ERROR, 0);
		return;
	case SERVICE_CONTROL_INTERROGATE:
		break;
	default:
		getServiceController()->ReportSvcStatus(CtrlCode, NO_ERROR, 0);
		break;
	}

}

void WINAPI SvcMain()
{
	//a = new QCoreApplication(argc, argv);
	//a = new QCoreApplication(dwArgc, lpszArgv);

	if (!testing)
	{

		getServiceController()->mService.serviceStatusHandle = RegisterServiceCtrlHandler(
			(TCHAR*)service->serviceName,
			SvCtrlHandler
		);

		if (!getServiceController()->mService.serviceStatusHandle)
			return;

		ZeroMemory(&getServiceController()->mService.serviceStatus, sizeof(getServiceController()->mService.serviceStatus));
		getServiceController()->mService.serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		getServiceController()->mService.serviceStatus.dwServiceSpecificExitCode = 0;

		getServiceController()->ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);
	
		SvcInit();

		if (getServiceController()->mService.serviceStatusHandle)
			CloseHandle(getServiceController()->mService.serviceStatusHandle);
	}
	else {
		SvcInit();
	}
	
	
	//delete a;
}

void WINAPI SvcInit()
{
	if (!testing)
	{

		getServiceController()->mService.serviceStopEvent = CreateEvent(
			NULL,
			TRUE,
			FALSE,
			NULL
		);

		if (getServiceController()->mService.serviceStopEvent == NULL)
		{
			getServiceController()->ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
			return;
		}

		

		DWORD dwThreadId = 0;

		HANDLE hThread = CreateThread(NULL, 0, SvcWorkerThread, &getServiceController()->mService.serviceStopEvent, 0, &dwThreadId);

		EventManager::CEventManager().ReportCustomEvent("SvcInit", std::string("Created Service Thread with id: ").append(std::to_string(dwThreadId)).data(), 1, 0);

		if (hThread) {
			getServiceController()->ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

			WaitForSingleObject(
				getServiceController()->mService.serviceStopEvent,
				INFINITE
			);
		}

		getServiceController()->ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		CloseHandle(getServiceController()->mService.serviceStopEvent);

	}
	else {
		std::cout << "Testing service\n";
		SvcWorkerThread();
	}
	return;

}

static void setContextMenuCommands(const tstring& verb, const tstring& command)
{
	std::string verbName = verb;
	std::erase(verbName, ' ');

	RegistryManager::CRegistryManager rtManagerShell(HKEY_CLASSES_ROOT, std::string(service->serviceName).append("\\Shell\\").append(verbName).c_str());
	RegistryManager::CRegistryManager rtManagerCommand(HKEY_CLASSES_ROOT, std::string(service->serviceName).append("\\Shell\\").append(verbName).append("\\command").c_str());

	rtManagerShell.SetVal("MUIVerb", REG_SZ, (TCHAR*)verb.data(), verb.length());
	rtManagerCommand.SetVal("", REG_SZ, (TCHAR*)command.data(), command.length());
}

static void setContextMenu(const std::string& installDir)
{
	RegistryManager::CRegistryManager rtManager(HKEY_CLASSES_ROOT, std::string("*\\shell\\").append(service->serviceName).c_str());
	rtManager.SetVal("MUIVerb", REG_SZ, (TCHAR*)"HenchmanService", sizeof("HenchmanService"));
	std::string serviceName(service->serviceName);
	rtManager.SetVal("ExtendedSubCommandsKey", REG_SZ, (TCHAR*)serviceName.data(), serviceName.length());
	rtManager.SetVal("Extended", REG_SZ, (TCHAR*)"", sizeof(""));
	std::string appliedTo("System.ItemName:\"");
	appliedTo.append(service->serviceName).append(".exe\"");
	rtManager.SetVal("AppliesTo", REG_SZ, (TCHAR*)appliedTo.data(), appliedTo.length());

	setContextMenuCommands("Start Service", "\"" + installDir + "/" + service->serviceName + ".exe\" \"--start\" \"%1\"");
	setContextMenuCommands("Stop Service", "\"" + installDir + "/" + service->serviceName + ".exe\" \"--stop\" \"%1\"");
	setContextMenuCommands("Install Service", "\"" + installDir + "/" + service->serviceName + ".exe\" \"--install\" \"%1\"");
	setContextMenuCommands("Remove Service", "\"" + installDir + "/" + service->serviceName + ".exe\" \"--remove\" \"%1\"");

}

static void removeContextMenu()
{
	RegistryManager::CRegistryManager::RemoveTargetKey(HKEY_CLASSES_ROOT, std::string("*\\shell\\").append(service->serviceName).c_str());
	RegistryManager::CRegistryManager::RemoveTargetKey(HKEY_CLASSES_ROOT, std::string(service->serviceName).c_str());
}

int main(int argc, char* argv[])
{
#ifdef DEBUG
	std::cout << "Debug Enabled" << std::endl;
#endif // DEBUG

	CSimpleIni ini;
	//ini.IsUnicode();
	service = make_unique<SService>();
	service->serviceName = SERVICE_NAME;
	service->displayName = SERVICE_DISPLAY_NAME;
	service->localUser = NULL;
	service->localPass = NULL;
	bool contextMenu = false;

	SI_Error rc = ini.LoadFile(".\\service.ini");
	if (rc < 0) {
		std::cout << "Failed to Load INI File" << endl;
	}
	else {
		testing = ini.GetBoolValue("DEVELOPMENT", "testingMain", 0);
		//std::string localUser(ini.GetValue("SYSTEM", "LocalSystemAccountName", NULL));
		//std::string localUserPass(ini.GetValue("SYSTEM", "LocalSystemAccountPassword", NULL));
		service->localUser = ini.GetValue("SYSTEM", "LocalSystemAccountName", NULL);
		service->localPass = ini.GetValue("SYSTEM", "LocalSystemAccountPassword", NULL);
		contextMenu = ini.GetBoolValue("SYSTEM", "enableContextMenu", 0);
		logEvent = ini.GetBoolValue("SYSTEM", "enableEventLogging", 0);
	}

	if (testing && argc <= 1)
		argv[1] = (char *)"--install";
	
		

	//svcController = make_unique<CServiceController>(*service);
	createUniqueServiceController(*service, testing);
		
	if (lstrcmpiA(argv[1], "--install") == 0)
	{	
		try {
			RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, tstring("SOFTWARE\\HenchmanTRAK\\").append(service->serviceName).c_str());
			//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));
			DWORD size = 0;
			std::vector<TCHAR> buffer(sizeof(TCHAR));
			std::cout << "Getting INSTALL_DIR size from registry\n";
			LONG nError = rtManager.GetValSize("INSTALL_DIR", REG_SZ, &size, &buffer);
			
			if (nError || size <= 0) {
				std::string currDir = std::filesystem::current_path().string();
				std::cout << "Setting INSTALL_DIR from registry\n";
				if (rtManager.SetVal("INSTALL_DIR", REG_SZ, currDir.data(), currDir.size()))
					throw HenchmanServiceException("Failed to set INSTALL_DIR to registry");
				std::cout << "Getting INSTALL_DIR size from registry again\n";
				rtManager.GetValSize("INSTALL_DIR", REG_SZ, &size, &buffer);
			}
			std::cout << "Getting INSTALL_DIR value from registry again\n";
			rtManager.GetVal("INSTALL_DIR", REG_SZ, buffer.data(), &size);
			tstring installDir(buffer.data());
			
			LOG << installDir << "\n";
			//string installDir = RegistryManager::GetStrVal(hKey, "INSTALL_DIR", REG_SZ);
			//if (installDir.empty())
			//{
			//	installDir = currDir;
			//	std::cout << installDir << "\n";
			//	//RegistryManager::SetVal(hKey, "INSTALL_DIR", installDir, REG_SZ);
			//	if (rtManager.SetVal("INSTALL_DIR", REG_SZ, (TCHAR*)installDir.c_str(), (installDir.length() + 1)))
			//		throw HenchmanServiceException("Failed to set INSTALL_DIR to registry");
			//}
			//std::cout << installDir << "\n";
			//std::cout << (installDir + "\\" + SERVICE_NAME + ".exe").c_str() << "\n";

			//service->servicePath = (installDir + "\\" + SERVICE_NAME + ".exe");
			//std::cout << service->servicePath << "\n";

			getServiceController()->mService.servicePath = (installDir + "\\" + SERVICE_NAME + ".exe");

			if (logEvent)
			{

				RegistryManager::CRegistryManager rmEvent(HKEY_LOCAL_MACHINE, tstring("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\").append(service->serviceName).data());

				DWORD size = MAX_PATH;
				std::vector<TCHAR> buffer(size);
				rtManager.GetValSize("EventMessageFile", REG_SZ, &size, &buffer);

				rtManager.GetVal("EventMessageFile", REG_SZ, buffer.data(), &size);
				
				tstring eventMessageFile(buffer.data());

				if (!installDir.empty() && (eventMessageFile.empty() || eventMessageFile != installDir))
				{
					installDir.append("\\event_log.dll");
					rmEvent.SetVal("EventMessageFile", REG_SZ, (TCHAR*)installDir.data(), installDir.size());
					DWORD typesSupported = 7;
					rmEvent.SetVal("TypesSupported", REG_DWORD, (DWORD*)&typesSupported, sizeof(DWORD));
				}
			}

			if (contextMenu)
				setContextMenu(installDir);
		}
		catch (const std::exception& e) {
			ServiceHelper().WriteToError(e.what());
			std::cout << "Press enter to start service or hit CTRL+C to exit..." << std::endl;
			int c = getchar();
			if (c == '\n' || c == EOF)
				return 0;
		}

		getServiceController()->DoInstallSvc(ini.GetBoolValue("SYSTEM", "disableTaskCreation", 0));
		if (!testing) {
			Sleep(1000);
			ServiceHelper::ShellExecuteApp(argv[0], " --start");
			return 0;
		}
		LOG << "installing...";
		Sleep(1000);
		std::cout << "Press enter to start service or hit CTRL+C to exit..." << std::endl;
		int c = getchar();
		if (c == '\n' || c == EOF)
		argv[1] = (char*)"--start";
		
	}

	if (lstrcmpiA(argv[1], "--remove") == 0)
	{
		if (!testing) {
			getServiceController()->DoStopSvc();
			ServiceHelper().WriteToLog("Service has stopped");
			getServiceController()->DoDeleteSvc();
		}
		else {
			LOG << "stopping...";
			LOG << "removing...";
			int c = getchar();
		}

		if(contextMenu)
			removeContextMenu();
		return 0;
	}

	if (lstrcmpiA(argv[1], "--start") == 0)
	{
		if (!testing){
			getServiceController()->DoStartSvc();
			return 0;
		}
		LOG << "starting...";
	}

	if (lstrcmpiA(argv[1], "--stop") == 0)
	{
		if (!testing) {
			getServiceController()->DoStopSvc();
			ServiceHelper().WriteToLog("Service has stopped");
		}
		else {
			LOG << "stopping...";
			int c = getchar();
		}
		return 0;
	}

	if (testing) {
		SvcMain();
		return 0;
	}

	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{(TCHAR *)service->serviceName, (LPSERVICE_MAIN_FUNCTION)SvcMain},
		{NULL, NULL}
	};

	if (!StartServiceCtrlDispatcher(ServiceTable))
	{
		std::cout << "Failed to find Registered Service Controller." << std::endl;
		std::cout << "Press enter to install service or hit CTRL+C to exit..." << std::endl;
		int c = getchar();
		if (c == '\n' || c == EOF)
			ServiceHelper::ShellExecuteApp(argv[0], " --install");
		//std::cout << "Press any key to exit..." << endl;
	}


	//delete svcController;
	//svcController = nullptr;
	return 0;
}