// HenchmanServices.cpp : Defines the entry point for the application.
//

#include "HenchmanServices.h"

std::stringstream HenchmanService::logx;
SOCKET HenchmanService::mailSocket = INVALID_SOCKET;
SSL_CTX* HenchmanService::ctx;
SSL* HenchmanService::ssl;
struct addrinfo* HenchmanService::mailAddrInfo = NULL;
std::string HenchmanService::app_path = "";
std::string HenchmanService::mail_username = "";
std::string HenchmanService::mail_password = "";

std::string HenchmanService::GetExportsPath() {
	std::string exportsPath;
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
	if (!std::filesystem::is_directory(exportsPath.c_str())) {
		std::filesystem::create_directory(exportsPath.c_str());
	}
	return exportsPath;
}

std::string HenchmanService::GetLogsPath() {
	std::string logsPath;
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
	if (!std::filesystem::is_directory(logsPath.c_str())) {
		std::filesystem::create_directory(logsPath.c_str());
	}
	return logsPath;
}

void HenchmanService::WriteToLog(std::string log) {
	if (log == "") {
		log = logx.str();
	}
	std::string logDir = GetLogsPath();
	logDir.append("log.txt");
	std::fstream fs(logDir.c_str(), std::ios::out | std::ios_base::app);
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
	logx.str(std::string());
}

void HenchmanService::WriteToError(std::string log) {
	if (log == "") {
		log = logx.str();
	}
	std::string logDir = GetLogsPath();
	logDir.append("error.txt");
	std::fstream fs(logDir.c_str(), std::ios::out | std::ios_base::app);
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
	logx.str(std::string());
}

std::vector<std::string> HenchmanService::Explode(const std::string &Seperator, std::string &s, int limit) {
	if (s == "")
		throw HenchmanServiceException("No String was Provided");
	if (limit < 0)
		throw HenchmanServiceException("Invalid Integer Provided");
	std::vector<std::string>results;
	if (Seperator == "") {
		results.push_back(s);
	}
	else {
		size_t pos = 0;
		std::string token;
		while ((pos = s.find(Seperator)) != std::string::npos and (limit == 0 ? true : results.size() <= limit)) {
			std::cout << results.size() << std::endl;
			token = s.substr(0, pos);
			results.push_back(token);
			s.erase(0, pos + Seperator.length());
		}
		results.push_back(s);
		token.clear();
	}
	return results;
}

void Check(int iStatus, std::string szFunction)
{
	if ((iStatus != SOCKET_ERROR) && (iStatus))
		std::cout << "No error duing call to " << szFunction.c_str() << ": " << iStatus << std::endl;
	return;

	std::cout << "Error during call to " << szFunction.c_str() << ": " << iStatus << " - " << GetLastError() << std::endl;
}

long int microseconds() {
	struct timespec tp;
	timespec_get(&tp, TIME_UTC);
	long int ms = tp.tv_sec * 1000 + tp.tv_nsec / 1000;
	return ms;
}

bool HenchmanService::isInternetConnected() {
	WSADATA wsaData;
	int iResult;
	std::string loghash = std::to_string(microseconds());
	SOCKET ConnectionCheck = INVALID_SOCKET;
	struct sockaddr_in clientService;

	struct addrinfo* httpAddrInfo = NULL;
	struct addrinfo hints;


	try {
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != NO_ERROR) {
			printf("WSAStartup failed: %d\n", iResult);
			return 1;
		}

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_protocol = IPPROTO_TCP;

		logx << "---" << loghash << "---\r\n" << std::endl;
		logx << "---" << "Getting Address Info" << "---\r\n" << std::endl;
		iResult = getaddrinfo("www.google.com", "https", &hints, &httpAddrInfo);
		if (iResult != NO_ERROR) {
			printf("getaddrinfo failed with error: %d\n", iResult);
			freeaddrinfo(httpAddrInfo);
			WSACleanup();
			return 1;
		}

		logx << "---" << loghash << "---\r\n" << std::endl;
		logx << "---" << "Setting up Network Check Socket" << "---\r\n" << std::endl;
		ConnectionCheck = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (ConnectionCheck == INVALID_SOCKET) {
			printf("Failed to connect to Socket: %ld\n", WSAGetLastError());
			closesocket(ConnectionCheck);
			freeaddrinfo(httpAddrInfo);
			WSACleanup();
			return 1;
		}
		
		clientService.sin_family = AF_INET;
		//clientService.sin_addr.s_addr = inet_addr("192.168.2.36");
		clientService.sin_port = htons(IPPORT_HTTPS);
		inet_pton(AF_INET, inet_ntoa(((struct sockaddr_in*)httpAddrInfo->ai_addr)->sin_addr), (SOCKADDR*)&clientService.sin_addr.s_addr);
		logx << "---" << loghash << "---\r\n" << std::endl;
		logx << "---" << "Connecting to Google.com via Socket" << "---\r\n" << std::endl;
		iResult = connect(ConnectionCheck, (SOCKADDR*)&clientService, sizeof(clientService));
		if (iResult == SOCKET_ERROR) {
			printf("Unable to connect to server: %ld\n", WSAGetLastError());
			closesocket(ConnectionCheck);
			freeaddrinfo(httpAddrInfo);
			WSACleanup();
			return 1;
		}
		else {
			logx << "Connected to: " << inet_ntoa(clientService.sin_addr) << " on port: " << clientService.sin_port << std::endl;
		}
		std::cout << logx.str();
		WriteToLog(logx.str());
		closesocket(ConnectionCheck);
		freeaddrinfo(httpAddrInfo);
		WSACleanup();
	}
	catch (std::exception& e) {
		logx << "---" << loghash << "---\r\n" << std::endl;
		std::cout << logx.str();
		WriteToError(logx.str());
		return 1;
	}
}

