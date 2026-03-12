
#include "HenchmanServiceTaskScheduler.h"


std::string convertWCharToStr(const TCHAR* wstr) {
	using convert_typeX = std::codecvt_utf8<TCHAR>;
	std::wstring_convert<convert_typeX, TCHAR> converterX;
	return converterX.to_bytes(wstr);
}

const TCHAR* convertStr(const std::string& str) {
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;
	const TCHAR* retStr;
#ifdef UNICODE
	retStr = converterX.from_bytes(str).c_str();
#else
	retStr = str.data();
#endif
	return retStr;
}

const TCHAR* convertStr(const std::wstring& str) {
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;
	const TCHAR* retStr;
#ifdef UNICODE
	retStr = str.data();
#else
	retStr = converterX.to_bytes(str).c_str();
#endif
	return retStr;
}

std::exception throwException(const std::string& message, HRESULT hresult = E_UNEXPECTED) {
	_com_error error(hresult);
	char buffer[1025] = "\0";
	sprintf(buffer, "%x", hresult);
	std::string errMessage(message + "(" + buffer + ") " + convertWCharToStr(error.ErrorMessage()));
	return std::exception(errMessage.c_str());
}

int releaseTargetInterface(IDispatch* pInterface)
{
	if (pInterface)
	{
		pInterface->Release();
		pInterface = nullptr;
	}
	return 0;
}

TaskScheduler::TaskScheduler()
{
	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		//throw std::exception();
		throw throwException("Failed to Initialize CoInstance: ", hr);
	}

	initialised = true;

	hr = CoInitializeSecurity(
		NULL,
		-1,
		NULL,
		NULL,
		RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
		RPC_C_IMP_LEVEL_IMPERSONATE,
		NULL,
		0,
		NULL);

	if (FAILED(hr))
	{	
		CoUninitialize();
		//throw std::exception("CoInitializeSecurity failed: " + hr);
		throw throwException("CoInitializeSecurity failed: ", hr);
	}

	hr = CoCreateInstance(CLSID_TaskScheduler,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_ITaskService,
		(void**)&pService);


	if (FAILED(hr))
	{
		//printf("Failed to create an instance of ITaskService: %x", hr);
		CoUninitialize();
		throw throwException("Failed to create an instance of ITaskService: ", hr);
	}

	//  Connect to the task service.
	hr = pService->Connect(variant_t(), variant_t(),
		variant_t(), variant_t());
	if (FAILED(hr))
	{
		//printf("ITaskService::Connect failed: %x", hr);
		releaseService();
		CoUninitialize();
		throw throwException("ITaskService::Connect failed: ", hr);
	}

	//  ------------------------------------------------------
	//  Get the pointer to the root task folder.  This folder will hold the
	//  new task that is registered.
	hr = pService->GetFolder(bstr_t(TEXT("\\")), &pRootFolder);
	if (FAILED(hr))
	{
		//printf("Cannot get Root Folder pointer: %x", hr);
		releaseService();
		CoUninitialize();
		throw throwException("Cannot get Root Folder pointer: ", hr);
	}
};

TaskScheduler::~TaskScheduler()
{	
	releaseService();
	releaseFolder();
	releaseTask();

	if (initialised) {
		initialised = false;
	}

	CoUninitialize();
}

int TaskScheduler::releaseService() {
	/*if (pService != NULL) {
		pService->Release();
		pService = NULL;
	}*/
	//return 0;
	return releaseTargetInterface(pService);
}

int TaskScheduler::releaseFolder() {
	/*if (pRootFolder != NULL) {
		pRootFolder->Release();
		pRootFolder = NULL;
	}*/
	//return 0;
	return releaseTargetInterface(pRootFolder);
}

int TaskScheduler::releaseTask() {
	/*if (pTask != NULL) {
		pTask->Release();
		pTask = NULL;
	}

	return 0;*/
	return releaseTargetInterface(pTask);
}

int TaskScheduler::removeTask(const std::string& szTaskName, const bool& shouldThrow) {
	return m_removeTask(convertStr(szTaskName), shouldThrow);
}
int TaskScheduler::removeTask(const std::wstring& szTaskName, const bool& shouldThrow) {
	return m_removeTask(convertStr(szTaskName), shouldThrow);
}

