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
///  - manage the registry associacted with the Service
///  - manage the ini files associated with the Service
/// </summary>

#include "HenchmanServices.h"
using namespace std;


stringstream HenchmanService::logx;
SOCKET HenchmanService::mailSocket = INVALID_SOCKET;
SSL_CTX* HenchmanService::ctx;
SSL* HenchmanService::ssl;
struct addrinfo* HenchmanService::mailAddrInfo = NULL;
string HenchmanService::app_path = "";
string HenchmanService::mail_username = "";
string HenchmanService::mail_password = "";

string GetExportsPath(string app_path = "") {
	string exportsPath;
	int _results = 0;
	char buff[1024];

	if (app_path == "") {
		do {
			_results = GetCurrentDirectory(sizeof(buff), buff);
			exportsPath = buff;
		} while (_results > exportsPath.length() && exportsPath.data());
	}
	else {
		exportsPath = app_path.substr(0, app_path.find_last_of("/\\"));
	}

	exportsPath.append("\\exports\\");
	if (!filesystem::is_directory(exportsPath.c_str())) {
		filesystem::create_directory(exportsPath.c_str());
	}
	return exportsPath;
}

string GetLogsPath(string app_path = "") {
	string logsPath;
	int _results = 0;
	char buff[1024];
	if (app_path == "") {
		do {
			_results = GetCurrentDirectory(sizeof(buff), buff);
			logsPath = buff;
		} while (_results > logsPath.length() && logsPath.data());
	}
	else {
		logsPath = app_path.substr(0, app_path.find_last_of("/\\"));
	}
	logsPath.append("\\logs\\");
	if (!filesystem::is_directory(logsPath.c_str())) {
		filesystem::create_directory(logsPath.c_str());
	}
	return logsPath;
}

void HenchmanService::WriteToLog(string log) {
	if (log == "") {
		log = logx.str();
	}
	string logDir = GetLogsPath();
	logDir.append("log.txt");
	 fstream fs(logDir.c_str(), ios::out | ios_base::app);
	if (fs) {
		time_t timer = time(NULL);
		struct tm currDateTime = *localtime(&timer);
		char dateBuf[120];
		char timeBuf[120];
		strftime(dateBuf, sizeof(dateBuf), "%d/%m/%Y", &currDateTime);
		strftime(timeBuf, sizeof(timeBuf), "%T", &currDateTime);
		fs << "###-< " << dateBuf << " " << timeBuf << " >-###\n\n";
		fs << log << '\n';
		fs.close();
	}
	logx.str(string());
}

void HenchmanService::WriteToError(string log) {
	if (log == "") {
		log = logx.str();
	}
	string logDir = GetLogsPath();
	logDir.append("error.txt");
	fstream fs(logDir.c_str(), ios::out | ios_base::app);
	if (fs) {
		time_t timer = time(NULL);
		struct tm currDateTime = *localtime(&timer);
		char dateBuf[120];
		char timeBuf[120];
		strftime(dateBuf, sizeof(dateBuf), "%F", &currDateTime);
		strftime(timeBuf, sizeof(timeBuf), "%T", &currDateTime);
		fs << "###-< " << dateBuf << " " << timeBuf << " >-###\n\n";
		fs << log << '\n';
		fs.close();
	}
	logx.str(string());
}

