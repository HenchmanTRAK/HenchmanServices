/// <summary>
/// Main Service for HenchmanTRAK Entry Point.
/// This script has the role of installing, controlling and managing the HenchmanService.
/// </summary>

#include "HenchmanService.h"

using namespace std;

bool testing = false;

QCoreApplication* a = nullptr;
unique_ptr<ServiceController> svcController = nullptr;

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

static bool ProcessExists(string& exeFileName)
{
	HANDLE SnapshotHandle;
	SnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32W ProcessEntry32;
	ZeroMemory(&ProcessEntry32, sizeof(ProcessEntry32));
	ProcessEntry32.dwSize = sizeof(ProcessEntry32);
	bool ContinueLoop = Process32FirstW(SnapshotHandle, &ProcessEntry32);
	bool result = false;
	while (ContinueLoop) {
		string targetEXE = exeFileName;
		transform(targetEXE.begin(), targetEXE.end(), targetEXE.begin(), ::toupper);
		string processEXE = QString::fromWCharArray(ProcessEntry32.szExeFile).toStdString();
		transform(processEXE.begin(), processEXE.end(), processEXE.begin(), ::toupper);
		string processEXEFileName = ServiceHelper().fileBasename(QString::fromWCharArray(ProcessEntry32.szExeFile));
		transform(processEXEFileName.begin(), processEXEFileName.end(), processEXEFileName.begin(), ::toupper);
		//WriteToLog("Checking if target exe: " + targetEXE + " is process: " + processEXEFileName);
		if ((processEXEFileName == targetEXE) || (processEXE == targetEXE)) {
			result = true;
			ContinueLoop = false;
		}
		else {
			ContinueLoop = Process32NextW(SnapshotHandle, &ProcessEntry32);
		}
	}
	CloseHandle(SnapshotHandle);
	return result;
}

static bool FileInUse(string fileName) {
	HANDLE fileRes;
	//struct stat buffer;
	std::cout << "Checking if: " << fileName << " is being used" << endl;
	bool result = false;
	if (filesystem::exists(fileName)) {
		std::cout << "Target File Exists" << endl;
		fileRes = CreateFileA(fileName.data(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		result = fileRes == INVALID_HANDLE_VALUE;
		CloseHandle(fileRes);
		return result;
	}
	std::cout << "Target File Does Not Exists Or Could Not Be Found" << endl;
	return result;
}

static int ShellExecuteApp(string appName, string params)
{
	SHELLEXECUTEINFOA SEInfo;
	DWORD ExitCode;
	string exeFile = appName;
	string paramStr = params;
	string StartInString;

	if (!filesystem::exists(appName)) {
		ServiceHelper().WriteToError("Could not find Target EXE: " + appName);
		return 0;
	}


	// fine the windows handle using https://learn.microsoft.com/en-us/troubleshoot/windows-server/performance/obtain-console-window-handle

	ZeroMemory(&SEInfo, sizeof(SEInfo));
	SEInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	SEInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	//SEInfo.hwnd = NULL;
	//SEInfo.lpVerb = NULL;
	//SEInfo.lpDirectory = NULL;
	SEInfo.lpFile = exeFile.data();
	SEInfo.lpParameters = paramStr.data();
	//: SW_HIDE
	SEInfo.nShow = paramStr == "" && SW_NORMAL;
	ServiceHelper().WriteToLog("Executing target: " + appName + params);
	if (ShellExecuteExA(&SEInfo)) {
		stringstream exitCodeMessage;
		//do {
			GetExitCodeProcess(SEInfo.hProcess, &ExitCode);
			exitCodeMessage << "Target: " << exeFile << " return exit code : " << to_string(ExitCode);
			ServiceHelper().WriteToLog(exitCodeMessage.str());
			exitCodeMessage.clear();
		//} while (ExitCode != STILL_ACTIVE);
		return 1;
	}

	return 0;
}

void WINAPI SvcCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:
		svcController->ReportSvcStatus(CtrlCode, NO_ERROR, 0);

		SetEvent(svcController->g_ServiceStopEvent);
		svcController->ReportSvcStatus(svcController->g_ServiceStatus.dwCurrentState, NO_ERROR, 0);
		return;
	case SERVICE_CONTROL_INTERROGATE:
		break;
	default:
		break;
	}
}

