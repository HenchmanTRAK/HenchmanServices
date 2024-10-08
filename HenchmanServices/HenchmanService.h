// HenchmanServices.h : Include file for standard system include files,
// or project specific include files.

#ifndef HENCHMAN_SERVICE_H
#define HENCHMAN_SERVICE_H
#pragma once


#include "openssl/crypto.h"
#include "openssl/err.h"
#include "openssl/ssl.h"


#include <Ws2tcpip.h>
#include <Windows.h>
#include <WinSock2.h>
#include <netlistmgr.h>
#include <tchar.h>
#include <strsafe.h>
#include <TlHelp32.h>

#include <thread>
#include <future>

#include <QCoreApplication>
#include <QTimer>
#include <QString>
#include <QByteArray>

#include "SimpleIni.h"

#include "HenchmanServiceException.h"
#include "RegistryManager.h"
#include "ServiceHelper.h"
#include "EventManager.h"
#include "TRAKManager.h"
#include "SQLiteManager.h"
#include "DatabaseManager.h"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Ws2_32.lib")

// TODO: Reference additional headers your program requires here.
#define SERVICE_NAME			"HenchmanService"
#define SERVICE_DISPLAY_NAME	"HenchmanTRAK Product Service"
#define SERVICE_DESIRED_ACCESS	SERVICE_ALL_ACCESS
#define SERVICE_TYPE			SERVICE_WIN32_OWN_PROCESS
#define SERVICE_START_TYPE		SERVICE_AUTO_START
#define SERVICE_ERROR_CONTROL	SERVICE_ERROR_NORMAL
#define SERVICE_DEPENDENCIES	""
//#define SERVICE_ACCOUNT			L"NT AUTHORITY\\LocalService"
#define SERVICE_PASSWORD        NULL
#define CRLF "\r\n"

SERVICE_STATUS		  g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE				  g_ServiceStopEvent = INVALID_HANDLE_VALUE;

SC_HANDLE schSCManager;
SC_HANDLE schService;