vector<string> HenchmanService::Explode(const string &Seperator, string &s, int limit) {
	if (s == "")
		throw HenchmanServiceException("No String was Provided");
	if (limit < 0)
		throw HenchmanServiceException("Invalid Integer Provided");
	vector<string>results;
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

void Check(int iStatus, string szFunction)
{
	if ((iStatus != SOCKET_ERROR) && (iStatus))
		cout << "No error duing call to " << szFunction.c_str() << ": " << iStatus << endl;
	return;

	cout << "Error during call to " << szFunction.c_str() << ": " << iStatus << " - " << GetLastError() << endl;
}

long int microseconds() {
	struct timespec tp;
	timespec_get(&tp, TIME_UTC);
	long int ms = tp.tv_sec * 1000 + tp.tv_nsec / 1000;
	return ms;
}

bool checkForInternetConnection() {
	string loghash = to_string(microseconds());
	bool returnState = false;
	if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) {
		INetworkListManager* pNetworkListManager = NULL;
		VARIANT_BOOL isConnected;
		//logx << "\n--- " << loghash << " ---\r\n\n";
		if (SUCCEEDED(CoCreateInstance(CLSID_NetworkListManager, NULL, CLSCTX_ALL, IID_INetworkListManager, (LPVOID*)&pNetworkListManager))) {
			//logx << "--- " << "Successfully created instance of network list manager." << " ---" << endl;
			//IEnumNetworkConnections* pEnum;
			//logx << "\n--- " << loghash << " ---\r\n\n";
			if (SUCCEEDED(pNetworkListManager->get_IsConnectedToInternet(&isConnected))) {
				//logx << "--- " << "Confirming existance of internet connection" << " ---" << endl;
				//logx << "\n--- " << loghash << " ---\r\n\n";
				if (!isConnected) {
					//logx << "--- " << "No Internet Connection." << " ---" << endl;
					goto Exit;
				}
				//logx << "--- " << "Internet Connection was Confirmed." << " ---" << endl;
				returnState = isConnected;
				goto Exit;
			}
			//logx << "--- " << "Could not confirm existance of internet connection" << " ---" << endl;
			printf("internet not connected");
			goto Exit;
		}
		//logx << "--- " << "Failed to create instance of network list manager." << " ---" << endl;
		goto Exit;
	}
Exit:
	CoUninitialize();
	/*cout << logx.str();
	WriteToLog(logx.str());*/
	return returnState;
}

bool isInternetConnected() {
	WSADATA wsaData;
	int iResult;
	string loghash = to_string(microseconds());
	SOCKET ConnectionCheck = INVALID_SOCKET;
	struct sockaddr_in clientService;

	struct addrinfo* httpAddrInfo = NULL;
	struct addrinfo hints;

	try {
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != NO_ERROR) {
			printf("WSAStartup failed: %d\n", iResult);
			return false;
		}

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_protocol = IPPROTO_TCP;

		//logx << "\n--- " << loghash << " ---\r\n" << endl;
		//logx << "--- " << "Getting Address Info" << " ---\r\n" << endl;
		iResult = getaddrinfo("www.google.com", "https", &hints, &httpAddrInfo);
		if (iResult != NO_ERROR) {
			printf("getaddrinfo failed with error: %d\n", iResult);
			freeaddrinfo(httpAddrInfo);
			WSACleanup();
			return false;
		}

		//logx << "\n--- " << loghash << " ---\r\n" << endl;
		//logx << "--- " << "Setting up Network Check Socket" << " ---\r\n" << endl;
		ConnectionCheck = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (ConnectionCheck == INVALID_SOCKET) {
			printf("Failed to connect to Socket: %ld\n", WSAGetLastError());
			closesocket(ConnectionCheck);
			freeaddrinfo(httpAddrInfo);
			WSACleanup();
			return false;
		}
		
		clientService.sin_family = AF_INET;
		//clientService.sin_addr.s_addr = inet_addr("192.168.2.36");
		clientService.sin_port = htons(IPPORT_HTTPS);
		inet_pton(AF_INET, inet_ntoa(((struct sockaddr_in*)httpAddrInfo->ai_addr)->sin_addr), (SOCKADDR*)&clientService.sin_addr.s_addr);
		//logx << "\n--- " << loghash << " ---\r\n" << endl;
		//logx << "---" << "Connecting to Google.com via Socket" << "---\r\n" << endl;
		iResult = connect(ConnectionCheck, (SOCKADDR*)&clientService, sizeof(clientService));
		if (iResult == SOCKET_ERROR) {
			printf("Unable to connect to server: %ld\n", WSAGetLastError());
			closesocket(ConnectionCheck);
			freeaddrinfo(httpAddrInfo);
			WSACleanup();
			return false;
		}
		else {
			cout << "Connected to: " << inet_ntoa(clientService.sin_addr) << " on port: " << clientService.sin_port << endl;
		}
		//cout << logx.str();
		//WriteToLog(logx.str());
		closesocket(ConnectionCheck);
		freeaddrinfo(httpAddrInfo);
		WSACleanup();
	}
	catch (exception& e) {
		//logx << "\n---" << loghash << "---\r\n" << endl;
		//cout << logx.str();
		//WriteToError(logx.str());
		return false;
	}
	return true;
}

