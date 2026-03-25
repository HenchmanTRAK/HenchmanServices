// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the EVENTMANAGER_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// EVENTMANAGER_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#pragma once

#include <iostream>
#include <strsafe.h>
#include <string>
#include <vector>


//#include "event_messages.h"
#include "event_log.h"

#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "advapi32.lib")

//#define _MBCS


namespace EventManager {

	// This class is exported from the dll
	class CEventManager {
	private:
		/**
		 * @var eventSource
		 *
		 * @brief The source of the event.
		 */
		LPCTSTR eventSource;

		/**
		 * @var eventSource
		 *
		 * @brief The source of the event.
		 */
		HANDLE hEventSource;

		/**
		 * @var lpMsgBuf
		 *
		 * @brief The message buffer for the event.
		 */
		 //LPVOID lpMsgBuf = nullptr;

		 /**
		  * @var lpDisplayBuf
		  *
		  * @brief The display buffer for the event.
		  */
		  //LPVOID lpDisplayBuf = nullptr;
	public:
		CEventManager(const LPCTSTR& source);
		CEventManager();
		~CEventManager();

		// TODO: add your methods here.
		/**
		 * @brief Reports a custom event.
		 *
		 * This member function reports a custom event with the specified function, message, and type.
		 *
		 * @param function The function that triggered the event.
		 * @param msg The message associated with the event.
		 * @param type The type of the event.
		 */
		void ReportCustomEvent(const LPCTSTR& function, const LPCTSTR& msg, int type = 3, DWORD errorCode = 0);
		//static void Init(const LPCTSTR& eventSource);

	private:
		/**
		 * @brief Gets the event message for a specific function and message.
		 *
		 * This member function gets the event message for a specific function and message.
		 *
		 * @param lpszFunction The function that triggered the event.
		 * @param msg The message associated with the event.
		 *
		 * @return The event message.
		 */
		DWORD EventMessage(const LPCTSTR& lpszFunction, const LPCTSTR& lpszMsg, const DWORD& errorCode, LPCTSTR buffer, const DWORD& bufferSize) const;
		DWORD EventMessage(const LPCTSTR& lpszFunction, const LPCTSTR& lpszMsg, const DWORD& errorCode, std::vector<TCHAR>* buffer) const;
	};

}
//extern EVENTMANAGER_API int nEventManager;
//
//EVENTMANAGER_API int fnEventManager(void);