int TaskScheduler::m_removeTask(const LPCTSTR& szTaskName, const bool& shouldThrow)
{
	// If the same task exists, remove it.
	hr = pRootFolder->DeleteTask(bstr_t(szTaskName), 0);

	if (FAILED(hr) && shouldThrow)
	{
		printf("Failed to Delete Task: %x", hr);
		releaseService();
		releaseFolder();
		CoUninitialize();
		throw std::exception("Failed to Delete Task: " + hr);
		return 1;
	}
	return 0;
}

int TaskScheduler::addNewTask(const LPCSTR& szTaskName, const std::string& strExecutablePath) {
	std::cout << "mbstring" << strExecutablePath << "\n";
	return m_addNewTask(convertStr(szTaskName), convertStr(strExecutablePath));
}

int TaskScheduler::addNewTask(const LPCWSTR& szTaskName, const std::wstring& strExecutablePath) {
	std::wcout << "wide string" << strExecutablePath << "";
	return m_addNewTask(convertStr(szTaskName), convertStr(strExecutablePath));
}

int TaskScheduler::m_addNewTask(const LPCTSTR& szTaskName, const LPCTSTR& strExecutablePath)
{

#ifdef UNICODE
	std::wstring tstrExecutablePath(strExecutablePath);
#else
	std::string tstrExecutablePath(strExecutablePath);
#endif

	if (tstrExecutablePath.empty()) {
		//printf("No executable path is provided");
		releaseService();
		releaseFolder();
		CoUninitialize();
		throw throwException("No executable path is provided");
		return 1;
	}

	this->removeTask(szTaskName, false);

	//  Create the task definition object to create the task.
	hr = pService->NewTask(0, &pTask);

	releaseService();

	if (FAILED(hr))
	{
		//printf("Failed to CoCreate an instance of the TaskService class: %x", hr);
		releaseFolder();
		CoUninitialize();
		throw throwException("Failed to CoCreate an instance of the TaskService class: ", hr);
		return 1;
	}


	hr = registerTaskRegInfo();
	if (FAILED(hr)) {
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot create Registration Info for task: ", hr);
		return 1;
	}

	

	IDailyTrigger* pDailyTrigger = NULL;
	hr = createDailyTask(pDailyTrigger);
	if(FAILED(hr))
	{
		releaseFolder();
		releaseTargetInterface(pDailyTrigger);
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot create Daily Trigger for task: ", hr);
		return 1;
	}
	
	hr = addRepititionToTask(pDailyTrigger);
	if (FAILED(hr))
	{
		releaseFolder();
		releaseTargetInterface(pDailyTrigger);
		releaseTask();
		CoUninitialize();
		throw throwException("Failed to add Repitition to task: ", hr);
		return 1;
	}

	IAction* pAction = NULL;
	hr = createTaskAction(pAction, TASK_ACTION_EXEC);
	if (FAILED(hr)) 
	{
		releaseFolder();
		releaseTargetInterface(pAction);
		releaseTask();
		CoUninitialize();
		throw throwException("Failed to create Action for task: ", hr);
		return 1;
	}

	hr = addActionDetails(pAction, tstrExecutablePath.c_str());
	if (FAILED(hr))
	{
		releaseFolder();
		releaseTargetInterface(pAction);
		releaseTask();
		CoUninitialize();
		throw throwException("Failed to define Action for task: ", hr);
		return 1;
	}

	/*
	//  ------------------------------------------------------
	//  Securely get the user name and password. The task will
	//  be created to run with the credentials from the supplied 
	//  user name and password.
	CREDUI_INFO cui;
	TCHAR pszName[CREDUI_MAX_USERNAME_LENGTH] = TEXT("");
	TCHAR pszPwd[CREDUI_MAX_PASSWORD_LENGTH] = TEXT("");
	BOOL fSave;
	DWORD dwErr;

	cui.cbSize = sizeof(CREDUI_INFO);
	cui.hwndParent = NULL;
	//  Ensure that MessageText and CaptionText identify
	//  what credentials to use and which application requires them.
	cui.pszMessageText = TEXT("Account information for task registration:");
	cui.pszCaptionText = TEXT("Enter Account Information for Task Registration");
	cui.hbmBanner = NULL;
	fSave = FALSE;

	//  Create the UI asking for the credentials.
	dwErr = CredUIPromptForCredentials(
		&cui,                             //  CREDUI_INFO structure
		TEXT(""),                         //  Target for credentials
		NULL,                             //  Reserved
		0,                                //  Reason
		pszName,                          //  User name
		CREDUI_MAX_USERNAME_LENGTH,       //  Max number for user name
		pszPwd,                           //  Password
		CREDUI_MAX_PASSWORD_LENGTH,       //  Max number for password
		&fSave,                           //  State of save check box
		CREDUI_FLAGS_GENERIC_CREDENTIALS |  //  Flags
		CREDUI_FLAGS_ALWAYS_SHOW_UI |
		CREDUI_FLAGS_DO_NOT_PERSIST);

	if (dwErr)
	{
		std::cout << "Did not get credentials." << std::endl;
		releaseFolder();
		releaseTask();
		CoUninitialize();
		return 1;
	}
	*/

	hr = createTaskPrincipal();
	if (FAILED(hr))
	{
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Failed to create Principal for task: ", hr);
		return 1;
	}

	//  ------------------------------------------------------
	//  Create the settings for the task
	ITaskSettings* pSettings = NULL;
	hr = createTaskSettings(pSettings);
	if (FAILED(hr))
	{
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Failed to create Settings for task: ", hr);
		return 1;
	}

	hr = createTaskIdleSettings(pSettings);
	if (FAILED(hr))
	{
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Failed to create Idle Settings for task: ", hr);
		return 1;
	}
	

	//  ------------------------------------------------------
	//  Save the task in the root folder.
	IRegisteredTask* pRegisteredTask = NULL;
	VARIANT varPassword;
	varPassword.vt = VT_EMPTY;
	hr = pRootFolder->RegisterTaskDefinition(
		bstr_t(szTaskName),
		pTask,
		TASK_CREATE_OR_UPDATE,
		//variant_t(bstr_t(pszName)),
		//variant_t(bstr_t(pszPwd)),
		//variant_t(TEXT("Local Service")),
		variant_t(),
		//varPassword,
		variant_t(),
		TASK_LOGON_INTERACTIVE_TOKEN,
		//TASK_LOGON_SERVICE_ACCOUNT,
		variant_t(TEXT("")),
		&pRegisteredTask);

	if (FAILED(hr))
	{
		//printf("\nError saving the Task : %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		//SecureZeroMemory(pszName, sizeof(pszName));
		//SecureZeroMemory(pszPwd, sizeof(pszPwd));
		
		throw throwException("Error saving the Task: ", hr);
		return 1;
	}

	printf("\n Success! Task successfully registered. ");

	//  Clean up.
	releaseFolder();
	releaseTask();
	releaseTargetInterface(pRegisteredTask);
	CoUninitialize();
	//SecureZeroMemory(pszName, sizeof(pszName));
	//SecureZeroMemory(pszPwd, sizeof(pszPwd));
	return 0;


}