bool Contain(string str, string search) {
	//cout << "searching: " << str.data() << " for: " << search.data() << endl;
	size_t found = str.find(search);
	if (found != string::npos) {
		return 1;
	}
	return 0;
}

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

void sslError(SSL* ssl, int received, string microtime, stringstream& logi) {
	const int err = SSL_get_error(ssl, received);
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

char* base64(string string) {
	// Credit to mtrw from Stackoverflow
	const auto pl = 4 * ((string.size() + 2) / 3);
	auto output = reinterpret_cast<char*>(calloc(pl + 1, 1)); //+1 for the terminating null that EVP_EncodeBlock adds on
	const auto ol = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(output), reinterpret_cast<unsigned char*>(string.data()), string.size());
	if (pl != ol) { cerr << "Whoops, encode predicted " << pl << " but we got " << ol << "\n"; }
	return output;
}

string fileBasename(string path) {
	string filename = path.substr(path.find_last_of("/\\") + 1);
	return filename;
	// without extension
	// string::size_type const p(base_filename.find_last_of('.'));
	// string file_without_extension = base_filename.substr(0, p);
}

string get_file_contents(const char* filename)
{
	
	if (ifstream is{ filename, ios::binary | ios::ate })
	{
		auto size = is.tellg();
		string str(size, '\0'); // construct string to stream size
		is.seekg(0);
		is.read(&str[0], size);
		/*if (is)
			cout << str << '\n';*/
		return str.c_str();
	}
	throw(errno);
}

