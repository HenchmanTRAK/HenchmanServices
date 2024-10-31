// HenchmanServices.h : Include file for standard system include files,
// or project specific include files.

#ifndef HENCHMAN_SERVICE_H
#define HENCHMAN_SERVICE_H
#pragma once


//#include "openssl/crypto.h"
//#include "openssl/err.h"
//#include "openssl/ssl.h"


//#include <netlistmgr.h>
//#include <strsafe.h>
//#include <tchar.h>
#include <Windows.h>
#include <TlHelp32.h>
//#include <WinSock2.h>
//#include <Ws2tcpip.h>

//#include <future>
//#include <thread>

#include <QObject>
//#include <QByteArray>
//#include <QCoreApplication>
//#include <QString>
//#include <QTimer>
//#include <QTcpSocket>

#include "SimpleIni.h"

#include "HenchmanServiceException.h"
#include "EventManager.h"
#include "RegistryManager.h"
#include "DatabaseManager.h"
#include "ServiceHelper.h"
#include "SQLiteManager2.h"
#include "SQLiteManager.h"
#include "TRAKManager.h"


#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Ws2_32.lib")

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
#define CRLF "\r\n"

SERVICE_STATUS			 g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE	 g_StatusHandle = NULL;
HANDLE					 g_ServiceStopEvent = INVALID_HANDLE_VALUE;

SC_HANDLE schSCManager;
SC_HANDLE schService;

/**
 * @brief The main class for the HenchmanService application.
 *
 * This class represents the main application for the HenchmanService.
 * It contains the necessary data members and member functions to manage the application's functionality,
 * including database management, email sending, and system monitoring.
 *
 * The class provides methods for setting up the application's configuration,
 * checking the state of the MySQL and Apache services,
 * and sending emails using SMTP.
 * 
 * @author Willem Swanepoel
 * @version 1.0
 *
 * @details
 * The class has the following data members:
 * - `std::string mail_username`: The username for sending emails.
 * - `std::string mail_password`: The password for sending emails.
 * - `bool update`: A flag indicating whether to update the service.
 *
 * The class has the following member functions:
 * - `HenchmanService()`: The default constructor for the HenchmanService class.
 * - `~HenchmanService()`: The destructor for the HenchmanService class.
 * - `bool SetRequiredParameters()`: Sets up the application's configuration.
 * - `void checkStateOfMySQL()`: Checks the state of the MySQL service.
 * - `void checkStateOfApache()`: Checks the state of the Apache service.
 * - `int MainFunction()`: The main function for the HenchmanService application.
 *
 * @throws HenchmanServiceException if there is an error in setting up the socket, getting the mail address info, or connecting to the server.
 */
class HenchmanService: public QObject 
{

	Q_OBJECT

private:
	/**
	 * @brief The username for sending emails.
	 *
	 * This variable stores the username used for sending emails using SMTP.
	 */
	std::string mail_username = "";
	/**
	 * @brief The username for sending emails.
	 *
	 * This variable stores the username used for sending emails using SMTP.
	 */
	std::string mail_password = "";

	/**
	 * @brief A flag indicating whether to update the service.
	 *
	 * This variable is used to determine whether the service should be updated or not.
	 */
	bool update = FALSE;

	/**
	 * @brief Sets the email login credentials for the HenchmanService application.
	 *
	 * This function sets the username and password for sending emails using SMTP.
	 *
	 * @param username The username for sending emails.
	 * @param password The password for sending emails.
	 *
	 * @return Returns true if the email login credentials are set successfully, otherwise returns false.
	 *
	 * @throws Throws an exception if the username or password is invalid.
	 */
	bool setMailLogin(std::string& username, std::string& password);

	/**
	 * @brief Checks the state of the MySQL service.
	 *
	 * Uses the MySQL API to check the state of the MySQL service and logs any errors.
	 *
	 * @return Returns 0 if the MySQL service is running, otherwise returns an error code.
	 *
	 * @throws Throws an exception if there is an error checking the MySQL service state.
	 */
	void checkStateOfMySQL();

