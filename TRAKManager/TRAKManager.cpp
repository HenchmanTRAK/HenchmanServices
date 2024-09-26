


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
	if (filesystem::exists(AppDir32.c_str())) {
		appDir = AppDir32+"\\";
		iniFile = ini.GetValue("TRAK", "INI_FILE", "");
		appName = ini.GetValue("TRAK", "EXE_FILE", "");
		appType = ini.GetValue("TRAK", "APP_NAME", "");
		WriteToLog("Using " + appDir + iniFile);
		return TRUE;
	}
	return FALSE;
}

void TRAKManager::conHenchmanAfterConnect()
{
	WriteToLog("Connected to local MYSQL database...");
}

void TRAKManager::conHenchmanAfterDisconnect()
{
	WriteToLog("Disconnected from local MYSQL database...");
}

void TRAKManager::conHenchmanConnectionLost()
{
	WriteToLog("Lost connection to local MYSQL database...\nretrying...");
}

void TRAKManager::conHenchmanError(exception& e)
{
	stringstream error;
	error << "Local MYSQL database encountered an error: " << e.what() << "\n";
	WriteToLog(error.str());
	error.clear();
}

void TRAKManager::conRemoteAfterConnect()
{
	WriteToLog("Connected to remote database...");
}

void TRAKManager::conRemoteAfterDisconnect()
{
	WriteToLog("Disconnected from remote database...");
}

void TRAKManager::conRemoteConnectionLost()
{
	WriteToLog("Lost connection to remote database...\nretrying...");
}

void TRAKManager::conRemoteError(exception& e)
{
	stringstream error;
	error << "Remote database encountered an error: " << e.what() << "\n";
	WriteToLog(error.str());
	error.clear();
}

void TRAKManager::saveINIToRegistry(CSimpleIniA &iniFile, string &section)
const {
	CSimpleIniA::TNamesDepend keys;
	iniFile.GetAllKeys(section.data(), keys);
	map<string, string> map;
	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + appType + "\\" + section));
	for (auto const& val : keys)
	{
		string key = val.pItem;
		string value = iniFile.GetValue(section.data(), val.pItem, "");
		//sanitize(value);
		removeQuotes(value);
		if (key == "Password" && value != "")
			value = base64(value);
		map[key] = value;
		GetStrVal(hKey, key.data(), REG_SZ);
		SetStrVal(hKey, key.data(), map[key], REG_SZ);
		if (key == "kabID" || key == "portaID" || key == "cribID")
		{
			value = key;
			key = "trakID";
			map[key] = value;
			GetStrVal(hKey, key.data(), REG_SZ);
			SetStrVal(hKey, key.data(), map[key], REG_SZ);
		}
		key.clear();
	}
	keys.clear();
	map.clear();
	RegCloseKey(hKey);
}

void TRAKManager::CreateDataModule(DatabaseManager* dbManager)
{
	CSimpleIniA ini;
	ini.SetUnicode();

	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	string serviceInstallDir = (GetStrVal(hKey, "Install_DIR", REG_SZ) + "\\service.ini");
	RegCloseKey(hKey);
	SI_Error rc = ini.LoadFile(serviceInstallDir.c_str());
	if (rc < 0) {
		WriteToError("Failed to Load INI File: " + serviceInstallDir);
		return;
	}

	if (!TRAKExists(ini))
	{
		WriteToError("No TRAK application could be found");
		return;
	}
	WriteToLog(appName +" exists with " +iniFile +" ini file at " +appDir);
	
	try {
		cout << "app dir: " << appDir << iniFile << endl;

		rc = ini.LoadFile((appDir + iniFile).c_str());
		if (rc < 0) {
			WriteToError("Failed to Load INI File: "+ appDir + iniFile);
			return;
		}
		
		WriteToLog("Adding Cloud entries to registry");
		string section = "Cloud";
		saveINIToRegistry(ini, section);
		WriteToLog("Adding Database entries to registry");
		section = "Database";
		saveINIToRegistry(ini, section);
		WriteToLog("Adding Customer entries to registry");
		section = "Customer";
		saveINIToRegistry(ini, section);

		cout << "Connecting to Local MySQL Database" << endl;
		
		dbManager->connectToLocalDB(appType);
		
	}
	catch (exception &e)
	{
		WriteToError("An error occurred: " + string(e.what()));
		throw e;
	}

}

		//string sqlFile = appDir + "database\\qkabmaster.sql";
		//dbManager->ExecuteTargetSqlScript(appType, sqlFile);
		//sqlFile = "C:\\Users\\Willem\\Documents\\henchmanTRAK Remote Support\\Files\\qkabtrak_sts_003.sql";
		//dbManager->ExecuteTargetSqlScript(appType, sqlFile);