HRESULT TaskScheduler::registerTaskRegInfo()
{
	if (!pTask)
		throw std::exception("Task must first be created inorder to register its information");

	//  ------------------------------------------------------
	//  Get the registration info for setting the identification.
	IRegistrationInfo* pRegInfo = NULL;
	hr = pTask->get_RegistrationInfo(&pRegInfo);
	if (FAILED(hr))
	{
		//printf("\nCannot get identification pointer: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot get identification pointer: ", hr);
		return 1;
	}

	hr = pRegInfo->put_Author(bstr_t(TEXT("HenchmanTRAK - Willem Swanepoel")));
	releaseTargetInterface(pRegInfo);
	if (FAILED(hr))
	{
		//printf("\nCannot put identification info: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot put identification info: ", hr);
		return 1;
	}

	return hr;
}

HRESULT TaskScheduler::createTaskTrigger(ITrigger*& pTrigger, TASK_TRIGGER_TYPE2 type)
{
	//  ------------------------------------------------------
	//  Get the trigger collection to insert the time trigger.
	ITriggerCollection* pTriggerCollection = NULL;
	hr = pTask->get_Triggers(&pTriggerCollection);
	if (FAILED(hr))
	{
		//printf("\nCannot get trigger collection: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot get trigger collection: ", hr);
		return 1;
	}

	//  Add the time trigger to the task.
	hr = pTriggerCollection->Create(type, &pTrigger);
	releaseTargetInterface(pTriggerCollection);
	if (FAILED(hr))
	{
		//printf("\nCannot create trigger: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot create trigger: ", hr);
		return 1;
	}

	return hr;
}

