// HenchmanServiceLibrary.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "henchmanservicelibrary_export.h"

#include <iostream>
#include <sstream>
#include <filesystem>


#include "ServiceController.h"

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QLibraryInfo>


#include "SimpleIni.h"


#include "DatabaseManager.h"
#include "SQLiteManager2.h"
#include "ServiceHelper.h"
#include "RegistryManager.h"
#include "EventManager.h"
#include "TRAKManager.h"


#include <Windows.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <TlHelp32.h>

#include <crtdbg.h>


#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Wtsapi32.lib")


//std::unique_ptr<ServiceController::CServiceController> svcController = nullptr;
//std::unique_ptr<ServiceController::SService> service;


namespace HenchmanService {
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
	class HENCHMANSERVICELIBRARY_EXPORT CHenchmanService : public QObject
	{

		Q_OBJECT

	private:
		/**
		 * @brief The username for sending emails.
		 *
		 * This variable stores the username used for sending emails using SMTP.
		 */
		tstring mail_username;
		/**
		 * @brief The username for sending emails.
		 *
		 * This variable stores the username used for sending emails using SMTP.
		 */
		tstring mail_password;

		/**
		 * @brief A flag indicating whether to update the service.
		 *
		 * This variable is used to determine whether the service should be updated or not.
		 */
		bool update = FALSE;

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
		tstring app_path;

		/**
		 * @brief A unique pointer to a DatabaseManager object.
		 *
		 * This variable is used to manage the database connection.
		 */
		 //std::unique_ptr<DatabaseManager> dbManager;
		 //DatabaseManager dbManager = nullptr;

		 /**
		  * @brief A unique pointer to a SQLiteManager2 object.
		  *
		  * This variable is used to manage the SQLite database connection.
		  */
		  //std::unique_ptr<SQLiteManager2> sqliteManager;
		SQLiteManager2 sqliteManager;
		DatabaseManager databaseManager;

	private:
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
		bool setMailLogin(const tstring& username, const tstring& password);

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
		 * @brief The default constructor for the HenchmanService class.
		 *
		 * Initializes the application's configuration and sets up the database connection.
		 *
		 */
		CHenchmanService(QObject* parent);

		/**
		 * @brief The destructor for the HenchmanService class.
		 *
		 * Releases any system resources allocated by the application.
		 */
		~CHenchmanService();

		/**
		 * @brief The main function for the HenchmanService application.
		 *
		 * Calls the necessary methods to set up the application's configuration, check the state of the MySQL and Apache services, and send emails using SMTP.
		 *
		 * @return Returns 0 if the application runs successfully, otherwise returns an error code.
		 *
		 * @throws Throws an exception if there is an error running the application.
		 */
		int MainFunction(QCoreApplication* a);
	};

};

DWORD WINAPI SvcWorkerThread(LPVOID lpParam);
DWORD SvcWorkerThread();

void createUniqueServiceController(const ServiceController::SService& service, bool isTesting = false);
ServiceController::CServiceController* getServiceController();
