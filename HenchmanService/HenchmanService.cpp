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

std::unique_ptr<CServiceController> svcController = nullptr;
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
	DWORD SessionId = GetCurrentSessionId();
	if (SessionId == 0) {   // no-one logged in
		ServiceHelper().WriteToError("No session id could be found");
		return false;
	}

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
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return true;
}

void WINAPI SvcCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:
		svcController->ReportSvcStatus(CtrlCode, NO_ERROR, 0);

		SetEvent(svcController->mServiceStopEvent);
		svcController->ReportSvcStatus(svcController->mServiceStatus.dwCurrentState, NO_ERROR, 0);
		return;
	case SERVICE_CONTROL_INTERROGATE:
		break;
	default:
		break;
	}
}

void WINAPI SvcMain()
{
	//a = new QCoreApplication(argc, argv);
	//a = new QCoreApplication(dwArgc, lpszArgv);

	if (!testing)
	{
		svcController->mServiceStatusHandle = RegisterServiceCtrlHandler(
			(TCHAR*)service->serviceName,
			SvcCtrlHandler
		);

		if (!svcController->mServiceStatusHandle)
			return;

		ZeroMemory(&svcController->mServiceStatus, sizeof(svcController->mServiceStatus));
		svcController->mServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		svcController->mServiceStatus.dwServiceSpecificExitCode = 0;

		svcController->ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);
	}
	
	SvcInit();

	if (svcController->mServiceStatusHandle)
		CloseHandle(svcController->mServiceStatusHandle);

	//delete a;
}

void SvcInit()
{
	if (!testing)
	{
		svcController->mServiceStopEvent = CreateEvent(
			NULL,
			TRUE,
			FALSE,
			NULL
		);

		if (svcController->mServiceStopEvent == NULL)
		{
			svcController->ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
			return;
		}

		svcController->ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

		HANDLE hThread = CreateThread(NULL, 0, SvcWorkerThread, &svcController->mServiceStopEvent, 0, NULL);

		if (hThread)
			WaitForSingleObject(
				svcController->mServiceStopEvent,
				INFINITE
			);

		svcController->ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		CloseHandle(svcController->mServiceStopEvent);
	}
	else {
		std::cout << "Testing service\n";
		SvcWorkerThread(NULL);
	}
	return;

}

