#ifndef TRAK_MANAGER_H
#define TRAK_MANAGER_H
#pragma once

#include <map>

#include "SimpleIni.h"

#include <QByteArray>

#include "RegistryManager.h"
#include "ServiceHelper.h"
#include "DatabaseManager.h"

class TRAKManager {
private:
	/**
	* Checks if the TRAK application exists by reading the TRAK_DIR, INI_FILE,
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
	* Saves the contents of an INI file to the Windows registry under a specific section.
	*
	* @param iniFile - the INI file to save
	* @param section - the section in the registry to save the INI file contents to
	*
	* @throws None
	*/
	void saveINIToRegistry(CSimpleIniA& iniFile, std::string& section) const;

public:
	std::string appType;
	std::string appDir;
	std::string iniFile;
	std::string appName;
	/*TRAKManager();
	~TRAKManager();*/
	void conHenchmanAfterConnect();
	void conHenchmanAfterDisconnect();
	void conHenchmanConnectionLost();
	void conHenchmanError(std::exception& e);
	void conRemoteAfterConnect();
	void conRemoteAfterDisconnect();
	void conRemoteConnectionLost();
	void conRemoteError(std::exception& e);

	/**
	* Creates a data module by loading an INI file and adding its contents to the registry.
	* Then, it connects to a local MySQL database and executes a SQL script.
	*
	* @param dbManager - a pointer to a DatabaseManager object used to connect to the local database
	*
	* @throws exception - if there is an error loading the INI file or executing the SQL script
	*/
	void CreateDataModule(DatabaseManager * dbManager);
};

#endif