bool HenchmanService::Contain(std::string str, std::string search) {
	//std::cout << "searching: " << str.data() << " for: " << search.data() << std::endl;
	std::size_t found = str.find(search);
	if (found != std::string::npos) {
		return 1;
	}
	return 0;
}

std::string HenchmanService::ShowCerts(SSL* ssl)
{
	X509* cert;
	char* line = {};

	cert = SSL_get_peer_certificate(ssl);	// get the server's certificate
	if (cert != NULL)
	{
		std::string log = "Server certificates:\n";
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
SSL_CTX* HenchmanService::InitCTX(void)
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

void HenchmanService::sslError(SSL* ssl, int received, std::string microtime, std::stringstream& logi) {
	const int err = SSL_get_error(ssl, received);
	// const int st = ERR_get_error();
	if (err == SSL_ERROR_NONE) {
		// OK send
		// std::cout<<"SSL_ERROR_NONE:"<<SSL_ERROR_NONE<<std::endl;
		// SSL_shutdown(ssl);        
	}
	else if (err == SSL_ERROR_WANT_READ) {
		logi << "[SSL_ERROR_WANT_READ]" << SSL_ERROR_WANT_READ << std::endl;
		logi << logi.str() << "--[" << microtime << "]--" << std::endl;
		std::cerr << logi.str() << std::endl;
		WriteToError(logi.str());
		SSL_shutdown(ssl);
		//kill(getpid(), SIGKILL);
	}
	else if (SSL_ERROR_SYSCALL) {
		logi << errno << " Received " << received << std::endl;
		logi << "[SSL_ERROR_SYSCALL] " << SSL_ERROR_SYSCALL << std::endl;
		logi << logi.str() << "--[" << microtime << "]--" << std::endl;
		WriteToError(logi.str());
		std::cerr << logi.str() << std::endl;
		SSL_shutdown(ssl);
		//kill(getpid(), SIGKILL);
	}
}

char* base64(std::string string) {
	// Credit to mtrw from Stackoverflow
	const auto pl = 4 * ((string.size() + 2) / 3);
	auto output = reinterpret_cast<char*>(calloc(pl + 1, 1)); //+1 for the terminating null that EVP_EncodeBlock adds on
	const auto ol = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(output), reinterpret_cast<unsigned char*>(string.data()), string.size());
	if (pl != ol) { std::cerr << "Whoops, encode predicted " << pl << " but we got " << ol << "\n"; }
	return output;
}

std::string fileBasename(std::string path) {
	std::string filename = path.substr(path.find_last_of("/\\") + 1);
	return filename;
	// without extension
	// std::string::size_type const p(base_filename.find_last_of('.'));
	// std::string file_without_extension = base_filename.substr(0, p);
}

std::string get_file_contents(const char* filename)
{
	
	if (std::ifstream is{ filename, std::ios::binary | std::ios::ate })
	{
		auto size = is.tellg();
		std::string str(size, '\0'); // construct string to stream size
		is.seekg(0);
		is.read(&str[0], size);
		/*if (is)
			std::cout << str << '\n';*/
		return str.c_str();
	}
	throw(errno);
}

std::string GetFileExtension(const std::string& FileName)
{
	if (FileName.find_last_of(".") != std::string::npos)
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

bool HenchmanService::ProcessExists(std::string exeFileName) {
	
	bool ContinueLoop;
	HANDLE SnapshotHandle;
	PROCESSENTRY32 ProcessEntry32;
	
	SnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	ProcessEntry32.dwSize = sizeof(ProcessEntry32);
	ContinueLoop = Process32First(SnapshotHandle, &ProcessEntry32);
	bool result = true;
	while (ContinueLoop) {
		std::string targetEXE = exeFileName;
		std::transform(targetEXE.begin(), targetEXE.end(), targetEXE.begin(), ::toupper);
		std::string processEXE =ProcessEntry32.szExeFile;
		std::transform(processEXE.begin(), processEXE.end(), processEXE.begin(), ::toupper);
		std::string processEXEFileName = fileBasename(ProcessEntry32.szExeFile);
		std::transform(processEXEFileName.begin(), processEXEFileName.end(), processEXEFileName.begin(), ::toupper);
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

bool HenchmanService::FileInUse(std::string fileName) {
	HANDLE fileRes;
	struct stat buffer;
	std::cout << "Checking if: " << fileName << " is being used" << std::endl;
	if (std::filesystem::exists(fileName)) {
		std::cout << "Target File Exists" << std::endl;
		fileRes = CreateFile(fileName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		return (fileRes == INVALID_HANDLE_VALUE);
	}
	std::cout << "Target File Does Not Exists Or Could Not Be Found" << std::endl;
	return false;
}

bool HenchmanService::setMailLogin(std::string username, std::string password) {
	mail_username = username;
	mail_password = password;
	if (mail_username.length() <= 1 || mail_password.length() <= 1) {
		return false;
	}
	return true;
}

std::optional<SSL*> HenchmanService::ConnectWithSMTP() {
	WSADATA wsaData;
	int iResult;
	std::string loghash = std::to_string(microseconds());

	struct sockaddr_in clientService;
	std::vector<std::string> files;
	
	ctx = InitCTX();
	
	char buff[1024];
	int buffLen = sizeof(buff);

	struct addrinfo hints;
	std::string reply;
	int iProtocolPort;
	try {
		logx << "---" << loghash << "---\r\n" << std::endl;
		logx << "---" << "Socket Setup" << "---\r\n" << std::endl;
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != NO_ERROR) {
			printf("WSAStartup failed: %d\n", iResult);
			return std::nullopt;
		}

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		logx << "---" << loghash << "---\r\n" << std::endl;
		logx << "---" << "Getting Address Info" << "---\r\n" << std::endl;
		iResult = getaddrinfo("mail.henchmantrak.com", "smtp", &hints, &mailAddrInfo);
		if (iResult != NO_ERROR) {
			printf("getaddrinfo failed with error: %d\n", iResult);
			WSACleanup();
			return std::nullopt;
		}
		logx << "---" << loghash << "---\r\n" << std::endl;
		logx << "---" << "Setting up SMTP Mail Socket" << "---\r\n" << std::endl;
		mailSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (mailSocket == INVALID_SOCKET) {
			printf("Failed to connect to Socket: %ld\n", WSAGetLastError());
			WSACleanup();
			return std::nullopt;
		}
		logx << "---" << loghash << "---\r\n" << std::endl;
		logx << "---" << "Getting Mail Service Port" << "---\r\n" << std::endl;
		LPSERVENT lpServEntry = getservbyname("mail", 0);
		if (!lpServEntry) {
			logx << "using IPPORT_SMTP" << std::endl;
			iProtocolPort = htons(IPPORT_SMTP);
			//iProtocolPort = 465;
		}
		else {
			logx << "using port provided from lpServEntry" << std::endl;
			iProtocolPort = lpServEntry->s_port;
		}
		std::cout << "Connecting on port: " << iProtocolPort << std::endl;

		clientService.sin_family = AF_INET;
		//clientService.sin_addr.s_addr = inet_addr("192.168.2.36");
		clientService.sin_port = iProtocolPort;
		inet_pton(AF_INET, inet_ntoa(((struct sockaddr_in*)mailAddrInfo->ai_addr)->sin_addr), (SOCKADDR*)&clientService.sin_addr.s_addr);
		logx << "---" << loghash << "---\r\n" << std::endl;
		logx << "---" << "Connecting to Mail Socket" << "---\r\n" << std::endl;
		iResult = connect(mailSocket, (SOCKADDR*)&clientService, sizeof(clientService));
		if (iResult == SOCKET_ERROR) {
			printf("Unable to connect to server: %ld\n", WSAGetLastError());
			WSACleanup();
			return std::nullopt;
		}
		else {
			logx << "Connected to: " << inet_ntoa(clientService.sin_addr) << " on port: " << iProtocolPort << std::endl;
		}
		
		logx << "---" << loghash << "---\r\n" << std::endl;
		logx << "---" << "Initiating communication through SMTP" << "---\r\n" << std::endl;
		char szMsgLine[255] = "";
		sprintf(szMsgLine, "HELO %s%s", "mail.henchmantrak.com", CRLF);
		std::string E1 = "ehlo ";
		E1.append("mail.henchmantrak.com");
		E1.append(CRLF);
		char* hello = (char*)E1.c_str();
		char* hellotls = (char*)"STARTTLS\r\n";

		// initiate connection
		iResult = recv(mailSocket, buff, sizeof(buff), 0);
		reply = buff;
		reply.resize(iResult);
		logx << "[Server] [" << iResult << "] " << buff << std::endl;
		logx.adjustfield;

		memset(buff, 0, sizeof buff);
		buff[0] = '\0';

		// send ehlo command
		send(mailSocket, hello, strlen(hello), 0);
		logx << "[HELO] " << hello << " " << iResult << std::endl;

		iResult = recv(mailSocket, buff, sizeof(buff), 0);
		reply = buff;
		reply.resize(iResult);
		logx << "[Server] [" << iResult << "] " << reply << std::endl;

		while (!Contain(std::string(buff), "250 ")) {
			iResult = recv(mailSocket, buff, sizeof(buff), 0);
			if (Contain(std::string(buff), "501 ") || Contain(std::string(buff), "503 ")) {
				logx << "---" << loghash << "---\r\n" << std::endl;
				std::cout << logx.str();
				WriteToError(logx.str());
				return std::nullopt;
			}
		}

		if (!Contain(std::string(buff), "STARTTLS")) {
			logx << "[EXTERNAL_SERVER_NO_TLS] " << "mail.henchmantrak.com" << " " << buff << "[CLOSING_CONNECTION]" << std::endl;
			logx << "---" << loghash << "---\r\n" << std::endl;
			std::cout << logx.str();
			WriteToError(logx.str());
			return std::nullopt;
		}

		memset(buff, 0, sizeof buff);
		buff[0] = '\0';
		char buff1[1024];

		// starttls connecion
		send(mailSocket, hellotls, strlen(hellotls), 0);
		logx << "[STARTTLS] " << hellotls << std::endl;

		iResult = recv(mailSocket, buff1, sizeof(buff1), 0);
		reply = buff1;
		reply.resize(iResult);
		logx << "[Server] " << iResult << " " << buff << std::endl;

		ctx = InitCTX();
		ssl = SSL_new(ctx);
		SSL_set_fd(ssl, mailSocket);

		logx << "---" << loghash << "---\r\n" << std::endl;
		logx << "Connection....smtp" << std::endl;
		std::cout << "Connection to smtp via tls" << std::endl;

		if (SSL_connect(ssl) == -1) {
			// ERR_print_errors_fp(stderr);            
			logx << "[TLS_SMTP_ERROR]" << std::endl;
			logx << "---" << loghash << "---\r\n" << std::endl;
			std::cout << logx.str();
			WriteToError(logx.str());
			return std::nullopt;
		}
		else {
			// char *msg = (char*)"{\"from\":[{\"name\":\"Zenobiusz\",\"email\":\"email@eee.ddf\"}]}";
			logx << "---" << loghash << "---\r\n" << std::endl;
			logx << "Connected with " << SSL_get_cipher(ssl) << " encryption" << std::endl;
			std::string cert = ShowCerts(ssl);
			std::cout << cert << std::endl;

			std::vector<std::string> attachments;
			for (const auto& entry : std::filesystem::directory_iterator(GetExportsPath())) {
				std::cout << entry.path().string() << std::endl;
				attachments.push_back(entry.path().string());
			}
			logx << "---" << loghash << "---\r\n" << std::endl;
			logx << "---" << "Generating and sending Email" << "---\r\n" << std::endl;
			std::cout << logx.str();
			WriteToLog(logx.str());
			SendEmail(ssl, attachments);
		}
		sslError(ssl, 1, loghash, logx);

		closesocket(mailSocket);
		SSL_CTX_free(ctx);
		freeaddrinfo(mailAddrInfo);
		WSACleanup();
	}
	catch (std::exception& e) {
		logx << "---" << loghash << "---\r\n" << std::endl;
		std::cout << logx.str();
		WriteToError(logx.str());
		return std::nullopt;
	}
	logx.str(std::string());
}

void HenchmanService::SendEmail(SSL* &ssl, std::vector<std::string>&attachments) {

	std::string loghash = std::to_string(microseconds());
		
	char buff[1024];
	int buffLen = sizeof(buff);
	int counter = 1;
	try {
		if (SSL_connect(ssl) == -1) {
			// ERR_print_errors_fp(stderr);            
			logx << "[TLS_SMTP_ERROR]" << std::endl;
			logx << "---" << loghash << "---\r\n" << std::endl;
			std::cout << logx.str();
			return;
		}
		else {
			buff[0] = '\0';
			std::ostringstream f0;
			f0 << "EHLO " << "mail.henchmantrak.com" << "\r\n";
			std::string f00 = f0.str();
			char* helo = (char*)f00.c_str();
			logx << "SEND TO SERVER " << helo << std::endl;
			SSL_write(ssl, helo, strlen(helo));
			int bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << std::endl;
			if (!Contain(std::string(buff), "250")) return;
			counter++;

			buff[0] = '\0';
			std::ostringstream f1;
			f1 << "AUTH PLAIN ";
			std::string f2;
			using namespace std::string_literals;
			f2 = mail_username + "\0"s + mail_username + "\0"s + mail_password;
			f1 << base64(f2);
			f1 << " \r\n";
			std::string f11 = f1.str();
			char* auth = f11.data();
			logx << "SEND TO SERVER " << auth<< std::endl;
			SSL_write(ssl, auth, strlen(auth));
			bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << std::endl;
			if (!Contain(std::string(buff), "235"))return;
			counter++;

			buff[0] = '\0';
			std::ostringstream f4;
			f4 << "mail from: <" << "test@henchmantrak.com" << ">\r\n";
			std::string f44 = f4.str();
			char* fromemail = (char*)f44.c_str();
			logx << "SEND TO SERVER " << fromemail << std::endl;
			SSL_write(ssl, fromemail, strlen(fromemail));
			bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << std::endl;
			if (!Contain(std::string(buff), "250"))return;
			counter++;


			buff[0] = '\0';
			std::string rcpt = "rcpt to: <";
			rcpt.append("wjaco.swanepoel@gmail.com").append(">\r\n");
			char* rcpt1 = (char*)rcpt.c_str();
			logx << "SEND TO SERVER " << rcpt1 << std::endl;
			SSL_write(ssl, rcpt1, strlen(rcpt1));
			bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << std::endl;
			if (!Contain(std::string(buff), "250"))return;
			counter++;

			buff[0] = '\0';
			char* data = (char*)"DATA\r\n";
			logx << "SEND TO SERVER " << data << std::endl;
			SSL_write(ssl, data, strlen(data));
			bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << std::endl;
			if (!Contain(std::string(buff), "354"))return;
			counter++;

			std::string Encoding = "iso-8859-2"; // charset: utf-8, utf-16, iso-8859-2, iso-8859-1
			Encoding = "utf-8";

			std::string subject = "Testin Mailer";
			std::string msg = "This is a test Message";
			
			// add html page layout
			std::stringstream msghtml;
			
			msghtml << "<!DOCTYPE html>\n";
			msghtml << "<HTML lang = 'en'>\n";
			msghtml << "<body>\n";
			msghtml << "	<p>Please find attched report(/ s).</p>\n";
			
			// 
			// add atachments
			//std::vector<std::string> files = Explode(", ", attachments);

			std::stringstream attachmentSection;
			std::vector<std::string>files = attachments;
			if (files.size() > 0) {
				for (unsigned int i = 0;i < files.size();i++) {
					std::string path = files.at(i);
					std::string filename = fileBasename(path);
					std::string fc = base64(get_file_contents(path.c_str()));
					std::string extension = GetFileExtension(filename);
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
			
			std::stringstream m;
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
			std::string mimemsg = m.str();
			logx << "Email body being sent: " << mimemsg.data() << std::endl;
			char* mdata = mimemsg.data();
			SSL_write(ssl, mdata, strlen(mdata));
			bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << std::endl;
			logx << "---" << loghash << "---" << std::endl << std::endl;
			counter++;

			// send log
			std::cout << logx.str();
			WriteToLog(logx.str());
			if (!Contain(std::string(buff), "250"))return;

			char* qdata = (char*)"quit\r\n";
			SSL_write(ssl, qdata, strlen(qdata));
			bytes = SSL_read(ssl, buff, sizeof(buff));
			buff[bytes] = 0;
			logx << counter << " [RECEIVED_TLS] " << buff << std::endl;
			if (!Contain(std::string(buff), "221"))return;

			SSL_free(ssl);
			return;
		}

	}
	catch (std::exception& e) {
		logx << "---" << loghash << "---\r\n" << std::endl;
		std::cout << logx.str();
		WriteToError(logx.str());
		return;
	}
}


//int GetExitCodeProcess() {
//
//}

bool ShellExecuteApp(std::string appName, std::string params)
{
	SHELLEXECUTEINFO SEInfo;
	DWORD ExitCode;
	std::string exeFile = appName;
	std::string paramStr = params;
	std::string StartInString;

	// fine the windows handle using https://learn.microsoft.com/en-us/troubleshoot/windows-server/performance/obtain-console-window-handle
	HWND windowHandle;
	char newWindowTitle[1024];
	char oldWindowTitle[1024];

	GetConsoleTitle(oldWindowTitle, sizeof(oldWindowTitle));
	
	printf(oldWindowTitle);
	printf("\n");
	Sleep(5000);
	windowHandle = FindWindow(NULL, oldWindowTitle);

	//std::fill_n(SEInfo, sizeof(SEInfo), NULL);
	SEInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	SEInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	SEInfo.hwnd = windowHandle;
	SEInfo.lpVerb = NULL;
	SEInfo.lpDirectory = NULL;
	SEInfo.lpFile = exeFile.c_str();
	SEInfo.lpParameters = paramStr.c_str();
	SEInfo.nShow = paramStr == "" ? SW_NORMAL : SW_HIDE;
	if(ShellExecuteEx(&SEInfo)){
		do {
			GetExitCodeProcess(SEInfo.hProcess, &ExitCode);
			std::cout << ExitCode << std::endl;
		} while (ExitCode != STILL_ACTIVE);
		return true;
	}

	return false;
}


int main() {
	HenchmanService service;
	CSimpleIni ini;
	ini.SetUnicode();
	
	std::cout << "Export path: " << service.GetExportsPath() << std::endl;
	//service.app_path = "C:\\FPC\\Kaptap.exe";
	std::cout << "Logs path: " << service.GetLogsPath() << std::endl;
	std::vector<std::string> explodedString;
	try {
		std::string s = "Hello World Jack Son";
		explodedString = service.Explode(" ", s, 1);
	}
	catch (const HenchmanServiceException& hse) {
		std::cout << "An Exception in HenchmanService Occured:" << std::endl;
		std::cout << hse.what();
	}
	for(auto i : explodedString) std::cout << i << std::endl;
	service.ProcessExists("HenchmanServices") ? std::cout << "Yes It Does" : std::cout << "No It Does Not";
	std::cout << std::endl;
	char buff[1024];
	int byteLength = GetCurrentDirectory(sizeof(buff), buff);
	std::string currDir = buff;
	currDir.resize(byteLength);
	service.FileInUse(currDir + "\\HenchmanServices.exe") ? std::cout << "Yes It Is" : std::cout << "No It Is Not";
	std::cout << std::endl;
	std::cout << "Reading ini file: " << std::string(currDir + "\\service.ini") << std::endl;
	SI_Error rc = ini.LoadFile(std::string(currDir + "\\service.ini").c_str());
	if (rc < 0) {
		std::cerr << "Failed to Load INI File" << std::endl;
	}
	const char* username;
	const char* password;
	username = ini.GetValue("SMTP", "username", "");
	password = ini.GetValue("SMTP", "password", "");
	if (service.isInternetConnected() && service.setMailLogin(username, password)) {
		//service.ConnectWithSMTP();
	}
	ShellExecuteApp("HenchmanServices.exe", "") ? std::cout << "Successfully excecuted" << std::endl : std::cout << "Failed to excecute" << std::endl;
	explodedString.clear();
	Sleep(5000);
	return 0;
}