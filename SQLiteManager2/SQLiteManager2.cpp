
#include "SQLiteManager2.h"


SQLiteManager2::SQLiteManager2(QObject *parent)
: QObject(parent)
{
	LOG << "Constructing SQLiteManager2";

	LOG << "Fetching values from registry";
	//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, t2tstr("SOFTWARE\\HenchmanTRAK\\HenchmanService").c_str());
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal("INSTALL_DIR", REG_SZ, (char*)buffer, size);
	//QString installDir = RegistryManager::GetStrVal(hKey, "INSTALL_DIR", REG_SZ).data();
	QString installDir(buffer);

	rtManager.GetVal("DatabaseName", REG_SZ, (char*)buffer, size);
	//std::string dbName = RegistryManager::GetStrVal(hKey, "DatabaseName", REG_SZ);
	std::string dbName(buffer);

	LOG << "Fetching values from ini file";
	QSettings ini(installDir+"\\service.ini", QSettings::IniFormat, this);
	ini.beginGroup("SYSTEM");
	databaseName = ini.value("Database", "").toString().toStdString();
	databaseLocation = ini.value("DatabaseLocation", "").toString();
	ini.endGroup();
	
	QSqlDatabase db;

	try{
		if (databaseLocation == "")
			databaseLocation = installDir;
		LOG << "Initializing requirements";
		LOG << "Database name in ini: " << databaseName;
		LOG << "Database name in registry: " << dbName;
		// HenchmanServiceException
		if(databaseName.empty() && dbName.empty())
			throw HenchmanServiceException("No database name specified in service.ini");

		if(databaseName.empty() && !dbName.empty())
			databaseName = dbName;
		if(databaseName != dbName)
			if (rtManager.SetVal("DatabaseName", REG_SZ, (char*)databaseName.c_str(), (databaseName.length() + 1)))
				throw HenchmanServiceException("Failed to set SERVICE_NAME to registry");
			//RegistryManager::SetVal(hKey, "DatabaseName", databaseName, REG_SZ);

		LOG << "Checking for driver availability";
		if (!QSqlDatabase::isDriverAvailable(databaseDriver))
			throw HenchmanServiceException("QSQLITE database driver was not found");

		LOG << "Checking if database has been previously defined";
		CreateNewDatabase(&db, QString::fromStdString(databaseName));
		
		LOG << "Checking if database file is hidden";
		int attr = GetFileAttributesA(db.databaseName().toUtf8());
		if (!(attr & FILE_ATTRIBUTE_HIDDEN))
		{
			LOG << "Setting hidden attribute";
			SetFileAttributesA(db.databaseName().toUtf8(), attr | FILE_ATTRIBUTE_HIDDEN);
		}
	}
	catch (std::exception& e)
	//catch (const HenchmanServiceException& e)
	{
		if (db.isOpen())
			db.close();

		ServiceHelper().WriteToError(e.what());
	}

	//RegCloseKey(hKey);
}

SQLiteManager2::~SQLiteManager2()
{

}

void SQLiteManager2::CreateNewDatabase(QSqlDatabase* targetDatabase, const QString& databaseName)
{
	QSqlDatabase &db = *targetDatabase;
	try {
		if (QSqlDatabase::contains(databaseName))
			db = QSqlDatabase::database(databaseName);
		else {
			db = QSqlDatabase::addDatabase(databaseDriver, databaseName);
			db.setDatabaseName(databaseLocation + "\\" + databaseName);
		}

		LOG << "Initializing Database";
		if (!db.open())
			throw HenchmanServiceException("Failed to open database");

		db.close();
	}
	catch (const std::exception& e) {
		if (db.isOpen())
			db.close();
		targetDatabase = &db;
		throw e;
	}

	targetDatabase = &db;
}


void SQLiteManager2::ExecQuery(const std::string& queryText)
{
	std::vector<stringmap> results;
	return ExecQuery(queryText, results);
}

