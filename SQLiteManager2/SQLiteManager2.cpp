
#include "SQLiteManager2.h"


SQLiteManager2::SQLiteManager2(QObject *parent)
: QObject(parent)
{
	/*CSimpleIniA ini;
	HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	string installDir = RegistryManager::GetStrVal(hKey, "Install_DIR", REG_SZ);
	SI_Error rc = ini.LoadFile(installDir.append("\\service.ini").c_str());
	if (rc < 0) {
		cerr << "Failed to Load INI File" << endl;
	}
	else {
		testingDBManager = ini.GetBoolValue("DEVELOPMENT", "testingDBManager", 0);
		queryLimit = ini.GetLongValue("API", "numberOfQueries", 10);
		apiUsername = ini.GetValue("API", "Username", "");
		apiPassword = ini.GetValue("API", "Password", "");
		apiUrl.append(ini.GetValue(
			"API",
			"defaultProt",
			"https"))
			.append("://")
			.append(ini.GetValue(
				"API",
				"url",
				"webportal.henchmantrak.com/webapi/public/api/portals/exec_query"));
	}*/

	HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	QString installDir = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Install_DIR", REG_SZ));
	RegCloseKey(hKey);
	QSettings ini(installDir+"\\service.ini", QSettings::IniFormat, this);
	ini.beginGroup("SYSTEM");
	databaseName = ini.value("database", "").toString()+".db3";
	ini.endGroup();
	bool foundDriver = false;
	bool foundConnection = false;

	std::cout << "Checking available QSqlDatabase Drivers" << std::endl;
	for (const auto& str : QSqlDatabase::drivers())
	{
		std::cout << " - " << str.toStdString() << std::endl;
		if(!ServiceHelper::Contain(str, dbDriver))
			continue;
		foundDriver = true;
		break;
	}

	// throw error
	if (!foundDriver)
		return;

	std::cout << "Checking registered connection names" << std::endl;
	for (const auto& str : QSqlDatabase::connectionNames())
	{
		std::cout << " - " << str.toStdString() << std::endl;

		if (ServiceHelper::Contain(str, databaseName))
			continue;
		foundConnection = false;
		break;
	}

	QSqlDatabase db;
	if (foundConnection)
		db = QSqlDatabase::database(databaseName);
	else {
		db = QSqlDatabase::addDatabase(dbDriver, databaseName);
		db.setDatabaseName(installDir+"\\"+databaseName);
	}

	
	if (!db.open())
	{
		std::cout << "Db failed to open" << std::endl;
		return;
	}
	std::cout << "Db opened" << std::endl;
	int attr = GetFileAttributesA(db.databaseName().toUtf8());
	if (!(attr & FILE_ATTRIBUTE_HIDDEN))
	{
		std::cout << "Setting hidden attribute" << std::endl;
		SetFileAttributesA(db.databaseName().toUtf8(), attr | FILE_ATTRIBUTE_HIDDEN);
	}
	
	db.close();
}

SQLiteManager2::~SQLiteManager2()
{

}