
#include "TRAKManager.h"

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
		WriteToLog("Using" + appDir + iniFile);
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
		WriteToLog("Using" + appDir + iniFile);
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
		WriteToLog("Using" + appDir + iniFile);
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

void TRAKManager::CreateDataModule()
{
	if (!(kabTRAKExists() || portaTRAKExists() || cribTRAKExists()))
	{
		cout << "No TRAK application could be found" << endl;
		return;
	}
	cout << appName << " exists with " << iniFile << " ini file at " << appDir << endl;
	
	try {
		CSimpleIni ini;
		ini.SetUnicode();
		cout << "app dir: " << appDir << iniFile << endl;


		SI_Error rc = ini.LoadFile((appDir + iniFile).c_str());
		if (rc < 0) {
			cerr << "Failed to Load INI File" << endl;
		}
		CSimpleIniA::TNamesDepend keys;
		ini.GetAllKeys("Cloud", keys);
		map<string, string> cloud_map ;
		HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + appType + "\\Cloud"));
		for (auto const& val : keys)
		{
			cloud_map[val.pItem] = ini.GetValue("Cloud", val.pItem, "");
			GetStrVal(hKey, val.pItem, REG_SZ);
			SetStrVal(hKey, val.pItem, cloud_map[val.pItem], REG_SZ);
		}
		keys.clear();
		RegCloseKey(hKey);
		
		/*cout << cloud_map["UseProxy"] << endl;
		if (stoi(cloud_map["UseProxy"].data()))
		{
			cout << "Don't use proxy" << endl;
		}*/

		ini.GetAllKeys("Database", keys);
		map<string, string> database_map;
		hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + appType + "\\Database"));
		for (auto const& val : keys)
		{
			database_map[val.pItem] = ini.GetValue("Database", val.pItem, "");
			GetStrVal(hKey, val.pItem, REG_SZ);
			SetStrVal(hKey, val.pItem, database_map[val.pItem], REG_SZ);
		}
		keys.clear();
		RegCloseKey(hKey);

		ini.GetAllKeys("Customer", keys);
		map<string, string> customer_map;
		hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + appType + "\\Customer"));
		for (auto const& val : keys)
		{
			string key = val.pItem;
			if (val.pItem == "kabID" || val.pItem == "portaID" || val.pItem == "cribID")
			{
				key = "trackID";
			}
			customer_map[key] = ini.GetValue("Customer", val.pItem, "");
			GetStrVal(hKey, key.c_str(), REG_SZ);
			SetStrVal(hKey, key.c_str(), customer_map[val.pItem], REG_SZ);
		}
		keys.clear();
		RegCloseKey(hKey);
		//ini.~CSimpleIniTempl();
	}
	catch (exception &e)
	{
		cout << "An error occured: " << e.what() << endl;
	}

}