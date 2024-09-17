// HenchmanServices.h : Include file for standard system include files,
// or project specific include files.

#ifndef HENCHMAN_SERVICE_H
#define HENCHMAN_SERVICE_H
#pragma once

//#include <QtCore>
//#include <openssl/ssl.h>
//#include <openssl/err.h>
//#include <openssl/crypto.h>
#include <Ws2tcpip.h>
#include <Windows.h>
//#include <iomanip>
//#include <sstream>
//#include <string>
//#include <vector>
//#include <ctime>
//#include <iostream>
#include <WinSock2.h>
//#include <WinUser.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <malloc.h>
//#include <sys/types.h>
//#include <optional>
//#include <fstream>
//#include <algorithm>
//#include <cstdio>
#include <netlistmgr.h>
#include <tchar.h>
#include <strsafe.h>
#include <TlHelp32.h>

#include <QCoreApplication>
#include <QTimer>
#include <QString>

#include "SimpleIni.h"
#include "HenchmanServiceException.h"
#include "ServiceHelper.h"
#include "RegistryManager.h"
#include "SQLiteManager.h"
#include "TRAKManager.h"

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

class HenchmanService {

    static std::string mail_username;
    static std::string mail_password;

    static SQLite_Manager *SQLiteM;

	static clock_t tmr1;
	static clock_t tmrkabTRAK;
	static clock_t tmrcribTRAK;
	static clock_t tmrPortaTRAK;

	static SOCKET mailSocket;
	static SSL_CTX* ctx;
	static SSL* ssl;
	static struct addrinfo* mailAddrInfo;

    bool kReport;
    bool cReport;
    bool pReport;
    bool update;
	
	/*bool report = false;
	bool update = false;*/

	void ServiceExecute();
	void Tmr1Timer();
	char *GetLogPath();
	
	void tmrkabTRAKTimer(class TObject Sender);
	void tmrcribTRAKTimer(class TObject Sender);
	void ServiceStart(class TService, bool& Started);
	void ServiceStop(class TService, bool& Started);
	void ServicePause(class TService, bool& Started);
	void ServiceCreate();
	void SendEmail( SSL*& , std::vector<std::string>);
private:

public:
    HenchmanService();
    ~HenchmanService();
    bool setMailLogin(std::string &, std::string &);
    static std::stringstream logx;
    static std::string app_path;
    std::vector<std::string> Explode(const std::string&, std::string&, int&);
    std::vector<std::string> Explode(const std::string&, std::string&);
	void ConnectWithSMTP();
    bool checkForInternetConnection();
    bool isInternetConnected();
	SC_HANDLE *GetServiceController();
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
void WINAPI SvcMain();
void SvcInit();
DWORD WINAPI SvcWorkerThread(LPVOID lpParam);

#endif