void SQLiteManager2::ExecQuery(
	const std::string& queryText, 
	std::vector<stringmap>& results
)
{
	std::vector<stringmap> resultVector;
	stringmap queryResult;

	QMutexLocker locket(&mutex);

	QSqlDatabase db;

	if (!db.thread()->isCurrentThread()) {
		QSqlDatabase::removeDatabase(QString::fromStdString(databaseName));
		CreateNewDatabase(&db, QString::fromStdString(databaseName));

		qDebug() << "Global Manager thread is current: " << this->thread()->isCurrentThread();
		qDebug() << "Global QSqlDatabase thread is current: " << QSqlDatabase::database(QString::fromStdString(databaseName)).thread()->isCurrentThread();
		qDebug() << "Local QSqlDatabase thread is current: " << db.thread()->isCurrentThread();
	}
	
	db = QSqlDatabase::database(QString::fromStdString(databaseName));
		
	try {
		// HenchmanServiceException
		if(!db.isOpen())
			if (!db.open())
			{
				throw HenchmanServiceException("Failed to open database");
			}

		if (!db.driver()->hasFeature(QSqlDriver::Transactions))
		{
			throw HenchmanServiceException("QSQLITE database driver does not support transactions");
		}

		db.transaction();
		QSqlQuery query(db);

		if (!query.exec(QString::fromStdString(queryText)))
		{
			throw HenchmanServiceException(
				"Failed to execute query: " +
				query.lastQuery().toStdString() +
				"\n Error Code: " +
				query.lastError().nativeErrorCode().toStdString() +
				"\n Database Statement: " +
				query.lastError().databaseText().toStdString() +
				"\n Driver Error: " +
				query.lastError().driverText().toStdString() +
				"\n General Error Text: " +
				query.lastError().text().toStdString()
			);
		}
		qDebug() << "Query Successful";
		while (query.next())
		{
			queryResult.clear();
			qDebug() << query.record();
			for (int i = 0; i <= query.record().count() - 1; i++)
			{
				std::string key = query.record().fieldName(i).toStdString();
				std::string value = query.value(i).toString().toStdString();
				queryResult[key] = value;
			}

			resultVector.push_back(queryResult);
		}
		qDebug() << resultVector;
		query.clear();
		query.finish();
		if (!db.commit())
			throw HenchmanServiceException("Failed to commit transaction");
		db.close();

	}
	catch (std::exception& e)
	//catch (const HenchmanServiceException& e)
	{
		ServiceHelper().WriteToError("An exception was thrown: " + (std::string)e.what());
		if (db.isOpen())
		{
			db.rollback();
			db.close();
		}
		//throw HenchmanServiceException("An exception was thrown: " + (std::string)e.what());
	}
	//if (results)
	resultVector.swap(results);
}

int SQLiteManager2::CreateTable(
	const std::string& newTable,
	const std::vector<std::string>& columns
)
{
	try {

		std::stringstream queryText;
		queryText << "CREATE TABLE IF NOT EXISTS";
		queryText << " " << newTable << " (";
		queryText << "id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,\n";
		queryText << "createdAt TEXT NOT NULL DEFAULT (datetime('now', 'localtime')),\n";
		queryText << "updatedAt TEXT NOT NULL DEFAULT (datetime('now', 'localtime')),\n";

		for (const auto& column : columns)
		{
			queryText << column;
			if(column != columns.back())
				queryText << ",\n";
		}
		queryText << ");\n";
		ExecQuery(queryText.str());

		queryText.clear();
		queryText.str(std::string());

		queryText << "CREATE TRIGGER IF NOT EXISTS set_updatedAt_on_" << newTable << "\n";
		queryText << "AFTER UPDATE ON " << newTable << "\n";
		queryText << "WHEN OLD.updatedAt <> (datetime('now', 'localtime'))\n";
		queryText << "BEGIN\n";
		queryText << "	UPDATE " << newTable << "\n";
		queryText << "	SET updatedAt = (datetime('now', 'localtime'))\n";
		queryText << "	WHERE OLD.id = id;\n";
		queryText << "END;\n";
		ExecQuery(queryText.str());		
		

	}
	catch (std::exception& e)
	//catch (const HenchmanServiceException& e)
	{
		
		ServiceHelper().WriteToError(e.what());
		return 0;
	}
	return 1;
}

int SQLiteManager2::AddEntry(
	const std::string& tableName,
	const stringmap& data
)
{
	/*
	
		INSERT INTO :tableName (:data.keys) SELECT :data.values
		WHERE NOT EXISTS (
			SELECT * FROM :tableName WHERE 
			:data.keys = :data.values (AND)
			ORDER BY id DESC LIMIT 1
		)
	
	*/

	std::stringstream queryText;
	std::string columns;
	std::string values;
	std::string conditionals;
	std::string update_sets;

	std::thread columnsAndValuesThread(
		[&data, &columns, &values]() {

		int count = data.size();
		for (const auto& [key, value] : data)
		{
			count--;
			columns.append(key + (count > 0 ? ", " : ""));
			values.append("'" + value + "'" + (count > 0 ? ", " : " "));
		}
		}
	);

	std::thread conditionalsThread(
		[&data, &conditionals, &update_sets]() {

			int count = data.size();
			for (const auto& [key, value] : data)
			{
				count--;
				conditionals.append(key + "='" + value + "'");
				update_sets.append(key + "='" + value + "'");
				conditionals.append(count > 0 ? " AND " : " ");
				update_sets.append(count > 0 ? ", " : "");
			}
		}
	);
	queryText << "INSERT INTO " << tableName;

	columnsAndValuesThread.join();
	conditionalsThread.join();

	queryText << " (" << columns << ") VALUES (" << values << ") ON CONFLICT DO UPDATE SET " + update_sets;

	/*queryText << "WHERE NOT EXISTS (SELECT * FROM " << tableName << " WHERE ";

	queryText << conditionals;
		
	queryText << "ORDER BY id DESC LIMIT 1);";*/

	LOG << queryText.str();

	try {
	
		ExecQuery(queryText.str());

	}
	catch (std::exception& e)
	//catch (const HenchmanServiceException& e)
	{
		ServiceHelper().WriteToError(e.what());
		return 0;
	}
	return 1;
}