DWORD WINAPI SvcWorkerThread(LPVOID lpParam)
{
	int argc = 0;
	char* argv[1];

	//EventManager::CEventManager::Init(service->serviceName);

	EventManager::CEventManager evntManager(service->serviceName);

	// add registering registering application in event log and removing on exit.
	//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\").append(SERVICE_NAME));

	/*RegistryManager::CRegistryManager rmEvent(HKEY_LOCAL_MACHINE, std::wstring(L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\").append(SERVICE_NAME).data());
	RegistryManager::CRegistryManager rmSource(HKEY_LOCAL_MACHINE, std::wstring(L"SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME).data());

	TCHAR eventBuff[MAX_PATH] = L"\0";
	DWORD eventBuffSize = MAX_PATH;
	rmEvent.GetVal(L"EventMessageFile", REG_SZ, (TCHAR *)eventBuff, eventBuffSize);
	string eventMessageFile(eventBuff);

	TCHAR sourceBuff[MAX_PATH] = L"\0";
	DWORD sourceBuffSize = MAX_PATH;
	rmSource.GetVal(L"INSTALL_DIR", REG_SZ, (TCHAR *)sourceBuff, sourceBuffSize);
	string installDir(sourceBuff);

	if (!installDir.empty() && (eventMessageFile.empty()  || eventMessageFile != installDir))
	{
		installDir.append((TCHAR *)"\\event_messages.dll");
		rmEvent.SetVal(L"EventMessageFile", REG_SZ, (TCHAR *)installDir.data(), installDir.length());
		DWORD typesSupported = 7;
		rmEvent.SetVal(L"TypesSupported", REG_DWORD, (DWORD*)&typesSupported, sizeof(DWORD));
	}
	eventMessageFile.clear();
	installDir.clear();*/

	//EventManager(SERVICE_NAME).ReportCustomEvent(SERVICE_NAME, "Service started", 0);

	evntManager.ReportCustomEvent(service->serviceName, "Service is running", 0);

	
	QCoreApplication* a = new QCoreApplication(argc, argv);
	HenchmanService hsService(a);
	
	while (testing || WaitForSingleObject(svcController->mServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		
		hsService.MainFunction();
#if false
		service.sqliteManager->UpdateEntry(
			"Test",
			{"id = 1"},
			{ 
				{"string", "string + " + to_string(counter++)} 
			}
		);

		service.sqliteManager->RemoveEntry(
			"Test",
			{"updatedAt <= datetime('now', 'localtime')"}
		);

		service.sqliteManager->GetEntry(
			"TestTable",
			{"*", "COUNT(*) count"},
			{"updatedAt <= datetime('now', 'localtime')"}
			);
#endif
		ServiceHelper().WriteToLog("Waiting for QT to finish execution...");
		
		a->exec();
		ServiceHelper().WriteToLog("Service sleeping for 30000 ms...");
		if (!testing)
			Sleep(30000);
		else
			Sleep(5);
	
	}
	
	evntManager.ReportCustomEvent(service->serviceName, "Service has exited", 0);

	a->deleteLater();
	return ERROR_SUCCESS;
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

HenchmanService::HenchmanService(QObject *parent)
	: QObject(parent), sqliteManager(make_unique<SQLiteManager2>(parent)), dbManager(make_unique<DatabaseManager>(parent))
{
	CSimpleIni ini;

	//ini.SetUnicode();

	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));*/
	LOG << std::string("SOFTWARE\\HenchmanTRAK\\").append(service->serviceName).data();
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\").append(service->serviceName).data());
	TCHAR buffer[1024];
	DWORD size = sizeof(buffer);
	rtManager.GetVal("INSTALL_DIR", REG_SZ, (TCHAR*)buffer, size);
	tstring installDir(buffer);
	// HenchmanServiceException
	LOG << "Install dir: " << installDir;
	SI_Error rc = ini.LoadFile((installDir + "\\service.ini").data());
	if (rc < 0) {
		cerr << "Failed to Load INI File" << endl;
	}

	CSimpleIniA::TNamesDepend keys;
	ini.GetAllKeys("WAMP", keys);
	for (auto& val : keys)
	{
		string key = val.pItem;
		string value = ini.GetValue("WAMP", val.pItem, "");
		ServiceHelper().removeQuotes(value);

		/*RegistryManager::GetStrVal(hKey, key.data(), REG_SZ);*/
		//RegistryManager::SetVal(hKey, key.data(), value, REG_SZ);
		if (rtManager.SetVal(key.data(), REG_SZ, (TCHAR *)value.data(), (value.length() + 1)))
			throw HenchmanServiceException("Failed to set INSTALL_DIR to registry");

		key.clear();
		value.clear();
	}

	ini.GetAllKeys("TRAK", keys);
	for (auto& val : keys)
	{
		string key = val.pItem;
		string value = ini.GetValue("TRAK", val.pItem, "");
		ServiceHelper().removeQuotes(value);

		if (rtManager.SetVal(key.data(), REG_SZ, (TCHAR*)value.data(), (value.length() + 1)))
			throw HenchmanServiceException("Failed to set INSTALL_DIR to registry");

		key.clear();
		value.clear();

	}

	//RegCloseKey(hKey);
	//sqliteManager = make_unique<SQLiteManager2>(a);


	string tableName = "TestTable";
	vector<string> columns;
	columns.push_back("username TEXT NOT NULL");
	columns.push_back("password TEXT NOT NULL");
	sqliteManager->CreateTable(
		tableName,
		columns
	);

	columns.clear();

	string username = ini.GetValue("EMAIL", "Username", "");
	string password = ini.GetValue("EMAIL", "Password", "");
	if (password != "")
		password = QByteArray(password.data()).toBase64();
	//string encodedPass = base64(password);
	if (setMailLogin(username, password))
	{
		map<string, string> data;
		data["username"] = username;
		data["password"] = password;
		
		sqliteManager->AddEntry(
			tableName,
			data
		);

		data.clear();
	}
	//dbManager = make_unique<DatabaseManager>(a);

	/*currDir.clear();
	installDir.clear();
	keys.clear();
	databaseName.clear();
	dbName.clear();
	username.clear();
	password.clear();
	tableName.clear();
	columns.clear();*/

}

