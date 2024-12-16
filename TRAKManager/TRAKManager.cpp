


#include "TRAKManager.h"


using namespace std;

//string TRAKManager::appDir = "";
//string TRAKManager::iniFile = "";
//string TRAKManager::appName = "";
//string TRAKManager::appType="";

TRAKManager::TRAKManager(DatabaseManager *dbManager)
{
	appDir = "";
	iniFile = "";
	appName = "";
	appType = "";
	if (dbManager)
		databaseManager = dbManager;

}

TRAKManager::~TRAKManager()
{
	if (databaseManager)
		databaseManager = nullptr;
}

bool TRAKManager::TRAKExists(CSimpleIniA& ini)
{
	string AppDir32 = ini.GetValue("TRAK", "TRAK_DIR", "");
	if (filesystem::exists(AppDir32.data())) {
		appDir = AppDir32+"\\";
		iniFile = ini.GetValue("TRAK", "INI_FILE", "");
		appName = ini.GetValue("TRAK", "EXE_FILE", "");
		appType = ini.GetValue("TRAK", "APP_NAME", "");

		if (appType == "kabTRAK")
			traktype = kabtrak;
		if (appType == "cribTRAK")
			traktype = cribtrak;
		if (appType == "portaTRAK")
			traktype = portatrak;

		ServiceHelper().WriteToLog("Using " + appDir + iniFile);
		return TRUE;
	}
	return FALSE;
}

void TRAKManager::conHenchmanAfterConnect()
{
	ServiceHelper().WriteToLog((string)"Connected to local MYSQL database...");
}

void TRAKManager::conHenchmanAfterDisconnect()
{
	ServiceHelper().WriteToLog((string)"Disconnected from local MYSQL database...");
}

void TRAKManager::conHenchmanConnectionLost()
{
	ServiceHelper().WriteToLog((string)"Lost connection to local MYSQL database...\nretrying...");
}

void TRAKManager::conHenchmanError(exception& e)
{
	stringstream error;
	error << "Local MYSQL database encountered an error: " << e.what() << "\n";
	ServiceHelper().WriteToLog(error.str());
	error.clear();
}

void TRAKManager::conRemoteAfterConnect()
{
	ServiceHelper().WriteToLog((string)"Connected to remote database...");
}

void TRAKManager::conRemoteAfterDisconnect()
{
	ServiceHelper().WriteToLog((string)"Disconnected from remote database...");
}

void TRAKManager::conRemoteConnectionLost()
{
	ServiceHelper().WriteToLog((string)"Lost connection to remote database...\nretrying...");
}

void TRAKManager::conRemoteError(exception& e)
{
	stringstream error;
	error << "Remote database encountered an error: " << e.what() << "\n";
	ServiceHelper().WriteToLog(error.str());
	error.clear();
}

void TRAKManager::saveINIToRegistry()
const {
	CSimpleIniA ini;
	ini.SetUnicode();

	SI_Error rc = ini.LoadFile((appDir + iniFile).data());
	if (rc < 0) {
		throw HenchmanServiceException("Failed to Load INI File: " + appDir + iniFile);
	}
	
	try 
	{
		CSimpleIniA::TNamesDepend sections;
		ini.GetAllSections(sections);
		for (auto const& sec : sections) {
			saveINIToRegistry(sec.pItem);
		}
		sections.clear();
	}
	catch (exception& e)
	{
		ServiceHelper().WriteToError(e.what());
	}
	
}

