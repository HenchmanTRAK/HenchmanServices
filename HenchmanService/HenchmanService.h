// HenchmanServices.h : Include file for standard system include files,
// or project specific include files.

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


void WINAPI SvcMain();
void WINAPI SvcInit();