HenchmanService::~HenchmanService()
{
	LOG << "Deconstructing HenchmanService";
	//delete SQLiteM;
	//delete TrakM;
	//delete dbManager;
	//dbManager->deleteLater();

	//logx.clear();
}

bool HenchmanService::setMailLogin(const string& username, const string& password) 
{
	try {
		if (username.length() <= 1 || password.length() <= 1) {
			throw HenchmanServiceException("No SMTP email or password provided.");
		}
		mail_username = username;
		mail_password = password;
	}
	catch (exception& e)
	{
		ServiceHelper().WriteToError(e.what());
		return false;
	}
	return true;
}

int HenchmanService::SetRequiredParameters()
{

#if false
	string tableName = "Test";
	sqliteManager->CreateTable(
		tableName,
		{"string TEXT"}
	);

	sqliteManager->AddEntry(
		tableName,
		{
			{"string", "Hello World!"}
		}
	);
#endif
	return 0;
}

int HenchmanService::MainFunction()
{
	update = TRUE;

	checkStateOfMySQL();
	checkStateOfApache();
	
	if (!dbManager->isInternetConnected())
	{
		ServiceHelper().WriteToLog("Failed to confirm network connection");
		//QTimer::singleShot(100, a, &QCoreApplication::quit);
		QTimer::singleShot(0, this->parent(), &QCoreApplication::quit);
		return 0;
	}

	//ConnectWithSMTP();

	//dbManager = new DatabaseManager(a);

	TRAKManager TrakM(dbManager.get());

	TrakM.CreateDataModule();

	ServiceHelper().WriteToLog("Checking if TRAK is Running");
	if (!ProcessExists(TrakM.appName)) {
		ServiceHelper().WriteToError("TRAK process is not running");
		string targetExe = TrakM.appDir + TrakM.appName;
		ServiceHelper().WriteToLog("TRAK process not running, starting with path: " + targetExe);
		//if (!CreateTargetProcess(targetExe))
		if (!LaunchProcess(targetExe.data()))
		{
			ServiceHelper().WriteToError("Failed to start " + targetExe);
		}
	}
	dbManager->targetApp = TrakM.appType;

	if (!dbManager->connectToLocalDB()) {
		ServiceHelper().WriteToError("Failed to establish connection to local database");
		QTimer::singleShot(0, this->parent(), &QCoreApplication::quit);
		return 0;
	}

	if (!TrakM.UploadCurrentStateToRemote())
	{
		dbManager->connectToRemoteDB();
	}
	else {
		//QTimer::singleShot(1000, a, &QCoreApplication::quit);
		QTimer::singleShot(0, this->parent(), &QCoreApplication::quit);
	}

	/*if (!(dbManager->AddKabsIfNotExists() ||
		dbManager->AddDrawersIfNotExists() ||
		dbManager->AddToolsIfNotExists() ||
		dbManager->AddToolsInDrawersIfNotExists())
		) {
		dbManager->connectToRemoteDB();
	}*/
	
	LOG << "Exiting Main Function";
	
	return 0;
}

