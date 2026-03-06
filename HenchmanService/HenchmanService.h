// HenchmanServices.h : Include file for standard system include files,
// or project specific include files.

#ifndef HENCHMAN_SERVICE_H
#define HENCHMAN_SERVICE_H
#pragma once


#include <iostream>
#include <sstream>
#include <filesystem>
#include <QtDebug>

#include "SimpleIni.h"

#include "ServiceHelper.h"
#include "RegistryManager.h"
#include "EventManager.h"
#include "ServiceController.h"
#include "HenchmanServiceLibrary.h"

#include <Windows.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <TlHelp32.h>

#include <crtdbg.h>


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