void WINAPI SvcMain(int dwArgc, char* lpszArgv[])
{
	//a = new QCoreApplication(argc, argv);
	//a = new QCoreApplication(dwArgc, lpszArgv);

	if (testing)
	{
		SvcInit();
		return;
	}

	svcController->g_StatusHandle = RegisterServiceCtrlHandlerA(
		SERVICE_NAME,
		SvcCtrlHandler
	);

	if (!svcController->g_StatusHandle)
	{
		return;
	}

	ZeroMemory(&svcController->g_ServiceStatus, sizeof(svcController->g_ServiceStatus));
	svcController->g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	svcController->g_ServiceStatus.dwServiceSpecificExitCode = 0;

	svcController->ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

	SvcInit();


	//delete a;
}

void SvcInit()
{

	if (testing)
	{
		SvcWorkerThread(&svcController->g_ServiceStopEvent);
		return;
	}
	
	svcController->g_ServiceStopEvent = CreateEventA(
		NULL,
		TRUE,
		FALSE,
		NULL
	);

	if (svcController->g_ServiceStopEvent == NULL)
	{
		svcController->ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
		return;
	}

	svcController->ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

	// Do Work Here
	HANDLE hThread = CreateThread(NULL, 0, SvcWorkerThread, &svcController->g_ServiceStopEvent, 0, NULL);
	//SvcWorkerThread(&g_ServiceStopEvent);

	/*while (1)
	{
		WaitForSingleObject(
			g_ServiceStopEvent,
			INFINITE
		);
	}*/
	if (hThread)
		WaitForSingleObject(
			svcController->g_ServiceStopEvent,
			INFINITE
		);

	svcController->ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
	CloseHandle(svcController->g_ServiceStopEvent);

	return;

}

DWORD WINAPI SvcWorkerThread(LPVOID lpParam)
{
	int argc = 0;
	char* argv[1];

	// add registering registering application in event log and removing on exit.
	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\").append(SERVICE_NAME));
	string evtMsgFile = RegistryManager().GetStrVal(hKey, "EventMessageFile", REG_SZ);
	DWORD typesSupported = RegistryManager().GetVal(hKey, "TypesSupported", REG_DWORD);

	HKEY serviceKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));
	string installDir = RegistryManager().GetStrVal(serviceKey, "INSTALL_DIR", REG_SZ);
	RegCloseKey(serviceKey);

	installDir.append("\\event_log.dll");
	if (evtMsgFile != installDir) {
		RegistryManager().SetVal(hKey, "EventMessageFile", installDir, REG_SZ);
	}
	RegistryManager().SetVal(hKey, "TypesSupported", 7, REG_DWORD);

	RegCloseKey(hKey);

	EventManager(SERVICE_NAME).ReportCustomEvent(SERVICE_NAME, "Service started", 0);
		
	a = new QCoreApplication(argc, argv);
	HenchmanService service;
	
	while (WaitForSingleObject(svcController->g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		
		service.MainFunction();
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
	
	a->deleteLater();
	EventManager(SERVICE_NAME).ReportCustomEvent(SERVICE_NAME, "Service has exited", 0);

	return ERROR_SUCCESS;
}

static void setContextMenuCommands(string verb, string command)
{
	string verbName = verb;
	std::erase(verbName, ' ');
	HKEY hKey = RegistryManager().OpenKey(HKEY_CLASSES_ROOT, string(SERVICE_NAME).append("\\Shell\\"+verbName));
	RegistryManager().SetVal(hKey, "MUIVerb", verb, REG_SZ);
	RegCloseKey(hKey);
	hKey = RegistryManager().OpenKey(HKEY_CLASSES_ROOT, string(SERVICE_NAME).append("\\Shell\\"+ verbName +"\\command"));
	RegistryManager().SetVal(hKey, "", command, REG_SZ);
	RegCloseKey(hKey);
}

static void setContextMenu(string installDir)
{
	HKEY hKey = RegistryManager().OpenKey(HKEY_CLASSES_ROOT, string("*\\shell\\").append(SERVICE_NAME));
	RegistryManager().SetVal(hKey, "MUIVerb", "HenchmanService", REG_SZ);
	RegistryManager().SetVal(hKey, "ExtendedSubCommandsKey", string(SERVICE_NAME), REG_SZ);
	RegistryManager().SetVal(hKey, "Extended", "", REG_SZ);
	RegistryManager().SetVal(hKey, "AppliesTo", string("System.ItemName:\"").append(SERVICE_NAME).append(".exe\""), REG_SZ);

	RegCloseKey(hKey);

	setContextMenuCommands("Start Service", "\"" + installDir + "\\HenchmanService.exe\" \"--start\" \"%1\"");
	setContextMenuCommands("Stop Service", "\"" + installDir + "\\HenchmanService.exe\" \"--stop\" \"%1\"");
	setContextMenuCommands("Install Service", "\"" + installDir + "\\HenchmanService.exe\" \"--install\" \"%1\"");
	setContextMenuCommands("Remove Service", "\"" + installDir + "\\HenchmanService.exe\" \"--remove\" \"%1\"");

}

static void removeContextMenu()
{
	RegistryManager().RemoveKey(HKEY_CLASSES_ROOT, string("*\\shell\\").append(SERVICE_NAME));
}

HenchmanService::HenchmanService()
{
	CSimpleIniA ini;

	ini.SetUnicode();

	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));

	string installDir = RegistryManager().GetStrVal(hKey, "INSTALL_DIR", REG_SZ);

	cout << "Install dir: " << installDir << endl;
	SI_Error rc = ini.LoadFile((installDir + "\\service.ini").data());
	if (rc < 0) {
		cerr << "Failed to Load INI File" << endl;
	}

	CSimpleIniA::TNamesDepend keys;
	ini.GetAllKeys("WAMP", keys);
	for (auto const& val : keys)
	{
		string key = val.pItem;
		string value = ini.GetValue("WAMP", val.pItem, "");
		ServiceHelper().removeQuotes(value);

		RegistryManager().GetStrVal(hKey, key.data(), REG_SZ);
		RegistryManager().SetVal(hKey, key.data(), value, REG_SZ);

		key.clear();
		value.clear();
	}

	ini.GetAllKeys("TRAK", keys);
	for (auto const& val : keys)
	{
		string key = val.pItem;
		string value = ini.GetValue("TRAK", val.pItem, "");
		ServiceHelper().removeQuotes(value);

		RegistryManager().GetStrVal(hKey, key.data(), REG_SZ);
		RegistryManager().SetVal(hKey, key.data(), value, REG_SZ);

		key.clear();
		value.clear();

	}

	RegCloseKey(hKey);
	sqliteManager = make_unique<SQLiteManager2>(a);


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
	dbManager = make_unique<DatabaseManager>(a);

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
	std::cout << "Deconstructing HenchmanService" << endl;
	//delete SQLiteM;
	//delete TrakM;
	//delete dbManager;
	//dbManager->deleteLater();

	//logx.clear();
}

