


#include "TRAKManager.h"


using namespace std;

//string TRAKManager::appDir = "";
//string TRAKManager::iniFile = "";
//string TRAKManager::appName = "";
//string TRAKManager::appType="";

//TRAKManager::TRAKManager()
//{
//	appDir = "";
//	iniFile = "";
//	appName = "";
//
//}

bool TRAKManager::TRAKExists(CSimpleIniA& ini)
{
	string AppDir32 = ini.GetValue("TRAK", "TRAK_DIR", "");
	if (filesystem::exists(AppDir32.data())) {
		appDir = AppDir32+"\\";
		iniFile = ini.GetValue("TRAK", "INI_FILE", "");
		appName = ini.GetValue("TRAK", "EXE_FILE", "");
		appType = ini.GetValue("TRAK", "APP_NAME", "");
		ServiceHelper::WriteToLog("Using " + appDir + iniFile);
		return TRUE;
	}
	return FALSE;
}

void TRAKManager::conHenchmanAfterConnect()
{
	ServiceHelper::WriteToLog((string)"Connected to local MYSQL database...");
}

void TRAKManager::conHenchmanAfterDisconnect()
{
	ServiceHelper::WriteToLog((string)"Disconnected from local MYSQL database...");
}

void TRAKManager::conHenchmanConnectionLost()
{
	ServiceHelper::WriteToLog((string)"Lost connection to local MYSQL database...\nretrying...");
}

void TRAKManager::conHenchmanError(exception& e)
{
	stringstream error;
	error << "Local MYSQL database encountered an error: " << e.what() << "\n";
	ServiceHelper::WriteToLog(error.str());
	error.clear();
}

void TRAKManager::conRemoteAfterConnect()
{
	ServiceHelper::WriteToLog((string)"Connected to remote database...");
}

void TRAKManager::conRemoteAfterDisconnect()
{
	ServiceHelper::WriteToLog((string)"Disconnected from remote database...");
}

void TRAKManager::conRemoteConnectionLost()
{
	ServiceHelper::WriteToLog((string)"Lost connection to remote database...\nretrying...");
}

void TRAKManager::conRemoteError(exception& e)
{
	stringstream error;
	error << "Remote database encountered an error: " << e.what() << "\n";
	ServiceHelper::WriteToLog(error.str());
	error.clear();
}

void TRAKManager::saveINIToRegistry(CSimpleIniA &iniFile, string &section)
const {
	CSimpleIniA::TNamesDepend keys;
	iniFile.GetAllKeys(section.data(), keys);
	map<string, string> map;
	HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + appType + "\\" + section).data());
	try 
	{
		for (auto const& val : keys)
		{
			string key = val.pItem;
			string value = iniFile.GetValue(section.data(), val.pItem, "");
			
			ServiceHelper::removeQuotes(value);
			if (key == "Password" && value != "")
				value = QByteArray(value.data()).toBase64();
			
			if (key == "kabID" || key == "portaID" || key == "cribID")
			{
				value = key;
				key = "trakID";
			}
			map[key] = value;
			RegistryManager::GetStrVal(hKey, key.data(), REG_SZ);
			RegistryManager::SetVal(hKey, key.data(), map[key].data(), REG_SZ);
			key.clear();
			value.clear();
		}
	}
	catch (exception& e)
	{
		ServiceHelper::WriteToError("An error occurred: " + string(e.what()));
	}
	keys.clear();
	map.clear();
	RegCloseKey(hKey);
}

void TRAKManager::CreateDataModule()
{
	CSimpleIniA ini;
	ini.SetUnicode();

	HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	string serviceInstallDir = RegistryManager::GetStrVal(hKey, "INSTALLDIR", REG_SZ) + "\\service.ini";
	RegCloseKey(hKey);
	try 
	{
		SI_Error rc = ini.LoadFile(serviceInstallDir.data());
		if (rc < 0) {
			throw HenchmanServiceException("Failed to Load INI File: " + serviceInstallDir);
		}
	

		if (!TRAKExists(ini))
			throw HenchmanServiceException("No TRAK application could be found");

		ServiceHelper::WriteToLog(appName +" exists with " +iniFile +" ini file at " +appDir);
	}
	catch (exception& e)
	{
		ServiceHelper::WriteToError("TRAKManager::CreateDataModule thrw an exception: " + string(e.what()));
		return;
	}
	
	try {
		cout << "app dir: " << appDir << iniFile << endl;

		SI_Error rc = ini.LoadFile((appDir + iniFile).data());
		if (rc < 0) {
			throw HenchmanServiceException("Failed to Load INI File: " + appDir + iniFile);
		}
		
		ServiceHelper::WriteToLog((string)"Adding Cloud entries to registry");
		string section = "Cloud";
		saveINIToRegistry(ini, section);
		ServiceHelper::WriteToLog((string)"Adding Database entries to registry");
		section = "Database";
		saveINIToRegistry(ini, section);
		ServiceHelper::WriteToLog((string)"Adding Customer entries to registry");
		section = "Customer";
		saveINIToRegistry(ini, section);
		
	}
	catch (exception &e)
	{
		ServiceHelper::WriteToError("TRAKManager::CreateDataModule thrw an exception: " + string(e.what()));
	}

}
		//cout << "Connecting to Local MySQL Database" << endl;

		//dbManager->deleteLater();
		//dbManager = nullptr;

		//string sqlFile = appDir + "database\\qkabmaster.sql";
		//dbManager->ExecuteTargetSqlScript(appType, sqlFile);
		//sqlFile = "C:\\Users\\Willem\\Documents\\henchmanTRAK Remote Support\\Files\\qkabtrak_sts_003.sql";
		//dbManager->ExecuteTargetSqlScript(appType, sqlFile);