void TRAKManager::saveINIToRegistry(string section)
const {
	CSimpleIniA ini;
	ini.SetUnicode();

	SI_Error rc = ini.LoadFile((appDir + iniFile).data());
	if (rc < 0) {
		throw HenchmanServiceException("Failed to Load INI File: " + appDir + iniFile);
	}

	ServiceHelper().WriteToLog("Adding " + section + " entries to registry");
	//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + appType + "\\" + section).data());
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\" + appType + "\\" + section).c_str());
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);

	try
	{
		map<string, string> map;
		CSimpleIniA::TNamesDepend keys;
		ini.GetAllKeys(section.data(), keys);
		for (auto const& val : keys)
		{
			string key = val.pItem;
			string value = ini.GetValue(section.data(), val.pItem, "");
			ServiceHelper().removeQuotes(value);
			if (key == "Password" && value != "")
				value = QByteArray(value.data()).toBase64();
			map[key] = value;
			//RegistryManager::GetStrVal(hKey, key.data(), REG_SZ);
			//RegistryManager::SetVal(hKey, key.data(), map[key].data(), REG_SZ);
			if (rtManager.SetVal(key.data(), REG_SZ, (char*)map[key].data(), (map[key].length() + 1)))
				throw HenchmanServiceException("Failed to set Key and key value to registry");

			if (key == "kabID" || key == "portaID" || key == "cribID" || key == "scaleID")
			{
				//RegistryManager::SetVal(hKey, "trakID", key.data(), REG_SZ);
				if (rtManager.SetVal("trakID", REG_SZ, (char*)key.data(), (key.length() + 1)))
					throw HenchmanServiceException("Failed to set trakId and trakId value to registry");
			}
			key.clear();
			value.clear();
		}
		keys.clear();
		map.clear();
	}
	catch (exception& e)
	{
		ServiceHelper().WriteToError(e.what());
	}
	//RegCloseKey(hKey);

}

void TRAKManager::CreateDataModule()
{
	CSimpleIniA ini;
	ini.SetUnicode();

	//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\HenchmanService").c_str());
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);

	//string serviceInstallDir = RegistryManager::GetStrVal(hKey, "INSTALL_DIR", REG_SZ) + "\\service.ini";
	rtManager.GetVal("INSTALL_DIR", REG_SZ, (char*)buffer, size);
	std::string serviceInstallDir(buffer);
	serviceInstallDir.append("\\service.ini");
	//RegCloseKey(hKey);
	try 
	{
		SI_Error rc = ini.LoadFile(serviceInstallDir.data());
		if (rc < 0) {
			throw HenchmanServiceException("Failed to Load INI File: " + serviceInstallDir);
		}
	

		if (!TRAKExists(ini))
			throw HenchmanServiceException("No TRAK application could be found");

		ServiceHelper().WriteToLog(appName +" exists with " +iniFile +" ini file at " +appDir);
	
		LOG << "app dir: " << appDir << iniFile;
		
		saveINIToRegistry();
		
	}
	catch (exception &e)
	{
		ServiceHelper().WriteToError(e.what());
	}

}
		//cout << "Connecting to Local MySQL Database" << endl;

int TRAKManager::exportGeneralTables()
{
	return (
		databaseManager->AddToolsIfNotExists() 
		|| databaseManager->AddUsersIfNotExists() 
		|| databaseManager->AddEmployeesIfNotExists()
		|| databaseManager->AddJobsIfNotExists()
		? 1
		: 0
		);
}

int TRAKManager::UploadCurrentStateToRemote()
{
	if (!databaseManager)
		return 1;
	
	if (exportGeneralTables())
		return 1;

	switch (traktype)
	{
	case kabtrak: {
		return (databaseManager->AddKabsIfNotExists() |
			databaseManager->AddDrawersIfNotExists() |
			databaseManager->AddToolsInDrawersIfNotExists());
	}
	case cribtrak: {
		return 1;
	}
	case portatrak: {
		return (databaseManager->AddItemKitsIfNotExists() | 
			databaseManager->AddKitCategoryIfNotExists() | 
			databaseManager->AddKitLocationIfNotExists());
	}
	default: {
	
	}
	}
	return 1;
}

		//dbManager->deleteLater();
		//dbManager = nullptr;

		//string sqlFile = appDir + "database\\qkabmaster.sql";
		//dbManager->ExecuteTargetSqlScript(appType, sqlFile);
		//sqlFile = "C:\\Users\\Willem\\Documents\\henchmanTRAK Remote Support\\Files\\qkabtrak_sts_003.sql";
		//dbManager->ExecuteTargetSqlScript(appType, sqlFile);