	/**
	 * @brief Checks the state of the Apache service.
	 *
	 * Uses the Apache API to check the state of the Apache service and logs any errors.
	 *
	 * @return Returns 0 if the Apache service is running, otherwise returns an error code.
	 *
	 * @throws Throws an exception if there is an error checking the Apache service state.
	 */
	void checkStateOfApache();

	/**
	 * @brief Sets up the application's configuration.
	 *
	 * Reads the configuration from a file and sets up the necessary data members.
	 *
	 * @return Returns 0 if the configuration is set up successfully, otherwise returns an error code.
	 *
	 * @throws Throws an exception if there is an error reading the configuration file.
	 */
	int SetRequiredParameters();



public:

	/**
	 * @brief A string stream for logging purposes.
	 *
	 * This variable is used to store log messages.
	 */
	std::stringstream logx;

	/**
	 * @brief The path to the application.
	 *
	 * This variable stores the path to the application.
	 */
	std::string app_path = "";
	
	/**
	 * @brief A unique pointer to a DatabaseManager object.
	 *
	 * This variable is used to manage the database connection.
	 */
	std::unique_ptr<DatabaseManager> dbManager;

	/**
	 * @brief A unique pointer to a SQLiteManager2 object.
	 *
	 * This variable is used to manage the SQLite database connection.
	 */
	std::unique_ptr<SQLiteManager2> sqliteManager;

	/**
	 * @brief The default constructor for the HenchmanService class.
	 *
	 * Initializes the application's configuration and sets up the database connection.
	 * 
	 * @param parent An pointer to a Qt core application that allows the application to communicate with other Qt components.
	 * 
	 */
	HenchmanService(QObject* parent = nullptr);

	/**
	 * @brief The destructor for the HenchmanService class.
	 *
	 * Releases any system resources allocated by the application.
	 */
	~HenchmanService();

	/**
	 * @brief Splits a string into a vector of substrings based on a specified separator.
	 *
	 * This function splits the input string `s` into a vector of substrings using the specified separator `Seperator`.
	 * The resulting substrings are stored in a vector and returned.
	 *
	 * @param Seperator The separator used to split the input string.
	 * @param s The input string to be split.
	 * @param limit The maximum number of substrings to be returned. If set to -1, all substrings are returned.
	 *
	 * @return A vector containing the substrings obtained by splitting the input string.
	 *
	 * @throws Throws an exception if the input string or separator is empty.
	 */
	std::vector<std::string> Explode(const std::string& Seperator, std::string& s, int limit = -1);

	/**
	 * @brief The main function for the HenchmanService application.
	 *
	 * Calls the necessary methods to set up the application's configuration, check the state of the MySQL and Apache services, and send emails using SMTP.
	 *
	 * @return Returns 0 if the application runs successfully, otherwise returns an error code.
	 *
	 * @throws Throws an exception if there is an error running the application.
	 */
	int MainFunction();
};

void DoInstallSvc();
void __stdcall DoStartSvc(const char* sService = SERVICE_NAME);
int __stdcall StartTargetSvc(const char* sService);
void __stdcall DoStopSvc(const char* sService = SERVICE_NAME);
bool __stdcall StopDependentServices();
void __stdcall DoDeleteSvc(const char* sService = SERVICE_NAME);
void ReportSvcStatus(
	DWORD dwCurrentState,
	DWORD dwWin32ExitCode,
	DWORD dwWaitHint
);
DWORD GetSvcStatus(const char* sService = SERVICE_NAME);
void WINAPI SvcCtrlHandler(DWORD CtrlCode);
void WINAPI SvcMain(int dwArgc, char* lpszArgv[]);
void SvcInit();
DWORD WINAPI SvcWorkerThread(LPVOID lpParam);

#endif