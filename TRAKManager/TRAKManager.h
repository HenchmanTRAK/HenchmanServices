#ifndef TRAK_MANAGER_H
#define TRAK_MANAGER_H
#pragma once

#include <map>

#include "SimpleIni.h"

#include "ServiceHelper.h"
#include "RegistryManager.h"

using namespace std;

class TRAKManager {
private:
	static string appDir;
	static string iniFile;
	static string appName;
	static string appType;

	bool kabTRAKExists();
	bool portaTRAKExists();
	bool cribTRAKExists();

public:
	/*TRAKManager();
	~TRAKManager();*/
	void conHechmanAfterConnect();
	void conHechmanAfterDisconnect();
	void conHenchmanConnectionLost();
	void conHenchmanError(exception& e);
	void conRemoteAfterConnect();
	void conRemoteAfterDisconnect();
	void conRemoteConnectionLost();
	void conRemoteError(exception& e);
	void CreateDataModule();
};

#endif