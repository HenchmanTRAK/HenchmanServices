// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the SERVICECONTROLLER_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// SERVICECONTROLLER_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.

#ifndef SERVICE_CONTROLLER_LIBRARY_H
#define SERVICE_CONTROLLER_LIBRARY_H
#pragma once

#ifdef SERVICE_CONTROLLER_LIBRARY_EXPORTS
#define SERVICE_CONTROLLER_LIBRARY_ __declspec(dllexport)
#else
#define SERVICE_CONTROLLER_LIBRARY_ __declspec(dllimport)
#endif

#include <iostream>
#include <strsafe.h>
#include <Windows.h>

#include "ServiceHelper.h"
#include "HenchmanServiceTaskScheduler.h"

namespace ServiceController
{
	/**
	 * @brief Struct to define and organise the required handlers and data for the ServiceController.
	 * 
	 * @param serviceName The name of the service.
	 * @param displayName The display name of the service.
	 * @param localUser The local user of the service.
	 * @param localPass The local password of the service.
	 * @param serviceStatus The status of the service.
	 * @param serviceStatusHandle The status handle of the service.
	 * @param serviceStopEvent The stop event of the service.
	 */
	struct SService
	{
		const TCHAR* serviceName = nullptr;
		const TCHAR* displayName = nullptr;
		const TCHAR* localUser = nullptr;
		const TCHAR* localPass = nullptr;
		std::string servicePath = "";
		SERVICE_STATUS serviceStatus = { 0 };
		SERVICE_STATUS_HANDLE serviceStatusHandle = NULL;
		HANDLE serviceStopEvent = INVALID_HANDLE_VALUE;
	};

	// This class is exported from the dll
	class CServiceController
	{
	private:
		SC_HANDLE schSCManager = nullptr;
		SC_HANDLE schService = nullptr;
		TaskScheduler mTaskScheduler;
		bool pTesting = false;
		bool disableTaskCreation = false;

	public:

		SService mService;
		/*SERVICE_STATUS			 mServiceStatus = { 0 };
		SERVICE_STATUS_HANDLE	 mServiceStatusHandle = NULL;
		HANDLE					 mServiceStopEvent = INVALID_HANDLE_VALUE;*/

	private:
		bool __stdcall StopDependentServices();

	public:
		CServiceController(const SService& serviceDetails, bool testing = false);

		~CServiceController();

		void DoInstallSvc(bool disableTask = false);

		void __stdcall DoStartSvc(const TCHAR* mService = nullptr);
		int __stdcall StartTargetSvc(const TCHAR* sService = nullptr);
		void __stdcall DoStopSvc(const TCHAR* mService = nullptr);
		void __stdcall DoDeleteSvc(const TCHAR* mService = nullptr);
		void ReportSvcStatus(
			DWORD dwCurrentState,
			DWORD dwWin32ExitCode,
			DWORD dwWaitHint
		);
		DWORD __stdcall GetSvcStatus(const TCHAR* mService = nullptr);
		virtual void WINAPI SvcCtrlHandler(DWORD CtrlCode);

		//virtual void WINAPI SvcMain();
		//virtual void SvcInit();
	};

	//DWORD SvcWorkerThread(LPVOID lpParam);
}

//void ReportSvcStatus(
//	SERVICE_STATUS_HANDLE& svcStatusHandle,
//	SERVICE_STATUS& svcStatus
//);
//
//void ReportSvcStatus(
//	DWORD dwCurrentState,
//	DWORD dwWin32ExitCode,
//	DWORD dwWaitHint
//);
#endif