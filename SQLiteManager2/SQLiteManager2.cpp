
#include "SQLiteManager2.h"


SQLiteManager2::SQLiteManager2(QObject *parent)
: QObject(parent)
{
	std::cout << "Constructing SQLiteManager2" << std::endl;

	std::cout << "Fetching values from registry" << std::endl;
	HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	QString installDir = QString::fromStdString(RegistryManager::GetStrVal(hKey, "INSTALLDIR", REG_SZ));
	std::string dbName = RegistryManager::GetStrVal(hKey, "DatabaseName", REG_SZ);

	std::cout << "Fetching values from ini file" << std::endl;
	QSettings ini(installDir+"\\service.ini", QSettings::IniFormat, this);
	ini.beginGroup("SYSTEM");
	databaseName = ini.value("database", "").toString().toStdString() + ".db3";
	databaseLocation = (
		ini.value("databaseLocation", "").toString() == ""
		? installDir
		: ini.value("databaseLocation", "").toString()
		).toStdString();
	ini.endGroup();
	
	QSqlDatabase db;

	try{
		std::cout << "Initializing requirements" << std::endl;

		if(QString::fromStdString(databaseName).isEmpty() && QString::fromStdString(dbName).isEmpty())
			throw HenchmanServiceException("No database name specified in service.ini");

		if(QString::fromStdString(databaseName).isEmpty() && !QString::fromStdString(dbName).isEmpty())
			databaseName = dbName;
		else
			RegistryManager::SetVal(hKey, "DatabaseName", databaseName, REG_SZ);

		std::cout << "Checking for driver availability" << std::endl;
		if (!QSqlDatabase::isDriverAvailable(databaseDriver.data()))
			throw HenchmanServiceException("QSQLITE database driver was not found");

		std::cout << "Checking if database has been previously defined" << std::endl;
		if (QSqlDatabase::contains(databaseName.data()))
			db = QSqlDatabase::database(databaseName.data());
		else {
			db = QSqlDatabase::addDatabase(databaseDriver.data(), databaseName.data());
			db.setDatabaseName(QString::fromStdString(databaseLocation + "\\" + databaseName));
		}

		std::cout << "Initializing Database" << std::endl;
		if (!db.open())
			throw HenchmanServiceException("Failed to open database");

		std::cout << "Checking if database file is hidden" << std::endl;
		int attr = GetFileAttributesA(db.databaseName().toUtf8());
		if (!(attr & FILE_ATTRIBUTE_HIDDEN))
		{
			std::cout << "Setting hidden attribute" << std::endl;
			SetFileAttributesA(db.databaseName().toUtf8(), attr | FILE_ATTRIBUTE_HIDDEN);
		}
	
		db.close();
	}catch (std::exception& e){
		ServiceHelper::WriteToLog("SQLiteManager::SQLiteManager threw and exception: " + (std::string)e.what());
	}

	RegCloseKey(hKey);
}

SQLiteManager2::~SQLiteManager2()
{

}

void SQLiteManager2::ExecQuery(
	const std::string& queryText, 
	std::vector<stringmap>* results
)
{
	std::vector<stringmap> resultVector;
	stringmap queryResult;
	QSqlDatabase db = QSqlDatabase::database(databaseName.data());

	try {
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
		while (query.next())
		{
			queryResult.clear();

			for (int i = 0; i <= query.record().count() - 1; i++)
			{
				std::string key = query.record().fieldName(i).toStdString();
				std::string value = query.value(i).toString().toStdString();
				queryResult[key] = value;
			}

			resultVector.push_back(queryResult);
		}
		query.clear();
		query.finish();
		if (!db.commit())
			throw HenchmanServiceException("Failed to commit transaction");
		db.close();

	}
	catch (std::exception& e) {
		db.rollback();
		db.close();
		throw HenchmanServiceException("SQLiteManager2::ExecQuery errored with exception: " + (std::string)e.what());
	}
	if (results)
		resultVector.swap(*results);
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
		

	}catch(std::exception& e){
		
		ServiceHelper::WriteToLog("SQLiteManager::CreateTable threw and exception: " + (std::string)e.what());
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
		[&data, &conditionals]() {

			int count = data.size();
			for (const auto& [key, value] : data)
			{
				count--;
				conditionals.append(key + "='" + value + "'");
				conditionals.append(count > 0 ? " AND " : " ");
			}
		}
	);
	queryText << "INSERT INTO " << tableName;

	columnsAndValuesThread.join();
	queryText << " (" << columns << ") SELECT " << values;

	queryText << "WHERE NOT EXISTS (SELECT * FROM " << tableName << " WHERE ";

	conditionalsThread.join();
	queryText << conditionals;
		
	queryText << "ORDER BY id DESC LIMIT 1);";

	std::cout << queryText.str() << std::endl;

	try {
	
		ExecQuery(queryText.str());

	}catch(std::exception& e){
		ServiceHelper::WriteToLog("SQLiteManager::AddEntry threw and exception: " + (std::string)e.what());
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

	std::cout << queryText.str() << std::endl;

	try {

		ExecQuery(queryText.str());

	}
	catch (std::exception& e) {
		ServiceHelper::WriteToLog("SQLiteManager::UpdateEntry threw and exception: " + (std::string)e.what());
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

	std::cout << queryText.str() << std::endl;

	try {

		ExecQuery(queryText.str());

	}
	catch (std::exception& e) {
		ServiceHelper::WriteToLog("SQLiteManager::RemoveEntry threw and exception: " + (std::string)e.what());
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
					: " "
					);
			}
		});

	std::thread conditionsThread(
		[&conditions, &conditionsForGetting]()
		{
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
	queryText << "SELECT " << selectionsMade;
	conditionsThread.join();
	queryText << "FROM " << tableName << " WHERE " << conditionsForGetting;

	std::cout << queryText.str() << std::endl;

	try {

		ExecQuery(queryText.str(), &results);

		for(const auto& result : results){
			for (const auto& [key, value] : result) {
				if (key == "id" || value == "" || value == "0")
				{
					continue;
				}
				std::cout << key << " = " << value << std::endl;
			}
		}

	}
	catch (std::exception& e) {
		ServiceHelper::WriteToLog("SQLiteManager::GetEntry threw and exception: " + (std::string)e.what());
		return results;
	}
	return results;
}