const char MimeTypes[][2][128] = {
    {"***", "application/octet-stream"},
    {"csv", "text/csv"},
    {"tsv", "text/tab-separated-values"},
    {"tab", "text/tab-separated-values"},
    {"html", "text/html"},
    {"htm", "text/html"},
    {"doc", "application/msword"},
    {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {"ods", "application/x-vnd.oasis.opendocument.spreadsheet"},
    {"odt", "application/vnd.oasis.opendocument.text"},
    {"rtf", "application/rtf"},
    {"sxw", "application/vnd.sun.xml.writer"},
    {"txt", "text/plain"},
    {"xls", "application/vnd.ms-excel"},
    {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {"pdf", "application/pdf"},
    {"ppt", "application/vnd.ms-powerpoint"},
    {"pps", "application/vnd.ms-powerpoint"},
    {"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {"wmf", "image/x-wmf"},
    {"atom", "application/atom+xml"},
    {"xml", "application/xml"},
    {"json", "application/json"},
    {"js", "application/javascript"},
    {"ogg", "application/ogg"},
    {"ps", "application/postscript"},
    {"woff", "application/x-woff"},
    {"xhtml", "application/xhtml+xml"},
    {"xht", "application/xhtml+xml"},
    {"zip", "application/zip"},
    {"gz", "application/x-gzip"},
    {"rar", "application/rar"},
    {"rm", "application/vnd.rn-realmedia"},
    {"rmvb", "application/vnd.rn-realmedia-vbr"},
    {"swf", "application/x-shockwave-flash"},
    {"au", "audio/basic"},
    {"snd", "audio/basic"},
    {"mid", "audio/mid"},
    {"rmi", "audio/mid"},
    {"mp3", "audio/mpeg"},
    {"aif", "audio/x-aiff"},
    {"aifc", "audio/x-aiff"},
    {"aiff", "audio/x-aiff"},
    {"m3u", "audio/x-mpegurl"},
    {"ra", "audio/vnd.rn-realaudio"},
    {"ram", "audio/vnd.rn-realaudio"},
    {"wav", "audio/x-wave"},
    {"wma", "audio/x-ms-wma"},
    {"m4a", "audio/x-m4a"},
    {"bmp", "image/bmp"},
    {"gif", "image/gif"},
    {"jpe", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"jpg", "image/jpeg"},
    {"jfif", "image/jpeg"},
    {"png", "image/png"},
    {"svg", "image/svg+xml"},
    {"tif", "image/tiff"},
    {"tiff", "image/tiff"},
    {"ico", "image/vnd.microsoft.icon"},
    {"css", "text/css"},
    {"bas", "text/plain"},
    {"c", "text/plain"},
    {"h", "text/plain"},
    {"rtx", "text/richtext"},
    {"mp2", "video/mpeg"},
    {"mpa", "video/mpeg"},
    {"mpe", "video/mpeg"},
    {"mpeg", "video/mpeg"},
    {"mpg", "video/mpeg"},
    {"mpv2", "video/mpeg"},
    {"mov", "video/quicktime"},
    {"qt", "video/quicktime"},
    {"lsf", "video/x-la-asf"},
    {"lsx", "video/x-la-asf"},
    {"asf", "video/x-ms-asf"},
    {"asr", "video/x-ms-asf"},
    {"asx", "video/x-ms-asf"},
    {"avi", "video/x-msvideo"},
    {"3gp", "video/3gpp"},
    {"3gpp", "video/3gpp"},
    {"3g2", "video/3gpp2"},
    {"movie", "video/x-sgi-movie"},
    {"mp4", "video/mp4"},
    {"wmv", "video/x-ms-wmv"},
    {"webm", "video/webm"},
    {"m4v", "video/x-m4v"},
    {"flv", "video/x-flv"}
};

const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

CSimpleIniA ini;

/**
* @class HenchmanService
* @brief Main Service for HenchmanTRAK Entry Point.
*
* This class has the role of installing, controlling and managing the HenchmanService.
*
* @details
*
* The `HenchmanService` class has the following members:
* 
* - `static std::string mail_username`: The username for the mail login.
* - `static std::string mail_password`: The password for the mail login.
* - `static SQLite_Manager *SQLiteM`: A pointer to the SQLite_Manager object.
* - `static clock_t tmr1`: A clock object used for timing.
* - `static clock_t tmrkabTRAK`: A clock object used for timing.
* - `static clock_t tmrcribTRAK`: A clock object used for timing.
* - `static clock_t tmrPortaTRAK`: A clock object used for timing.
* - `static SOCKET mailSocket`: The mail socket used for communication.
* - `static SSL_CTX* ctx`: The SSL context used for SSL/TLS communication.
* - `static SSL* ssl`: The SSL object used for SSL/TLS communication.
* - `static struct addrinfo* mailAddrInfo`: The mail address information used for communication.
* - `bool kReport`: A boolean flag indicating whether to report the kabTRAK status.
* - `bool cReport`: A boolean flag indicating whether to report the cribTRAK status.
* - `bool pReport`: A boolean flag indicating whether to report the portaTRAK status.
* - `bool update`: A boolean flag indicating whether to perform an update.
* - `static std::stringstream logx`: A stringstream used for logging.
* - `static std::string app_path`: The path of the application.
*
* The `HenchmanService` class has the following methods:
* 
* - `HenchmanService()`: Constructor for the HenchmanService class. Initializes the service and loads configuration settings from an INI file.
* - `~HenchmanService()`: Destructor for the HenchmanService class. Deletes the SQLiteM and TrakM objects, and clears the logx stringstream.
* - `bool setMailLogin(std::string & username, std::string & password)`: Sets the mail login credentials for the HenchmanService.
* - `std::vector<std::string> Explode(const std::string& Seperator, std::string& s, int& limit)`: Splits a string into substrings based on a specified separator.
* - `std::vector<std::string> Explode(const std::string &Seperator, std::string &s)`: Splits a string into substrings based on a specified separator.
* - `void ConnectWithSMTP()`: Connects to the SMTP server and initiates communication through SMTP.
* - `bool checkForInternetConnection()`: Checks if there is an active internet connection.
* - `bool isInternetConnected()`: Checks if there is an active internet connection.
* - `int MainFunction()`: Main function for the HenchmanService.
*
* The `HenchmanService` class performs the following tasks:
* 
* - Creates a new instance of the TRAKManager.
* - Retrieves the status of the wampmysqld64 service and performs actions based on the status.
* - Retrieves the status of the wampapache64 service and performs actions based on the status.
* - Checks if there is an active internet connection and returns 0 if not.
* - Creates a data module for the TRAKManager.
* - Connects to the remote database.
* - Checks if the TRAK process is running and starts it if not.
*
* @throws HenchmanServiceException if there is an error in setting up the socket, getting the mail address info,
* or connecting to the server.
*/
class HenchmanService {

    std::string mail_username;
    std::string mail_password;

    SQLite_Manager *SQLiteM;
    TRAKManager* TrakM;
    DatabaseManager* dbManager;

	static clock_t tmr1;
	static clock_t tmrkabTRAK;
	static clock_t tmrcribTRAK;
	static clock_t tmrPortaTRAK;

	SOCKET mailSocket;
	SSL_CTX* ctx;
	SSL* ssl;
	struct addrinfo* mailAddrInfo;


    bool kReport;
    bool cReport;
    bool pReport;
    bool update;
	
	/*bool report = false;
	bool update = false;*/

	//void ServiceExecute();
	//void Tmr1Timer();
	//char *GetLogPath();
	
	//void tmrkabTRAKTimer(class TObject Sender);
	//void tmrcribTRAKTimer(class TObject Sender);
	//void ServiceStart(class TService, bool& Started);
	//void ServiceStop(class TService, bool& Started);
	//void ServicePause(class TService, bool& Started);
	//void ServiceCreate();

	void SendEmail( SSL*& , std::vector<std::string>);
private:

public:
    std::stringstream logx;
    std::string app_path;

    /**
    * Constructor for the HenchmanService class. Initializes the service and loads configuration settings from an INI file.
    *
    * @throws None
    */
    HenchmanService();

    /**
    * Destructor for the HenchmanService class.
    * Deletes the SQLiteM and TrakM objects, and clears the logx stringstream.
    *
    * @throws None
    */
    ~HenchmanService();

    /**
    * Sets the mail login credentials for the HenchmanService.
    *
    * @param username - The username for the mail login.
    * @param password - The password for the mail login.
    *
    * @return True if the mail login credentials are successfully set, false otherwise.
    *
    * @throws None
    */
    bool setMailLogin(std::string & username, std::string & password);

    /**
    * Splits a string into substrings based on a specified separator.
    *
    * @param Seperator - The separator used to split the string.
    * @param s - The string to be split.
    * @param limit - The maximum number of substrings to be returned. If set to 0, all substrings will be returned.
    *
    * @return A vector containing the substrings.
    *
    * @throws HenchmanServiceException if an empty string is provided for s or if an invalid integer is provided for limit.
    */
    std::vector<std::string> Explode(const std::string& Seperator, std::string& s, int& limit);
    /**
    * Splits a string into substrings based on a specified separator.
    *
    * @param Seperator - The separator used to split the string.
    * @param s - The string to be split.
    *
    * @return A vector containing the substrings.
    *
    * @throws HenchmanServiceException if an empty string is provided for s or if an invalid integer is provided for limit.
    */
    std::vector<std::string> Explode(const std::string &Seperator, std::string &s);

    /**
    * Connects to the SMTP server and initiates communication through SMTP.
    *
    * @throws HenchmanServiceException if there is an error in setting up the socket, getting the mail address info,
    * or connecting to the server.
    */
	void ConnectWithSMTP();

    /**
    * Checks if there is an active internet connection.
    *
    * @return True if there is an active internet connection, false otherwise.
    *
    * @throws None
    */
    bool checkForInternetConnection();

    /**
    * Checks if there is an active internet connection.
    *
    * @return True if there is an active internet connection, false otherwise.
    *
    * @throws None
    */
    bool isInternetConnected();
	//SC_HANDLE *GetServiceController();

    /**
    * Main function for the HenchmanService.
    *
    * This function performs the following tasks:
    * 
    *  - Creates a new instance of the TRAKManager.
    * 
    *  - Retrieves the status of the wampmysqld64 service and performs actions based on the status.
    * 
    *  - Retrieves the status of the wampapache64 service and performs actions based on the status.
    * 
    *  - Checks if there is an active internet connection and returns 0 if not.
    * 
    *  - Creates a data module for the TRAKManager.
    * 
    *  - Connects to the remote database.
    * 
    *  - Checks if the TRAK process is running and starts it if not.
    *
    * @return 0 if the function fails to confirm the network connection or if the TRAK process fails to start.
    *
    * @throws HenchmanServiceException if there is an error in setting up the socket, getting the mail address info,
    * or connecting to the server.
    */
    int MainFunction();
};

void DoInstallSvc();
void __stdcall DoStartSvc(const char* sService = SERVICE_NAME);
int __stdcall StartTargetSvc(const char* sService);
void __stdcall DoStopSvc(const char* sService = SERVICE_NAME);
bool __stdcall StopDependentServices();
void __stdcall DoDeleteSvc(const char* sService = SERVICE_NAME);
void ReportSvcStatus(
    DWORD dwCurrentState,
    DWORD dwWin32ExitCode,
    DWORD dwWaitHint
);
DWORD GetSvcStatus(const char* sMachine, const char* sService = SERVICE_NAME);
void WINAPI SvcCtrlHandler(DWORD CtrlCode);
void WINAPI SvcMain(int dwArgc, char* lpszArgv[]);
void SvcInit();
DWORD WINAPI SvcWorkerThread(LPVOID lpParam);

#endif