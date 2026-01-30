/// <summary>
/// Main Service for HenchmanTRAK Entry Point.
/// This script has the role of installing, controlling and managing the HenchmanService.
/// </summary>

#include "HenchmanServiceLibrary.h"

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
//std::unique_ptr<SService> service = nullptr;


void createUniqueServiceController(const SService& pService, bool isTesting) {
	svcController = make_unique<CServiceController>(pService, isTesting || testing);
}

CServiceController* getServiceController() {
	return svcController.get();
}

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

static bool FileInUse(tstring fileName) {
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

static int CreateTargetProcess(tstring lpAppName)
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

DWORD WINAPI SvcWorkerThread(LPVOID lpParam)
{
	int argc = 0;
	char* argv[1];

	//EventManager::CEventManager::Init(service->serviceName);

	EventManager::CEventManager evntManager(svcController->mService.serviceName);

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

	evntManager.ReportCustomEvent(svcController->mService.serviceName, "Service is running", 0);

	
	QCoreApplication* a = new QCoreApplication(argc, argv);
	HenchmanService hsService(a);
	
	while (testing || WaitForSingleObject(svcController->mService.serviceStopEvent, 0) != WAIT_OBJECT_0)
	{
		
		hsService.MainFunction(a);
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
		
	
	}
	
	evntManager.ReportCustomEvent(svcController->mService.serviceName, "Service has exited", 0);

	a->deleteLater();
	return ERROR_SUCCESS;
}

HenchmanService::HenchmanService(QObject *parent)
	: QObject(parent), sqliteManager(parent), dbManager(parent)
{
	enum ini_keys_enum {
		Apache_DIR,
		MySQL_DIR,
		PHP_DIR,
		TRAK_DIR,
		INI_FILE,
		EXE_FILE,
		APP_NAME
	};
	std::map<tstring, ini_keys_enum> int_keys_map = {
		{"Apache_DIR", Apache_DIR},
		{"MySQL_DIR", MySQL_DIR},
		{"PHP_DIR", PHP_DIR},
		{"TRAK_DIR", TRAK_DIR},
		{"INI_FILE", INI_FILE},
		{"EXE_FILE", EXE_FILE},
		{"APP_NAME", APP_NAME},
	};
	
	CSimpleIni ini;

	// setup message logging
	qInstallMessageHandler(ServiceHelper().messageOutput);

	//ini.SetUnicode();

	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));*/
	LOG << std::string("SOFTWARE\\HenchmanTRAK\\").append(svcController->mService.serviceName).data();
	
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, tstring("SOFTWARE\\HenchmanTRAK\\").append(svcController->mService.serviceName).data());
	TCHAR buffer[1024] = "\0";
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
		tstring key = val.pItem;
		tstring value = ini.GetValue("WAMP", val.pItem, "");
		ServiceHelper().removeQuotes(value);

		if (value.empty()) {
			switch (int_keys_map.at(key))
			{
			case Apache_DIR:
			{
				std::filesystem::directory_iterator dir("C:\\wamp\\bin\\apache");
				for (const auto& entry : dir) {
					if (!QString(entry.path().filename().c_str()).contains("apache"))
						continue;
					value = t2tstr(entry.path().c_str());
//#ifdef UNICODE
//					value = entry.path().wstring();
//#else
//					value = entry.path().c_str();
//#endif
					
				}
				value += "\\bin";
				//value =;
				break;
			}
			case MySQL_DIR:
			{
				std::filesystem::directory_iterator dir("C:\\wamp\\bin\\mysql");
				for (const auto& entry : dir) {
					if (!QString(entry.path().filename().c_str()).contains("mysql"))
						continue;
					value = t2tstr(entry.path().c_str());
//#ifdef UNICODE
//					value = entry.path().wstring();
//#else
//					value = entry.path().string();
//#endif

				}
				value += "\\bin";
				break;
			}
			case PHP_DIR:
			{
				std::filesystem::directory_iterator dir("C:\\wamp\\bin\\php");
				for (const auto& entry : dir) {
					if (!QString(entry.path().filename().c_str()).contains("php"))
						continue;
					value = t2tstr(entry.path().c_str());
//#ifdef UNICODE
//					value = entry.path().wstring();
//#else
//					value = entry.path().string();
//#endif
				}
				break;
			}
			default:
			{
				break;
			}

			}

			ini.SetValue("WAMP", key.data(), value.data());
		}


		if (rtManager.SetVal(key.data(), REG_SZ, (TCHAR*)value.data(), (value.length() + 1)))
			throw HenchmanServiceException("Failed to set "+ key +" to registry");

		key.clear();
		value.clear();
	};

	ini.GetAllKeys("TRAK", keys);
	std::string appType;
	for (auto& val : keys)
	{
		tstring key = val.pItem;
		tstring value = ini.GetValue("TRAK", val.pItem, "");
		ServiceHelper().removeQuotes(value);

		if (value.empty()) {
			if (appType.empty()) {
				std::filesystem::directory_iterator dir("C:\\Program Files (x86)\\HenchmanTRAK");
				for (const auto& entry : dir) {
					std::string file(t2tstr(entry.path().filename().string()));
					if (file.ends_with("kabTRAK")) {
						appType = "kabTRAK";
						break;
					}
					if (file.ends_with("cribTRAK")) {
						appType = "cribTRAK";
						break;
					}
					if (file.ends_with("portaTRAK")) {
						appType = "portaTRAK";
						break;
					}

				}
			}


			switch (int_keys_map.at(key))
			{
			case TRAK_DIR:
			{
				value = t2tstr("C:\\Program Files (x86)\\HenchmanTRAK\\" + appType);
				break;
			}
			case INI_FILE:
			{
				const char* iniFile = "";
				if (appType == "kabTRAK")
					iniFile = "trak.ini";
				if (appType == "cribTRAK")
					iniFile = "crib.ini";
				if (appType == "portaTRAK")
					iniFile = "porta.ini";
				value = t2tstr(iniFile);
				break;
			}
			case EXE_FILE:
			{
				std::string exe(appType);
				for (int i = 0; i < exe.length(); ++i) {
					exe[i] = tolower(exe.at(i));
				}
				value = t2tstr(exe + ".exe");
				break;
			}
			case APP_NAME: 
			{
				value = t2tstr(appType);
				break;
			}
			default:
			{
				break;
			}

			}

			ini.SetValue("TRAK", key.data(), value.data());
		}

		if (rtManager.SetVal(key.data(), REG_SZ, (TCHAR*)value.data(), (value.length() + 1)))
			throw HenchmanServiceException("Failed to set INSTALL_DIR to registry");

		key.clear();
		value.clear();

	}

	ini.SaveFile((installDir + "\\service.ini").data());


	//RegCloseKey(hKey);
	//sqliteManager = make_unique<SQLiteManager2>(a);


	tstring tableName = "TestTable";
	vector<tstring> columns;
	columns.push_back("username TEXT NOT NULL");
	columns.push_back("password TEXT NOT NULL");
	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery("CREATE UNIQUE INDEX IF NOT EXISTS unique_username_password ON " + tableName + "(username,password)", result);

	columns.clear();

	tstring username = ini.GetValue("EMAIL", "Username", "");
	tstring password = ini.GetValue("EMAIL", "Password", "");
	if (password != "")
		password = QByteArray(password.data()).toBase64();
	//string encodedPass = base64(password);
	if (setMailLogin(username, password))
	{
		stringmap data;
		data["username"] = username;
		data["password"] = password;
		
		sqliteManager.AddEntry(
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
	ServiceHelper().WriteToLog("HenchmanService has been closed");

	sqliteManager.deleteLater();
	dbManager.deleteLater();
	this->parent()->deleteLater();

	//delete SQLiteM;
	//delete TrakM;
	//delete dbManager;
	//dbManager->deleteLater();

	//logx.clear();
}

bool HenchmanService::setMailLogin(const tstring& username, const tstring& password)
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

int HenchmanService::MainFunction(QCoreApplication* a)
{
	update = TRUE;
	int timer = 30000;

	checkStateOfMySQL();
	checkStateOfApache();

	try
	{

		if (!dbManager.networkManager.isInternetConnected())
		{
			//ServiceHelper().WriteToLog("Failed to confirm network connection");
			throw HenchmanServiceException("Failed to confirm network connection");
			//QTimer::singleShot(100, a, &QCoreApplication::quit);
		}
		//ConnectWithSMTP();

		//dbManager = new DatabaseManager(a);

		TRAKManager TrakM(&dbManager);

		TrakM.CreateDataModule();
		dbManager.loadTrakDetailsFromRegistry();

		ServiceHelper().WriteToLog("Checking if TRAK is Running");
		if (!ProcessExists(TrakM.appName)) {
			ServiceHelper().WriteToError("TRAK process is not running");
			string targetExe = TrakM.appDir + TrakM.appName;
#if false
			ServiceHelper().WriteToLog("TRAK process not running, starting with path: " + targetExe);
			//if (!CreateTargetProcess(targetExe))
			if (!LaunchProcess(targetExe.data()))
			{
				ServiceHelper().WriteToError("Failed to start " + targetExe);
			}
#endif
		}

		dbManager.targetApp = TrakM.appType;

		if (!dbManager.connectToLocalDB()) {
			throw HenchmanServiceException("Failed to establish connection to local database");
			//ServiceHelper().WriteToError("Failed to establish connection to local database");
			//QTimer::singleShot(0, this->parent(), &QCoreApplication::quit);
			//return 0;
		}

		if (!TrakM.UploadCurrentStateToRemote())
		{
			dbManager.connectToRemoteDB();
		}
	} 
	catch (exception& e) 
	{
		ServiceHelper().WriteToError(e.what());
	}

	if (!testing)
		timer = 30000;
	else
		timer = 5000;
		
	
	ServiceHelper().WriteToLog("Service sleeping for " + to_string(timer) + " ms...");
	QTimer::singleShot(timer, this->parent(), &QCoreApplication::quit);
	ServiceHelper().WriteToLog("Waiting for QT to finish execution...");
	a->exec();
	//Sleep(timer - 1);

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
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, tstring("SOFTWARE\\HenchmanTRAK\\").append(svcController->mService.serviceName).c_str());
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

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, tstring("SOFTWARE\\HenchmanTRAK\\").append(svcController->mService.serviceName).c_str());
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