
#include "TRAKManager.h"

using namespace std;

string TRAKManager::appDir;
string TRAKManager::iniFile;
string TRAKManager::appName;
string TRAKManager::appType;

//TRAKManager::TRAKManager()
//{
//	appDir = "";
//	iniFile = "";
//	appName = "";
//
//}

bool TRAKManager::kabTRAKExists()
{
	string AppDir32 = "C:\\Program Files (x86)\\HenchmanTRAK\\kabTRAK\\";
	if (filesystem::exists(AppDir32.c_str())) {
		appDir = AppDir32;
		iniFile = "trak.ini";
		appName = "kabtrak.exe";
		appType = "kabTRAK";
		WriteToLog("Using " + appDir + iniFile);
		return TRUE;
	}
	return FALSE;
}

bool TRAKManager::portaTRAKExists()
{
	string AppDir32 = "C:\\Program Files (x86)\\HenchmanTRAK\\portaTRAK\\";
	if (filesystem::exists(AppDir32.c_str())) {
		appDir = AppDir32;
		iniFile = "porta.ini";
		appName = "portaTRAK.exe";
		appType = "portaTRAK";
		WriteToLog("Using " + appDir + iniFile);
		return TRUE;
	}
	return FALSE;
}

bool TRAKManager::cribTRAKExists()
{
	string AppDir32 = "C:\\Program Files (x86)\\HenchmanTRAK\\cribTRAK\\";
	if (filesystem::exists(AppDir32.c_str())) {
		appDir = AppDir32;
		iniFile = "crib.ini";
		appName = "kribTRAK.exe";
		appType = "cribTRAK";
		WriteToLog("Using " + appDir + iniFile);
		return TRUE;
	}
	return FALSE;
}

void TRAKManager::conHechmanAfterConnect()
{
	WriteToLog("Connected to local MYSQL database...");
}

void TRAKManager::conHechmanAfterDisconnect()
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
{
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

void TRAKManager::CreateDataModule()
{
	if (!(kabTRAKExists() || portaTRAKExists() || cribTRAKExists()))
	{
		cout << "No TRAK application could be found" << endl;
		return;
	}
	cout << appName << " exists with " << iniFile << " ini file at " << appDir << endl;
	
	try {
		CSimpleIniA ini;
		ini.SetUnicode();
		cout << "app dir: " << appDir << iniFile << endl;


		SI_Error rc = ini.LoadFile((appDir + iniFile).c_str());
		if (rc < 0) {
			cerr << "Failed to Load INI File" << endl;
		}
		
		cout << "Adding Cloud entries to registry" << endl;
		string section = "Cloud";
		saveINIToRegistry(ini, section);
		cout << "Adding Database entries to registry" << endl;
		section = "Database";
		saveINIToRegistry(ini, section);
		cout << "Adding Customer entries to registry" << endl;
		section = "Customer";
		saveINIToRegistry(ini, section);

		cout << "Connecting to MySQL Database" << endl;
		connection(appType);

		string sqlFile = appDir + "database\\qkabmaster.sql";

		ExecuteTargetSqlScript(appType, sqlFile);

		
		/*cout << cloud_map["UseProxy"] << endl;
		if (stoi(cloud_map["UseProxy"].data()))
		{
			cout << "Don't use proxy" << endl;
		}*/

		//ini.~CSimpleIniTempl();
	}
	catch (exception &e)
	{
		cout << "An error occured: " << e.what() << endl;
	}

}