string GetFileExtension(const string& FileName)
{
	if (FileName.find_last_of(".") != string::npos)
		return FileName.substr(FileName.find_last_of(".") + 1);
	return "";
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

bool ProcessExists(string exeFileName) {
	
	bool ContinueLoop;
	HANDLE SnapshotHandle;
	PROCESSENTRY32 ProcessEntry32;
	
	SnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	ProcessEntry32.dwSize = sizeof(ProcessEntry32);
	ContinueLoop = Process32First(SnapshotHandle, &ProcessEntry32);
	bool result = true;
	while (ContinueLoop) {
		string targetEXE = exeFileName;
		transform(targetEXE.begin(), targetEXE.end(), targetEXE.begin(), ::toupper);
		string processEXE =ProcessEntry32.szExeFile;
		transform(processEXE.begin(), processEXE.end(), processEXE.begin(), ::toupper);
		string processEXEFileName = fileBasename(ProcessEntry32.szExeFile);
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
	struct stat buffer;
	cout << "Checking if: " << fileName << " is being used" << endl;
	if (filesystem::exists(fileName)) {
		cout << "Target File Exists" << endl;
		fileRes = CreateFile(fileName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		return (fileRes == INVALID_HANDLE_VALUE);
	}
	cout << "Target File Does Not Exists Or Could Not Be Found" << endl;
	return false;
}

bool HenchmanService::setMailLogin(string username, string password) {
	mail_username = username;
	mail_password = password;
	if (mail_username.length() <= 1 || mail_password.length() <= 1) {
		return false;
	}
	return true;
}

void HenchmanService::ConnectWithSMTP() {
	WSADATA wsaData;
	int iResult;
	string loghash = to_string(microseconds());

	struct sockaddr_in clientService;
	vector<string> files;
	
	ctx = InitCTX();
	
	char buff[1024];
	int buffLen = sizeof(buff);

	struct addrinfo hints;
	string reply;
	int iProtocolPort;
	try {
		logx << "\n---" << loghash << "---\r\n" << endl;
		logx << "---" << "Socket Setup" << "---\r\n" << endl;
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != NO_ERROR) {
			printf("WSAStartup failed: %d\n", iResult);
			return;
		}

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		logx << "\n---" << loghash << "---\r\n" << endl;
		logx << "---" << "Getting Address Info" << "---\r\n" << endl;
		iResult = getaddrinfo("mail.henchmantrak.com", "smtp", &hints, &mailAddrInfo);
		if (iResult != NO_ERROR) {
			printf("getaddrinfo failed with error: %d\n", iResult);
			WSACleanup();
			return;
		}
		logx << "\n---" << loghash << "---\r\n" << endl;
		logx << "---" << "Setting up SMTP Mail Socket" << "---\r\n" << endl;
		mailSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (mailSocket == INVALID_SOCKET) {
			printf("Failed to connect to Socket: %ld\n", WSAGetLastError());
			WSACleanup();
			return;
		}
		logx << "\n---" << loghash << "---\r\n" << endl;
		logx << "---" << "Getting Mail Service Port" << "---\r\n" << endl;
		LPSERVENT lpServEntry = getservbyname("mail", 0);
		if (!lpServEntry) {
			logx << "using IPPORT_SMTP" << endl;
			iProtocolPort = htons(IPPORT_SMTP);
			//iProtocolPort = 465;
		}
		else {
			logx << "using port provided from lpServEntry" << endl;
			iProtocolPort = lpServEntry->s_port;
		}
		cout << "Connecting on port: " << iProtocolPort << endl;

		clientService.sin_family = AF_INET;
		//clientService.sin_addr.s_addr = inet_addr("192.168.2.36");
		clientService.sin_port = iProtocolPort;
		inet_pton(AF_INET, inet_ntoa(((struct sockaddr_in*)mailAddrInfo->ai_addr)->sin_addr), (SOCKADDR*)&clientService.sin_addr.s_addr);
		logx << "\n---" << loghash << "---\r\n" << endl;
		logx << "---" << "Connecting to Mail Socket" << "---\r\n" << endl;
		iResult = connect(mailSocket, (SOCKADDR*)&clientService, sizeof(clientService));
		if (iResult == SOCKET_ERROR) {
			printf("Unable to connect to server: %ld\n", WSAGetLastError());
			WSACleanup();
			return;
		}
		else {
			logx << "Connected to: " << inet_ntoa(clientService.sin_addr) << " on port: " << iProtocolPort << endl;
		}
		
		logx << "\n---" << loghash << "---\r\n" << endl;
		logx << "---" << "Initiating communication through SMTP" << "---\r\n" << endl;
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
				logx << "\n---" << loghash << "---\r\n" << endl;
				cout << logx.str();
				WriteToError(logx.str());
				return;
			}
		}

		if (!Contain(string(buff), "STARTTLS")) {
			logx << "[EXTERNAL_SERVER_NO_TLS] " << "mail.henchmantrak.com" << " " << buff << "[CLOSING_CONNECTION]" << endl;
			logx << "\n---" << loghash << "---\r\n" << endl;
			cout << logx.str();
			WriteToError(logx.str());
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

		logx << "\n---" << loghash << "---\r\n" << endl;
		logx << "Connection....smtp" << endl;
		cout << "Connection to smtp via tls" << endl;

		if (SSL_connect(ssl) == -1) {
			// ERR_print_errors_fp(stderr);            
			logx << "[TLS_SMTP_ERROR]" << endl;
			logx << "\n---" << loghash << "---\r\n" << endl;
			cout << logx.str();
			WriteToError(logx.str());
			return;
		}
		else {
			// char *msg = (char*)"{\"from\":[{\"name\":\"Zenobiusz\",\"email\":\"email@eee.ddf\"}]}";
			logx << "\n---" << loghash << "---\r\n" << endl;
			logx << "Connected with " << SSL_get_cipher(ssl) << " encryption" << endl;
			string cert = ShowCerts(ssl);
			cout << cert << endl;

			vector<string> attachments;
			for (const auto& entry : filesystem::directory_iterator(GetExportsPath())) {
				cout << entry.path().string() << endl;
				attachments.push_back(entry.path().string());
			}
			logx << "\n---" << loghash << "---\r\n" << endl;
			logx << "---" << "Generating and sending Email" << "---\r\n" << endl;
			cout << logx.str();
			WriteToLog(logx.str());
			SendEmail(ssl, attachments);
		}
		sslError(ssl, 1, loghash, logx);

		closesocket(mailSocket);
		SSL_CTX_free(ctx);
		freeaddrinfo(mailAddrInfo);
		WSACleanup();
	}
	catch (exception& e) {
		logx << "\n---" << loghash << "---\r\n" << endl;
		cout << logx.str();
		WriteToError(logx.str());
		return;
	}
	logx.str(string());
}

void HenchmanService::SendEmail(SSL* &ssl, vector<string>&attachments) {

	string loghash = to_string(microseconds());
		
	char buff[1024];
	int buffLen = sizeof(buff);
	int counter = 1;
	try {
		if (SSL_connect(ssl) == -1) {
			// ERR_print_errors_fp(stderr);            
			logx << "[TLS_SMTP_ERROR]" << endl;
			logx << "\n---" << loghash << "---\r\n" << endl;
			cout << logx.str();
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
			f2 = mail_username + "\0"s + mail_username + "\0"s + mail_password;
			f1 << base64(f2);
			f1 << " \r\n";
			string f11 = f1.str();
			char* auth = f11.data();
			logx << "SEND TO SERVER " << auth<< endl;
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
			logx << "\n---" << loghash << "---" << endl << endl;
			counter++;

			// send log
			cout << logx.str();
			WriteToLog(logx.str());
			if (!Contain(string(buff), "250"))return;

			char* qdata = (char*)"quit\r\n";
			SSL_write(ssl, qdata, strlen(qdata));
			bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << endl;
			if (!Contain(string(buff), "221"))return;

			SSL_free(ssl);
			return;
		}

	}
	catch (exception& e) {
		logx << "\n---" << loghash << "---\r\n" << endl;
		cout << logx.str();
		WriteToError(logx.str());
		return;
	}
}

int ShellExecuteApp(string appName, string params)
{
	SHELLEXECUTEINFO SEInfo;
	DWORD ExitCode;
	string exeFile = appName;
	string paramStr = params;
	string StartInString;

	// fine the windows handle using https://learn.microsoft.com/en-us/troubleshoot/windows-server/performance/obtain-console-window-handle
	//HWND windowHandle;
	//char newWindowTitle[1024];
	//char oldWindowTitle[1024];

	//GetConsoleTitle(oldWindowTitle, sizeof(oldWindowTitle));
	//
	//printf(oldWindowTitle);
	//printf("\n");
	Sleep(5000);

	//fill_n(SEInfo, sizeof(SEInfo), NULL);
	SEInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	SEInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	SEInfo.hwnd = NULL;
	SEInfo.lpVerb = NULL;
	SEInfo.lpDirectory = NULL;
	SEInfo.lpFile = exeFile.c_str();
	SEInfo.lpParameters = paramStr.c_str();
	SEInfo.nShow = paramStr == "" ? SW_NORMAL : SW_HIDE;
	if(ShellExecuteEx(&SEInfo)){
		do {
			GetExitCodeProcess(SEInfo.hProcess, &ExitCode);
			cout << ExitCode << endl;
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
		szPath,						// path to service's binary 
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
	if (!ssStatus.dwCurrentState == SERVICE_RUNNING)
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

void __stdcall DoStopSvc(const char* sService)
{
	SERVICE_STATUS_PROCESS ssp;
	DWORD dwStartTime = GetTickCount64();
	DWORD dwBytesNeeded;
	DWORD dwTimeout = (30*1000); // 30-second time-out
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
	schService = OpenService(
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

bool __stdcall StopDependentServices()
{
	DWORD i;
	DWORD dwBytesNeeded;
	DWORD dwCount;

	LPENUM_SERVICE_STATUS   lpDependencies = NULL;
	ENUM_SERVICE_STATUS     ess;
	SC_HANDLE               hDepService;
	SERVICE_STATUS_PROCESS  ssp;


	DWORD dwStartTime = GetTickCount64();
	DWORD dwTimeout = (30*1000); // 30-second time-out

	// Pass a zero-length buffer to get the required buffer size.
	if (EnumDependentServices(schService, SERVICE_ACTIVE,
		lpDependencies, 0, &dwBytesNeeded, &dwCount))
	{
		// If the Enum call succeeds, then there are no dependent
		// services, so do nothing.
		return TRUE;
	}
	else
	{
		if (GetLastError() != ERROR_MORE_DATA)
			return FALSE; // Unexpected error

		// Allocate a buffer for the dependencies.
		lpDependencies = (LPENUM_SERVICE_STATUS)HeapAlloc(
			GetProcessHeap(), HEAP_ZERO_MEMORY, dwBytesNeeded);

		if (!lpDependencies)
			return FALSE;

		__try {
			// Enumerate the dependencies.
			if (!EnumDependentServices(schService, SERVICE_ACTIVE,
				lpDependencies, dwBytesNeeded, &dwBytesNeeded,
				&dwCount))
				return FALSE;

			for (i = 0; i < dwCount; i++)
			{
				ess = *(lpDependencies + i);
				// Open the service.
				hDepService = OpenService(schSCManager,
					ess.lpServiceName,
					SERVICE_STOP | SERVICE_QUERY_STATUS);

				if (!hDepService)
					return FALSE;

				__try {
					// Send a stop code.
					if (!ControlService(hDepService,
						SERVICE_CONTROL_STOP,
						(LPSERVICE_STATUS)&ssp))
						return FALSE;

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
							return FALSE;

						if (ssp.dwCurrentState == SERVICE_STOPPED)
							break;

						if (GetTickCount() - dwStartTime > dwTimeout)
							return FALSE;
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
	return TRUE;
}

void __stdcall DoDeleteSvc(const char* sService)
{
	SERVICE_STATUS ssStatus;

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
		return 0;
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
		return 0;
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

void WINAPI SvcMain()
{
	g_StatusHandle = RegisterServiceCtrlHandlerA(
		SERVICE_NAME,
		SvcCtrlHandler
	);

	if(!g_StatusHandle)
	{
		return;
	}
	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwServiceSpecificExitCode = 0;

	ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

	SvcInit();
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

	while (1)
	{
		WaitForSingleObject(
			g_ServiceStopEvent,
			INFINITE
		);

		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		return;
	}

}

int main() {
	HenchmanService service;
	SQLite_Manager SQLiteM;
	SQLiteM.ToggleConsoleLogging();
	CSimpleIni ini;
	ini.SetUnicode();
	
	cout << "Export path: " << GetExportsPath() << endl;
	//service.app_path = "C:\\FPC\\Kaptap.exe";
	cout << "Logs path: " << GetLogsPath() << endl;
	vector<string> explodedString;
	try {
		string s = "Hello World Jack Son";
		explodedString = service.Explode(" ", s, 1);
	}
	catch (const HenchmanServiceException& hse) {
		cout << "An Exception in HenchmanService Occured:" << endl;
		cout << hse.what();
	}
	for(auto i : explodedString) cout << i << endl;
	ProcessExists("HenchmanServices") ? cout << "Yes It Does" : cout << "No It Does Not";
	cout << endl;
	char buff[1024];
	int byteLength = GetCurrentDirectory(sizeof(buff), buff);
	string currDir = buff;
	currDir.resize(byteLength);
	FileInUse(currDir + "\\HenchmanServices.exe") ? cout << "Yes It Is" : cout << "No It Is Not";
	cout << endl;
	cout << "Reading ini file: " << string(currDir + "\\service.ini") << endl;
	SI_Error rc = ini.LoadFile(string(currDir + "\\service.ini").c_str());
	if (rc < 0) {
		cerr << "Failed to Load INI File" << endl;
	}
	const char* username = ini.GetValue("Email", "Username", "");
	const char* password = ini.GetValue("Email", "Password", "");
	if (checkForInternetConnection() && isInternetConnected() && service.setMailLogin(username, password)) {
		cout << "Able to send mail" << endl;
		//service.ConnectWithSMTP();
	}
	//ShellExecuteApp("HenchmanServices.exe", "") ? cout << "Successfully excecuted" << endl : cout << "Failed to excecute" << endl;
	/*InstallMySQL();
	InstallApache();
	InstallOnlineOfflineScript();*/
	cout << "Checking DB" << endl;
	SQLiteM.InitDB();
	explodedString.clear();
	Sleep(5000);
	return 0;
}