#ifndef TRAK_MANAGER_H
#define TRAK_MANAGER_H
#pragma once

#include <map>

#include "SimpleIni.h"

#include "RegistryManager.h"
#include "ServiceHelper.h"
#include "DatabaseManager.h"

class TRAKManager {
private:
	bool TRAKExists(CSimpleIniA& ini);
	/*bool kabTRAKExists();
	bool portaTRAKExists();
	bool cribTRAKExists();*/
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
	void CreateDataModule();
};

#endif