vector<string> HenchmanService::Explode(const string& Seperator, string& s, int limit)
{
	vector<string> results;
	try {
		if (s == "")
			throw HenchmanServiceException("No String was provided");
		if (Seperator == "")
			throw HenchmanServiceException("Invalid Seperator provided");
		if (limit > s.size())
			throw HenchmanServiceException("Invalid Integer provided");

		size_t pos = 0;
		string token;
		while ((pos = s.find(Seperator)) != string::npos and (limit <= 0 ? true : results.size() <= limit)) {
			std::cout << results.size() << endl;
			token = s.substr(0, pos);
			results.push_back(token);
			s.erase(0, pos + Seperator.length());
		}
		//results.push_back(s);
		token.clear();
	}
	catch (exception& e)
	{
		ServiceHelper().WriteToError(e.what());
	}
	return results;
}

bool HenchmanService::setMailLogin(string& username, string& password) 
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
		QTimer::singleShot(100, a, &QCoreApplication::quit);
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
		if (!ShellExecuteApp(targetExe, ""))
		{
			ServiceHelper().WriteToError("Failed to start " + targetExe);
		}
	}
	dbManager->targetApp = TrakM.appType;

	dbManager->connectToLocalDB();

	if (!TrakM.UploadCurrentStateToRemote())
	{
		dbManager->connectToRemoteDB();
	}
	else {
		QTimer::singleShot(1000, a, &QCoreApplication::quit);
	}

	/*if (!(dbManager->AddKabsIfNotExists() ||
		dbManager->AddDrawersIfNotExists() ||
		dbManager->AddToolsIfNotExists() ||
		dbManager->AddToolsInDrawersIfNotExists())
		) {
		dbManager->connectToRemoteDB();
	}*/
	
	std::cout << "Exiting Main Function" << endl;
	
	return 0;
}

