// HenchmanServices.h : Include file for standard system include files,
// or project specific include files.

#ifndef HENCHMAN_SERVICE_H
#define HENCHMAN_SERVICE_H
#pragma once


//#include "openssl/crypto.h"
//#include "openssl/err.h"
//#include "openssl/ssl.h"

#include <iostream>
#include <sstream>
#include <filesystem>
#include <QtDebug>

//#include <netlistmgr.h>

//#include <tchar.h>

//#include <WinSock2.h>
//#include <Ws2tcpip.h>

//#include <future>
//#include <thread>

//#include <QCoreApplication>
//#include <QObject>
//#include <QString>
//#include <QLibraryInfo>
//#include <QByteArray>
//#include <QString>
//#include <QTimer>
//#include <QTcpSocket>

#include "SimpleIni.h"


//#include "DatabaseManager.h"
//#include "SQLiteManager2.h"
#include "ServiceHelper.h"
#include "RegistryManager.h"
#include "EventManager.h"
//#include "TRAKManager.h"
#include "ServiceController.h"
#include "HenchmanServiceLibrary.h"

//#include "ServiceException.h"

#include <Windows.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <TlHelp32.h>


#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Wtsapi32.lib")

#ifdef _DEBUG
	#define DEBUG 1
#endif // _DEBUG

#ifndef DEBUG
	#define QT_NO_DEBUG_OUTPUT
#endif

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
#define CRLF L"\r\n"

#ifdef UNICODE
#define tstring std::wstring
//std::wstring installDir(buffer);
#else
#define tstring std::string
//std::string installDir(buffer);
#endif


void WINAPI SvcMain();
void SvcInit();

#endif