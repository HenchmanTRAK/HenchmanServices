// HenchmanServices.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <Ws2tcpip.h>
#include <windows.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <ctime>
#include <iostream>
#include <WinSock2.h>
#include <WinUser.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/types.h>
#include <optional>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstdio>

#include <TlHelp32.h>

#include "SimpleIni.h"


#include "HenchmanServiceException.h"

#pragma comment(lib, "Ws2_32.lib")

// TODO: Reference additional headers your program requires here.
#define SERVICE_NAME			L"HenchmanService"
#define SERVICE_DISPLAY_NAME	L"HenchmanTRAK Product Service"
#define SERVICE_DESIRED_ACCESS	SERVICE_ALL_ACCESS
#define SERVICE_TYPE			SERVICE_WIN32_OWN_PROCESS
#define SERVICE_START_TYPE		SERVICE_AUTO_START
#define SERVICE_ERROR_CONTROL	SERVICE_ERROR_NORMAL
#define SERVICE_DEPENDENCIES	L""
#define SERVICE_ACCOUNT			L"NT AUTHORITY\\LocalService"
#define SERVICE_PASSWORD        NULL
#define CRLF "\r\n"

const char MimeTypes[][2][128] =
{
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


class HenchmanService {
    static std::string mail_username;
    static std::string mail_password;

	static SC_HANDLE schSCManager;
	static SC_HANDLE schService;
	static clock_t tmr1;
	static clock_t tmrkabTRAK;
	static clock_t tmrcribTRAK;
	static clock_t tmrPortaTRAK;

	static SOCKET mailSocket;
	static SSL_CTX* ctx;
	static SSL* ssl;
	static struct addrinfo* mailAddrInfo;
	
	bool report;
	bool update;

	static std::vector<std::string> _empty_argument;

	void ServiceExecute();
	void Tmr1Timer();
	char *GetLogPath();
	
	void WriteLog(std::string&);
	void tmrkabTRAKTimer(class TObject Sender);
	void tmrcribTRAKTimer(class TObject Sender);
	void ServiceStart(class TService, bool& Started);
	void ServiceStop(class TService, bool& Started);
	void ServicePause(class TService, bool& Started);
	void ServiceCreate();
	void SendEmail( SSL*& , std::vector<std::string> & = _empty_argument);
	bool Contain(std::string, std::string);
	std::string ShowCerts(SSL* ssl);
	void sslError(SSL*, int, std::string, std::stringstream&);
	SSL_CTX* InitCTX(void);

private:
	
public:
    bool setMailLogin(std::string, std::string);
    static std::stringstream logx;
    static std::string app_path;
    void WriteToLog(std::string);
    void WriteToError(std::string);
    std::string GetExportsPath();
    std::string GetLogsPath();
    bool isInternetConnected();
    bool FileInUse(std::string);
    bool ProcessExists(std::string);
	std::vector<std::string> Explode(const std::string&, std::string&, int = 0);
	std::optional<SSL*> ConnectWithSMTP();
	SC_HANDLE *GetServiceController();
};