void HenchmanService::checkStateOfMySQL()
{
	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));
	string mysql_dir = RegistryManager().GetStrVal(hKey, "MySQL_DIR", REG_SZ);
	RegCloseKey(hKey);
	ServiceHelper().WriteToLog("Checking for Local MYSQL service...");
	int wampMySQLSvcStatus = svcController->GetSvcStatus("wampmysqld64");
	try {
		switch (wampMySQLSvcStatus) {
		case -1: {
			ServiceHelper().WriteToError("MySQL Service errored with unknown error");
			if (!ShellExecuteApp(mysql_dir + "\\mysqld.exe", " --install-manual wampmysqld64"))
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
		if (ShellExecuteApp(mysql_dir + "\\mysqld.exe", " --remove wampmysqld64"))
			ServiceHelper().WriteToLog("Removed Local MYSQL service...");
	}

}

void HenchmanService::checkStateOfApache()
{
	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));
	string apache_dir = RegistryManager().GetStrVal(hKey, "Apache_DIR", REG_SZ);
	RegCloseKey(hKey);
	ServiceHelper().WriteToLog("Checking for Local Apache service...");
	int wampApacheSvcStatus = svcController->GetSvcStatus("wampapache64");
	try {
		switch (wampApacheSvcStatus) {
		case -1: {
			ServiceHelper().WriteToError("Apache Service errored with unknown error");
			if (!ShellExecuteApp(apache_dir + "\\httpd.exe", " -k install -n wampapache64"))
				throw HenchmanServiceException("Failed to install Apache service");
			ServiceHelper().WriteToLog("Apache Service installed...");
			if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k start -n wampapache64"))
				ServiceHelper().WriteToLog("Apache Services started...");
			else {
				throw HenchmanServiceException("Failed to start Apache service");
			}
			break;

		}
		case 1: {
			ServiceHelper().WriteToLog("Apache Service has stopped...");
			if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k start -n wampapache64"))
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
		if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k stop -n wampapache64"))
			ServiceHelper().WriteToLog("Apache Service stopped...");
		if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k stop -n wampapache"))
			ServiceHelper().WriteToLog("Apache Service stopped...");
		if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k uninstall -n wampapache64"))
			ServiceHelper().WriteToLog("Apache Service uninstalled...");
		if (ShellExecuteApp(apache_dir + "\\httpd.exe", " -k uninstall -n wampapache"))
			ServiceHelper().WriteToLog("Apache Service uninstalled...");
	}
}

int main(int argc, char* argv[])
{

	CSimpleIniA ini;

	SI_Error rc = ini.LoadFile(".\\service.ini");
	if (rc < 0) {
		cerr << "Failed to Load INI File" << endl;
	}
	else {
		testing = ini.GetBoolValue("DEVELOPMENT", "testingMain", 0);
	}

	if (testing && argc <= 1)
		argv[1] = (char *)"--install";
	
	svcController = make_unique<ServiceController>(SERVICE_NAME, SERVICE_DISPLAY_NAME);

	if (lstrcmpiA(argv[1], "--install") == 0)
	{
		HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\").append(SERVICE_NAME));
		char buff[MAX_PATH];
		int byteLength = GetCurrentDirectoryA(sizeof(buff), buff);
		string installDir = RegistryManager().GetStrVal(hKey, "INSTALL_DIR", REG_SZ);
		if (installDir == "" || installDir != buff)
		{
			installDir = buff;
			std::cout << installDir << endl;
			RegistryManager().SetVal(hKey, "INSTALL_DIR", installDir, REG_SZ);
		}
		RegCloseKey(hKey);
		setContextMenu(installDir);
		
		if (!testing) {
			svcController->DoInstallSvc();
			Sleep(1000);
			ShellExecuteApp(installDir + "\\" + SERVICE_NAME + ".exe", "--start");
			return 0;
		}
		cout << "installing..." << endl;
		Sleep(1000);
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
			std::cout << "stopping..." << endl;
			std::cout << "removing..." << endl;
		}
		removeContextMenu();
		int c = getchar();
		return 0;
	}

	if (lstrcmpiA(argv[1], "--start") == 0)
	{
		if (!testing){
			svcController->DoStartSvc();
			return 0;
		}
		std::cout << "starting..." << endl;
	}

	if (lstrcmpiA(argv[1], "--stop") == 0)
	{
		if (!testing) {
			svcController->DoStopSvc();
			ServiceHelper().WriteToLog("Service has stopped");
		}
		else
			std::cout << "stopping..." << endl;
		int c = getchar();
		return 0;
	}

	if (testing) {
		SvcMain(argc, argv);
		return 0;
	}

	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{QString(SERVICE_NAME).toStdWString().data(), (LPSERVICE_MAIN_FUNCTION)SvcMain},
		{NULL, NULL}
	};

	if (!StartServiceCtrlDispatcher(ServiceTable))
	{
		std::cout << "Failed to find Registered Service Controller." << std::endl;
		std::cout << "Press enter to install service or hit CTRL+C to exit..." << std::endl;
		int c = getchar();
		if (c == '\n' || c == EOF)
			ShellExecuteApp(string(SERVICE_NAME) + ".exe", "--install");
		std::cout << "Press any key to exit..." << endl;
	}
	//delete svcController;
	//svcController = nullptr;
	return 0;
}