int SQLiteManager2::UpdateEntry(
	const std::string& tableName,
	const std::vector<std::string>& conditions,
	const stringmap& data
)
{
	std::stringstream queryText;
	queryText << "UPDATE OR ROLLBACK " << tableName << " SET ";
	std::string dataToUpdate;
	std::string conditionsForUpdate;

	std::thread dataToUpdateThread(
		[&dataToUpdate, &data]() 
		{
			int count = data.size();
			for (const auto& [
				key,
				value
			] : data)
			{
				count--;
				dataToUpdate.append(key + "=" + value + (count > 0 ? ", " : " "));
			}
		});

	std::thread conditionsForUpdateThread(
		[&conditionsForUpdate, &conditions]()
		{
			for (const auto& condition : conditions)
			{
				conditionsForUpdate.append(condition);
				conditionsForUpdate.append(conditions.back() != condition
					? " AND " 
					: ""
				);
			}
		});

	dataToUpdateThread.join();
	conditionsForUpdateThread.join();

	queryText << dataToUpdate << "WHERE ";
	queryText << conditionsForUpdate << ";";

	LOG << queryText.str();

	try {

		ExecQuery(queryText.str());

	}
	catch (std::exception& e)
	//catch (const HenchmanServiceException& e)
	{
		ServiceHelper().WriteToError(e.what());
		return 0;
	}
	return 1;
}

int SQLiteManager2::RemoveEntry(
	const std::string& tableName,
	const std::vector<std::string>& conditions
)
{
	std::stringstream queryText;
	std::string conditionsForDelete;

	queryText << "DELETE FROM " << tableName << " WHERE ";

	for (const auto& condition : conditions)
	{
		conditionsForDelete.append(condition);
		conditionsForDelete.append(conditions.back() != condition
			? " AND " 
			: ""
		);
	}

	queryText << conditionsForDelete << ";";

	LOG << queryText.str();

	try {

		ExecQuery(queryText.str());

	}
	catch (std::exception& e)
	//catch (const HenchmanServiceException& e)
	{
		ServiceHelper().WriteToError(e.what());
		return 0;
	}
	return 1;
}

std::vector<stringmap> SQLiteManager2::GetEntry(
	const std::string& tableName,
	const std::vector<std::string>& selections,
	const std::vector<std::string>& conditions
)
{
	qDebug() << tableName;
	qDebug() << selections;
	qDebug() << conditions;

	std::vector<stringmap> results;
	std::stringstream queryText;
	std::string selectionsMade;
	std::string conditionsForGetting;

	std::thread selectionsThread(
		[&selections, &selectionsMade]() 
		{
			for (const auto& selection : selections)
			{
				selectionsMade.append( selection);
				selectionsMade.append(selections.back() != selection
					? ", " 
					: ""
					);
			}
		});

	std::thread conditionsThread(
		[&conditions, &conditionsForGetting]()
		{
			if (conditions.size() <= 0)
				return;
			for (const auto& condition : conditions)
			{
				conditionsForGetting.append(condition);
				conditionsForGetting.append(conditions.back() != condition
					? " AND "
					: ";"
					);
			}
		});

	selectionsThread.join();
	queryText << "SELECT\n" << selectionsMade << "\n";
	queryText << "FROM " << tableName << "\n";
	conditionsThread.join();
	if(conditions.size() > 0)
		queryText << "WHERE " << conditionsForGetting;

	LOG << queryText.str();

	try {

		ExecQuery(queryText.str(), results);

		for(const auto& result : results){
			for (const auto& [key, value] : result) {
				if (key == "id" || value == "" || value == "0")
				{
					continue;
				}
				LOG << key << " = " << value;
			}
		}

	}
	catch (std::exception& e)
	//catch (const HenchmanServiceException& e)
	{
		ServiceHelper().WriteToError(e.what());
		return results;
	}
	return results;
}