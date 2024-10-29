#ifndef TRAK_MANAGER_H
#define TRAK_MANAGER_H
#pragma once

#include <iostream>
//#include <map>

#include <SimpleIni.h>

//#include <QByteArray>

#include "HenchmanServiceException.h"
//#include "RegistryManager.h"
#include "ServiceHelper.h"
#include "DatabaseManager.h"

enum Trak_Type {
	unknown = 0,
	kabtrak = 1,
	cribtrak = 2,
	portatrak = 3
};

/**
 * @class TRAKManager
 *
 * @brief The TRAKManager class is responsible for managing the TRAK application and its associated functionality.
 *
 * This class provides methods for checking if the TRAK application exists, saving INI file contents to the Windows registry,
 * and connecting to a local MySQL database.
 *
 * @author Willem Swanepoel
 * @version 1.0
 */
class TRAKManager {
private:
	DatabaseManager *databaseManager = nullptr;

	Trak_Type traktype = unknown;

	/**
	* @brief Checks if the TRAK application exists by reading the TRAK_DIR, INI_FILE,
	* EXE_FILE, and APP_NAME values from the given CSimpleIniA object. If the
	* TRAK_DIR exists, it sets the appDir, iniFile, appName, and appType
	* variables. It then writes a log message indicating the appDir and iniFile
	* being used. Returns true if the TRAK_DIR exists, false otherwise.
	*
	* @param ini - The CSimpleIniA object to read values from.
	*
	* @return True if the TRAK_DIR exists, false otherwise.
	*
	* @throws None.
	*/
	bool TRAKExists(CSimpleIniA& ini);
	
	/*bool kabTRAKExists();
	bool portaTRAKExists();
	bool cribTRAKExists();*/

	/**
	* @brief Saves the contents of an INI file to the Windows registry under a specific section.
	*
	* @param iniFile - the INI file to save
	* @param section - the section in the registry to save the INI file contents to
	*
	* @throws None
	*/
	void saveINIToRegistry() const;

	void conHenchmanAfterConnect();
	void conHenchmanAfterDisconnect();
	void conHenchmanConnectionLost();
	void conHenchmanError(std::exception& e);
	void conRemoteAfterConnect();
	void conRemoteAfterDisconnect();
	void conRemoteConnectionLost();
	void conRemoteError(std::exception& e);

public:
	/**
	* @brief The type of the TRAK application.
	*/
	std::string appType;

	/**
	* @brief The directory of the TRAK application.
	*/
	std::string appDir;

	/**
	* @brief The name of the INI file of the TRAK application.
	*/
	std::string iniFile;

	/**
	* @brief The name of the TRAK application.
	*/
	std::string appName;

	TRAKManager(DatabaseManager *dbManager = nullptr);
	~TRAKManager();

	/**
	 * @brief Creates a data module by loading an INI file and adding its contents to the registry.
	 *
	 * This function loads an INI file from the Windows registry and checks if the TRAK application exists.
	 * If the TRAK application exists, it loads the INI file and adds its contents to the registry under specific sections.
	 *
	 * @throws HenchmanServiceException - if there is an error loading the INI file or executing the SQL script
	 *
	 */
	void CreateDataModule();

	int UploadCurrentStateToRemote();
};

#endif