HRESULT TaskScheduler::createDailyTask(IDailyTrigger*& pDailyTrigger) {
	
	ITrigger* pTrigger = NULL;
	hr = createTaskTrigger(pTrigger, TASK_TRIGGER_DAILY);
	if (FAILED(hr))
	{
		releaseFolder();
		releaseTargetInterface(pTrigger);
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot create Trigger for task: ", hr);
		return 1;
	}

	hr = pTrigger->QueryInterface(
		IID_IDailyTrigger, (void**)&pDailyTrigger);
	releaseTargetInterface(pTrigger);
	if (FAILED(hr))
	{
		//printf("\nQueryInterface call failed for ITimeTrigger: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("QueryInterface call failed for ITimeTrigger: ", hr);
		return 1;
	}

	hr = pDailyTrigger->put_Id(bstr_t(TEXT("HenchmanServceRestartTrigger")));
	if (FAILED(hr))
		printf("\nCannot put trigger ID: %x", hr);

	/*hr = pTimeTrigger->put_EndBoundary(bstr_t("2015-05-02T08:00:00"));
	if (FAILED(hr))
		printf("\nCannot put end boundary on trigger: %x", hr);*/

		//  Set the task to start at a certain time. The time 
		//  format should be YYYY-MM-DDTHH:MM:SS(+-)(timezone).
		//  For example, the start boundary below
		//  is January 1st 2026 at 00:00
	hr = pDailyTrigger->put_StartBoundary(bstr_t(TEXT("2026-01-01T12:00:00")));
	if (FAILED(hr))
		printf("\nCannot put start boundary: %x", hr);

	//  Define the interval for the daily trigger. An interval of 2 produces an
	//  every other day schedule
	hr = pDailyTrigger->put_DaysInterval((short)1);
	if (FAILED(hr))
	{
		//printf("\nCannot put days interval: %x", hr);
		releaseFolder();
		releaseTargetInterface(pDailyTrigger);
		releaseTargetInterface(pTrigger);
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot put days interval: ", hr);
		return 1;
	}

	return hr;
}

HRESULT TaskScheduler::addRepititionToTask(IDailyTrigger*& pDailyTrigger)
{
	// Add a repetition to the trigger so that it repeats
	// five times.
	IRepetitionPattern* pRepetitionPattern = NULL;
	hr = pDailyTrigger->get_Repetition(&pRepetitionPattern);
	releaseTargetInterface(pDailyTrigger);
	if (FAILED(hr))
	{
		//printf("\nCannot get repetition pattern: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot get repetition pattern: ", hr);
		return 1;
	}

	hr = pRepetitionPattern->put_Duration(bstr_t(TEXT("P1D")));
	if (FAILED(hr))
		printf("\nCannot put repetition duration: %x", hr);

	hr = pRepetitionPattern->put_Interval(bstr_t(TEXT("PT1H")));
	releaseTargetInterface(pRepetitionPattern);
	if (FAILED(hr))
	{
		//printf("\nCannot put repetition interval: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot put repetition interval: ", hr);
		return 1;
	}

	return hr;
}

HRESULT TaskScheduler::createTaskAction(IAction*& pAction, TASK_ACTION_TYPE type)
{
	//  ------------------------------------------------------
	//  Add an action to the task. This task will execute the target exe.     
	IActionCollection* pActionCollection = NULL;

	//  Get the task action collection pointer.
	hr = pTask->get_Actions(&pActionCollection);
	if (FAILED(hr))
	{
		//printf("\nCannot get Task collection pointer: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot get Task collection pointer: ", hr);
		return 1;
	}

	//  Create the action, specifying that it is an executable action.
	hr = pActionCollection->Create(type, &pAction);
	releaseTargetInterface(pActionCollection);
	if (FAILED(hr))
	{
		//printf("\nCannot create the action: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot create the action: ", hr);
		return 1;
	}

	return hr;
}

