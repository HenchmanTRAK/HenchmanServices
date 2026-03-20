// HenchmanServiceLibrary.h : Include file for standard system include files,
// or project specific include files.

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
#include <locale>
#include <vector>


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
	TaskScheduler();
	~TaskScheduler();

	int removeTask(const std::string& szTaskName, const bool& shouldThrow = true);
	int removeTask(const std::wstring& szTaskName, const bool& shouldThrow = true);
	
	int addNewTask(const LPCSTR& szTaskName, const std::string& strExecutablePath);
	int addNewTask(const LPCWSTR& szTaskName, const std::wstring& strExecutablePath);

private:
	int m_removeTask(const LPCTSTR& szTaskName, const bool& shouldThrow = true);

	int  m_addNewTask(const LPCTSTR& szTaskName, const LPCTSTR& strExecutablePath = TEXT(""));

	int releaseService();
	int releaseTask();
	int releaseFolder();

	HRESULT registerTaskRegInfo();
	HRESULT createTaskTrigger(ITrigger*& pTrigger, TASK_TRIGGER_TYPE2 type);
	HRESULT createDailyTask(IDailyTrigger*& pDailyTrigger);
	HRESULT addRepititionToTask(IDailyTrigger*& pDailyTrigger);
	HRESULT createTaskAction(IAction*& pAction, TASK_ACTION_TYPE type);
	HRESULT addActionDetails(IAction*& pAction, const LPCTSTR& strExecutablePath = TEXT(""));
	HRESULT createTaskPrincipal();
	HRESULT createTaskSettings(ITaskSettings*& pSettings);
	HRESULT createTaskIdleSettings(ITaskSettings*& pSettings);
	HRESULT registerTask(IRegisteredTask* pRegisteredTask);
};