void HenchmanService::checkStateOfMySQL()
{
	//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\").append(service->serviceName).c_str());
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal("MySQL_DIR", REG_SZ, (TCHAR *)buffer, size);
	//string mysql_dir = RegistryManager::GetStrVal(hKey, "MySQL_DIR", REG_SZ);
	string mysql_dir(buffer);
	//RegCloseKey(hKey);
	ServiceHelper().WriteToLog("Checking for Local MYSQL service...");
	int wampMySQLSvcStatus = svcController->GetSvcStatus("wampmysqld64");
	// HenchmanServiceException
	try {
		switch (wampMySQLSvcStatus) {
		case -1: {
			ServiceHelper().WriteToError("MySQL Service errored with unknown error");
			if (!ServiceHelper::ShellExecuteApp(mysql_dir + "\\mysqld.exe", " --install-manual wampmysqld64"))
				throw HenchmanServiceException("Failed to install Local MYSQL service");
			ServiceHelper().WriteToLog("Local MYSQL service installed...");
			if (!svcController->StartTargetSvc("wampmysqld64"))
				throw HenchmanServiceException("Failed to start Local MYSQL service");
			ServiceHelper().WriteToLog("Local MYSQL Service Started");
			break;

		}
		case 1: {
			ServiceHelper().WriteToLog("Local MYSQL Service has stopped...");
			if (svcController->StartTargetSvc("wampmysqld64"))
				ServiceHelper().WriteToLog("Successfully Restarted Local MYSQL Service");
			else {
				throw HenchmanServiceException("Failed to start Local MYSQL service");
			}
			break;
		}
		default: {
			ServiceHelper().WriteToLog("Local MYSQL Service has not stopped or errored \nIt returned with status code: " + to_string(wampMySQLSvcStatus));
			break;
		}
		}
	}
	catch (exception& e)
	{
		ServiceHelper().WriteToError(e.what());
		if (ServiceHelper::ShellExecuteApp(mysql_dir + "\\mysqld.exe", " --remove wampmysqld64"))
			ServiceHelper().WriteToLog("Removed Local MYSQL service...");
	}

}

