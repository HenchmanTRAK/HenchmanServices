#ifndef TRAK_MANAGER_H
#define TRAK_MANAGER_H
#pragma once

#include <map>

#include "SimpleIni.h"

#include "ServiceHelper.h"
#include "RegistryManager.h"
#include "DatabaseManager.h"

class TRAKManager {
private:
	static std::string appDir;
	static std::string iniFile;
	static std::string appName;
	static std::string appType;

	bool kabTRAKExists();
	bool portaTRAKExists();
	bool cribTRAKExists();
	void saveINIToRegistry(CSimpleIniA& iniFile, std::string& section);

public:
	/*TRAKManager();
	~TRAKManager();*/
	void conHechmanAfterConnect();
	void conHechmanAfterDisconnect();
	void conHenchmanConnectionLost();
	void conHenchmanError(std::exception& e);
	void conRemoteAfterConnect();
	void conRemoteAfterDisconnect();
	void conRemoteConnectionLost();
	void conRemoteError(std::exception& e);
	void CreateDataModule();
};

#endif