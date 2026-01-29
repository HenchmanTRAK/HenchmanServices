// HenchmanServiceLibrary.h : Include file for standard system include files,
// or project specific include files.

#ifndef HENCHMAN_SERVICE_TASK_SCHEDULER_LIBRARY_H
#define HENCHMAN_SERVICE_TASK_SCHEDULER_LIBRARY_H
#pragma once

//#ifdef HENCHMAN_SERVICE_TASK_SCHEDULER_LIBRARY_EXPORTS
//#define HENCHMAN_SERVICE_TASK_SCHEDULER_LIBRARY_ __declspec(dllexport)
//#else
//#define HENCHMAN_SERVICE_TASK_SCHEDULER_LIBRARY_ __declspec(dllimport)
//#endif

#define _WIN32_DCOM

#include <windows.h>
#include <exception>
#include <iostream>
#include <stdio.h>
#include <comdef.h>
#include <wincred.h>
#include <codecvt>
#include <string>
#include <sstream>


#include <taskschd.h>
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "credui.lib")

class TaskScheduler {

private:
	HRESULT hr = S_OK;
	ITaskService* pService = NULL;
	ITaskFolder* pRootFolder = NULL;
	ITaskDefinition* pTask = NULL;

	bool initialised = false;


public:
	__cdecl TaskScheduler();
	__cdecl ~TaskScheduler();

	int __cdecl removeTask(const std::string& szTaskName, const bool& shouldThrow = true);
	int __cdecl removeTask(const std::wstring& szTaskName, const bool& shouldThrow = true);
	
	int __cdecl addNewTask(const LPCSTR& szTaskName, const std::string& strExecutablePath);
	int __cdecl addNewTask(const LPCWSTR& szTaskName, const std::wstring& strExecutablePath);

private:
	int __cdecl removeTask(const LPCTSTR& szTaskName, const bool& shouldThrow = true);

	int  __cdecl addNewTask(const LPCTSTR& szTaskName, const LPCTSTR& strExecutablePath = TEXT(""));

	int __cdecl releaseService();
	int __cdecl releaseTask();
	int __cdecl releaseFolder();

	HRESULT __cdecl registerTaskRegInfo();
	HRESULT __cdecl createTaskTrigger(ITrigger* pTrigger, TASK_TRIGGER_TYPE2 type);
	HRESULT __cdecl createDailyTask(ITrigger* pTrigger, IDailyTrigger* pDailyTrigger);
	HRESULT __cdecl addRepititionToTask(IDailyTrigger* pDailyTrigger);
	HRESULT __cdecl createTaskAction(IAction* pAction, TASK_ACTION_TYPE type);
	HRESULT __cdecl addActionDetails(IAction* pAction, const LPCTSTR& strExecutablePath = TEXT(""));
	HRESULT __cdecl createTaskPrincipal();
	HRESULT __cdecl createTaskSettings(ITaskSettings* pSettings);
	HRESULT __cdecl createTaskIdleSettings(ITaskSettings* pSettings);
	HRESULT __cdecl registerTask(IRegisteredTask* pRegisteredTask);
};

#endif