HRESULT TaskScheduler::addActionDetails(IAction*& pAction, const LPCTSTR& strExecutablePath)
{
	IExecAction* pExecAction = NULL;
	//  QI for the executable task pointer.
	hr = pAction->QueryInterface(
		IID_IExecAction, (void**)&pExecAction);
	releaseTargetInterface(pAction);
	if (FAILED(hr))
	{
		//printf("\nQueryInterface call failed for IExecAction: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("QueryInterface call failed for IExecAction: ", hr);
		return 1;
	}

	hr = pExecAction->put_Arguments(bstr_t(TEXT("--start")));
	if (FAILED(hr))
		printf("\nCannot put action arguments: %x", hr);

	//  Set the path of the executable to notepad.exe.
	hr = pExecAction->put_Path(bstr_t(strExecutablePath));
	releaseTargetInterface(pExecAction);
	if (FAILED(hr))
	{
		//printf("\nCannot put action path: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot put action path: ", hr);
		return 1;
	}

	return hr;
}

HRESULT TaskScheduler::createTaskPrincipal()
{
	//  ------------------------------------------------------
	//  Create the principal for the task - these credentials
	//  are overwritten with the credentials passed to RegisterTaskDefinition
	IPrincipal* pPrincipal = NULL;
	hr = pTask->get_Principal(&pPrincipal);
	if (FAILED(hr))
	{
		//printf("\nCannot get principal pointer: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot get principal pointer: ", hr);
		return 1;
	}

	/*hr = pPrincipal->put_Id(bstr_t(TEXT("Administrator")));
	if (FAILED(hr))
		printf("\nCannot put the principal ID: %x", hr);*/

	hr = pPrincipal->put_UserId(bstr_t(TEXT("KioskMode")));
	if (FAILED(hr))
		printf("\nCannot put the principal ID: %x", hr);

	hr = pPrincipal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
	if (FAILED(hr))
		printf("\nCannot put principal run level to heightest: %x", hr);

	//  Set up principal logon type to interactive logon
	hr = pPrincipal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
	//hr = pPrincipal->put_LogonType(TASK_LOGON_SERVICE_ACCOUNT);
	releaseTargetInterface(pPrincipal);
	if (FAILED(hr))
	{
		//printf("\nCannot put principal info: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot put principal info: ", hr);
		return 1;
	}

	return hr;
}

HRESULT TaskScheduler::createTaskSettings(ITaskSettings*& pSettings)
{
	
	hr = pTask->get_Settings(&pSettings);
	if (FAILED(hr))
	{
		//printf("\nCannot get settings pointer: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot get settings pointer: ", hr);
		return 1;
	}

	//  Set setting values for the task.  
	hr = pSettings->put_StartWhenAvailable(VARIANT_TRUE);
	if (FAILED(hr))
		printf("\nCannot put start when available to true: %x", hr);

	hr = pSettings->put_AllowDemandStart(VARIANT_TRUE);
	if (FAILED(hr))
		printf("\nCannot put allow demand start to true: %x", hr);

	hr = pSettings->put_RunOnlyIfNetworkAvailable(VARIANT_TRUE);
	if (FAILED(hr))
		printf("\nCannot put run only if network available to true: %x", hr);

	hr = pSettings->put_ExecutionTimeLimit(bstr_t(TEXT("PT1M")));
	if (FAILED(hr))
		printf("\nCannot put execution time limit to 1 minute: %x", hr);

	if (FAILED(hr))
	{
		//printf("\nCannot put setting information: %x", hr);
		releaseFolder();
		releaseTargetInterface(pSettings);
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot put setting information: ", hr);
		return 1;
	}

	return hr;
}

HRESULT TaskScheduler::createTaskIdleSettings(ITaskSettings*& pSettings)
{
	// Set the idle settings for the task.
	IIdleSettings* pIdleSettings = NULL;
	hr = pSettings->get_IdleSettings(&pIdleSettings);
	releaseTargetInterface(pSettings);
	if (FAILED(hr))
	{
		//printf("\nCannot get idle setting information: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot get idle setting information: ", hr);
		return 1;
	}

	hr = pIdleSettings->put_WaitTimeout(bstr_t(TEXT("PT5M")));
	releaseTargetInterface(pIdleSettings);
	if (FAILED(hr))
	{
		//printf("\nCannot put idle setting information: %x", hr);
		releaseFolder();
		releaseTask();
		CoUninitialize();
		throw throwException("Cannot put idle setting information: ", hr);
		return 1;
	}

	return hr;
}