void HenchmanService::checkStateOfApache()
{
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));
	string apache_dir = RegistryManager::GetStrVal(hKey, "Apache_DIR", REG_SZ);
	RegCloseKey(hKey);*/

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\").append(service->serviceName).c_str());
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal("Apache_DIR", REG_SZ, (TCHAR*)buffer, size);
	//string mysql_dir = RegistryManager::GetStrVal(hKey, "MySQL_DIR", REG_SZ);
	string apache_dir(buffer);

	ServiceHelper().WriteToLog("Checking for Local Apache service...");
	int wampApacheSvcStatus = svcController->GetSvcStatus("wampapache64");
	// HenchmanServiceException
	try {
		switch (wampApacheSvcStatus) {
		case -1: {
			ServiceHelper().WriteToError("Apache Service errored with unknown error");
			if (!ServiceHelper::ShellExecuteApp(apache_dir + "\\httpd.exe", " -k install -n wampapache64"))
				throw HenchmanServiceException("Failed to install Apache service");
			ServiceHelper().WriteToLog("Apache Service installed...");
			if (ServiceHelper::ShellExecuteApp(apache_dir + "\\httpd.exe", " -k start -n wampapache64"))
				ServiceHelper().WriteToLog("Apache Services started...");
			else {
				throw HenchmanServiceException("Failed to start Apache service");
			}
			break;

		}
		case 1: {
			ServiceHelper().WriteToLog("Apache Service has stopped...");
			if (ServiceHelper::ShellExecuteApp(apache_dir + "\\httpd.exe", " -k start -n wampapache64"))
				ServiceHelper().WriteToLog("Successfully Restarted Apache Service");
			else {
				throw HenchmanServiceException("Failed to start Apache Service");
			}
			break;
		}
		default: {
			ServiceHelper().WriteToLog("Apache Service has not stopped or errored \nIt returned with status code: " + to_string(wampApacheSvcStatus));
			break;
		}
		}
	}
	catch (exception& e) {
		ServiceHelper().WriteToError(e.what());
		if (ServiceHelper::ShellExecuteApp(apache_dir + "\\httpd.exe", " -k stop -n wampapache64"))
			ServiceHelper().WriteToLog("Apache Service stopped...");
		if (ServiceHelper::ShellExecuteApp(apache_dir + "\\httpd.exe", " -k stop -n wampapache"))
			ServiceHelper().WriteToLog("Apache Service stopped...");
		if (ServiceHelper::ShellExecuteApp(apache_dir + "\\httpd.exe", " -k uninstall -n wampapache64"))
			ServiceHelper().WriteToLog("Apache Service uninstalled...");
		if (ServiceHelper::ShellExecuteApp(apache_dir + "\\httpd.exe", " -k uninstall -n wampapache"))
			ServiceHelper().WriteToLog("Apache Service uninstalled...");
	}
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
	
	svcController = make_unique<CServiceController>(*service);

	if (lstrcmpiA(argv[1], "--install") == 0)
	{
		try {
			RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, tstring("SOFTWARE\\HenchmanTRAK\\").append(service->serviceName).c_str());
			//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));
			TCHAR buffer[MAX_PATH] = "\0";
			DWORD size = MAX_PATH;
			rtManager.GetVal("INSTALL_DIR", REG_SZ, (TCHAR *)buffer, size);
			tstring installDir(buffer);

			//string installDir = RegistryManager::GetStrVal(hKey, "INSTALL_DIR", REG_SZ);
			if (installDir.empty() || installDir != std::filesystem::current_path().string())
			{
				installDir = filesystem::current_path().string();
				//RegistryManager::SetVal(hKey, "INSTALL_DIR", installDir, REG_SZ);
				if (rtManager.SetVal("INSTALL_DIR", REG_SZ, (TCHAR *)installDir.c_str(), (installDir.length() + 1)))
					throw HenchmanServiceException("Failed to set INSTALL_DIR to registry");
			}
			if (logEvent)
			{

				RegistryManager::CRegistryManager rmEvent(HKEY_LOCAL_MACHINE, tstring("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\").append(service->serviceName).data());

				TCHAR eventBuff[1024];
				DWORD eventBuffSize = 1024;
				rmEvent.GetVal("EventMessageFile", REG_SZ, (TCHAR*)eventBuff, eventBuffSize);
				tstring eventMessageFile(eventBuff);

				if (!installDir.empty() && (eventMessageFile.empty() || eventMessageFile != installDir))
				{
					installDir.append("\\event_log.dll");
					rmEvent.SetVal("EventMessageFile", REG_SZ, (TCHAR*)installDir.data(), installDir.length() + 1);
					DWORD typesSupported = 7;
					rmEvent.SetVal("TypesSupported", REG_DWORD, (DWORD*)&typesSupported, sizeof(DWORD));
				}
			}

			if(contextMenu)
				setContextMenu(installDir);
		}
		catch (const std::exception& e)
		{
			ServiceHelper().WriteToError(e.what());
			std::cout << "Press enter to start service or hit CTRL+C to exit..." << std::endl;
			int c = getchar();
			if (c == '\n' || c == EOF)
			return 0;
		}

		if (!testing) {
			svcController->DoInstallSvc();
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
			svcController->DoStopSvc();
			ServiceHelper().WriteToLog("Service has stopped");
			svcController->DoDeleteSvc();
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
			svcController->DoStartSvc();
			/*int c = getchar();
			if (c == '\n' || c == EOF)
				std::cout << "Press enter to start service or hit CTRL+C to exit..." << std::endl;*/
			return 0;
		}
		LOG << "starting...";
	}

	if (lstrcmpiA(argv[1], "--stop") == 0)
	{
		if (!testing) {
			svcController->DoStopSvc();
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