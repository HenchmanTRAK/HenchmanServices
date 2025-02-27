// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the SERVICECONTROLLER_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// SERVICECONTROLLER_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#pragma once

#include <iostream>
#include <strsafe.h>
#include <Windows.h>

namespace ServiceController
{

	struct SService
	{
		const TCHAR* serviceName = nullptr;
		const TCHAR* displayName = nullptr;
		const TCHAR* localUser = nullptr;
		const TCHAR* localPass = nullptr;
	};

	// This class is exported from the dll
	class CServiceController
	{
	private:
		SC_HANDLE schSCManager = nullptr;
		SC_HANDLE schService = nullptr;

		SService mService;

	public:
		SERVICE_STATUS			 mServiceStatus = { 0 };
		SERVICE_STATUS_HANDLE	 mServiceStatusHandle = NULL;
		HANDLE					 mServiceStopEvent = INVALID_HANDLE_VALUE;

	private:
		bool __stdcall StopDependentServices();

	public:
		CServiceController(const SService& serviceDetails);

		~CServiceController();

		void DoInstallSvc();

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
		void WINAPI SvcCtrlHandler(DWORD CtrlCode);
	};
}