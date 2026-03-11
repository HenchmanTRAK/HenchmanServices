#include "QueryManager.h";

QueryManager::QueryManager(QObject* parent, const s_DATABASE_INFO& database_info)
	:QObject(parent), db_info(database_info)
{

}

QueryManager::~QueryManager()
{
	LOG << "Deconstructing QueryManager";

	if (db_info.schema.isEmpty())
	{
		qDebug() << "Schema for database has not been set";
		return;
	}

	if (QSqlDatabase::contains(db_info.schema))
	{
		try {
			QSqlDatabase db = QSqlDatabase::database(db_info.schema);
			if (db.isOpen()) {
				db.close();
			}
		}catch(void*){}

		//QSqlDatabase::removeDatabase(db_info.schema);
	}
}

void QueryManager::setSchema(const QString& new_schema)
{
	/*if (schema != new_schema)
		schema = new_schema;*/
	db_info.schema = new_schema;
}

void QueryManager::set_database_details(const s_DATABASE_INFO& database_info)
{
	/*if (schema != new_schema)
		schema = new_schema;*/
	db_info = database_info;
}

QSqlDatabase QueryManager::GetDatabaseConnection()
{
	QSqlDatabase db;
	try {


		if (QSqlDatabase::contains(db_info.schema)) {

			db = QSqlDatabase::database(db_info.schema);
			LOG << "connecting to existing database";
		}
		else {
			/*db = QSqlDatabase::addDatabase(databaseDriver, databaseName);
			db.setDatabaseName(databaseLocation + "\\" + databaseName);
			LOG << "Initializing new Database";*/
			throw HenchmanServiceException("Database connection does not exist, please create one before using QueryManager");
		}

		if (!db.open())
			throw HenchmanServiceException("Failed to open database");

	}
	catch (const std::exception& e) {
		if (db.isOpen())
			db.close();
		throw e;
	}

	db.close();

	return db;
}

QString QueryManager::getSchema()
{
	return db_info.schema;
}

s_TZ_INFO QueryManager::GetTimezone()
{

	if (!tz_info.time_zone.isEmpty() && tz_info.time_zone_offset) {
		return tz_info;
	}

	qDebug() << "Schema: " << db_info.schema;

	QSqlDatabase db(GetDatabaseConnection());

	if (db.isOpen())
		qDebug() << "Database Open";
	else if(!db.open())
		qDebug() <<	"Failed to open Database";

	{
		QSqlQueryModel time_zone_modal(this);
		time_zone_modal.setQuery("SELECT LPAD(TIME_FORMAT(TIMEDIFF(NOW(), UTC_TIMESTAMP),'%H:%i'),6,'+') as tz, TIMESTAMPDIFF(SECOND, UTC_TIMESTAMP, NOW()) as tz_offset", db);
		QString tz = time_zone_modal.record(0).value("tz").toString();
		int tz_offset = time_zone_modal.record(0).value("tz_offset").toInt();
		time_zone_modal.clear();
		tz_info.time_zone = tz;
		tz_info.time_zone_offset = tz_offset;

		time_zone_modal.deleteLater();
	}

	if (db.isOpen())
		db.close();

	if (!db.isOpen())
		qDebug() << "Database Closed";

	return tz_info;
}


QVariantMap QueryManager::recordToMap(const QSqlRecord& record)
{
	QVariantMap queryResult;

	for (int i = 0; i < record.count(); i++)
	{
		QString column = record.fieldName(i);
		QVariant value = record.value(i);

		qDebug() << column << ": " << value;
		switch (value.typeId()) {
		case QVariant::String: {
			queryResult.insert(column, value.toString());
			break;
		}
		case QVariant::Int: {
			queryResult.insert(column, value.toInt());
			break;
		}
		case QVariant::Date: {
			if (!value.toDate().isValid())
			{
				queryResult.insert(column, "");
				break;
			}

			queryResult.insert(column, value.toDate().toString(Qt::ISODateWithMs));
			break;
		}
		case QVariant::Time: {
			if (!value.toTime().isValid())
			{
				queryResult.insert(column, "");
				break;
			}

			queryResult.insert(column, value.toTime().toString(Qt::ISODateWithMs));
			break;
		}
		case QVariant::DateTime: {
			if (!value.toDateTime().isValid())
			{
				queryResult.insert(column, "");
				break;
			}

			QDateTime dt = value.toDateTime();

			dt.setOffsetFromUtc(tz_info.time_zone_offset);

			queryResult.insert(column, dt.toUTC().toString(Qt::ISODateWithMs));
			break;
		}
		default: {
			queryResult.insert(column, value);
			break;
		}
		}
	}

	return queryResult;
}

QVariantMap QueryManager::processPlaceholders(const QVariantMap& placeholders, QString& statement)
{
	QVariantMap boundValues;

	if (statement.contains(":tz") && !placeholders.contains("tz")) {
		GetTimezone();
		boundValues.insert(":tz", tz_info.time_zone);
	}
	
	if (placeholders.isEmpty())
		return boundValues;

	QMapIterator it(placeholders);

	while (it.hasNext()) {
		it.next();

		QString key = it.key();
		QVariant value = it.value();
		key = ":" + key;

		if (value.typeId() == QVariant::DateTime) {
			boundValues.insert(key, value.toDateTime().toString(Qt::ISODateWithMs));
			continue;
		}

		if (value.canConvert<QString>()) {
			boundValues.insert(key, value.toString());
			continue;
		}

		if (value.canConvert<QJsonArray>()) {
			if (!statement.contains(key))
				continue;

			QStringList values;
			foreach(QJsonValue val, value.toJsonArray()) {
				QString value = "'" + (val.isDouble() ? QString::number(val.toInt()) : val.toString()) + "'";

				if (values.contains(value))
					continue;
				values << value;
			}
			
			QString firstSection = statement.sliced(0, statement.indexOf(key));
			QString secondSection = statement.sliced(statement.indexOf(key) + key.size());
			statement = firstSection + values.join(",") + secondSection;
			continue;
		}

		boundValues.insert(key, value.toString());

	};

	

	return boundValues;
}

void QueryManager::BindValues(const QVariantMap& values, QSqlQuery* query)
{
	QMapIterator it(values);

	while (it.hasNext()) {
		it.next();
		query->bindValue(it.key(), it.value());
	}
}

QList<QVariantMap> QueryManager::execute(const TCHAR* sql, const QVariantMap& placeholders)
{
	QList<QVariantMap> results(0);

	QSqlDatabase db(GetDatabaseConnection());


	if(!db.open())
		throw HenchmanServiceException("Failed to open DB Connection");

	int isSelect = false;

	try {

		QString statement(sql);

		if (statement.trimmed() == "")
			throw HenchmanServiceException("No Query was provided.");
				

		LOG << "Executing: " << statement.toStdString();

		QVariantMap boundValues = processPlaceholders(placeholders, statement);

		QSqlQuery query(db);


		query.prepare(statement);

		BindValues(boundValues, &query);
		
		isSelect = query.isSelect();

		if (!isSelect) {
			db.transaction();
		}
		
		if (!query.exec()) {

			QSqlError err = query.lastError();
			qWarning() << "Database Text" << err.databaseText();
			qWarning() << "Driver Text" << err.driverText();
			qWarning() << "Native Error Code" << err.nativeErrorCode();
			qWarning() << "Text" << err.text();
 			
			query.finish();
			throw HenchmanServiceException("Failed executing: " + statement.toStdString() + "\nReason provided: " + err.text().toStdString());
		}

		QSqlRecord record = query.record();
		
		while (query.next())
		{	
			QVariantMap queryResult;
			for (int i = 0; i < record.count(); ++i)
			{
				const QString column = record.fieldName(i);
				const QVariant value = query.value(i);

				//qDebug() << column << ": " << value;
				
				switch (value.typeId()) {
				case QVariant::ByteArray: {
					queryResult.insert(column, QString::fromStdString(value.toByteArray().toStdString()));
					continue;
				}
				case QVariant::Int: {
					queryResult.insert(column, value.toInt());
					continue;
				}
				case QVariant::Date: {
					if (!value.toDate().isValid())
					{
						queryResult.insert(column, "");
						continue;
					}

					queryResult.insert(column, value.toDate().toString(Qt::ISODateWithMs));
					continue;
				}
				case QVariant::Time: {
					if (!value.toTime().isValid())
					{
						queryResult.insert(column, "");
						continue;
					}

					queryResult.insert(column, value.toTime().toString(Qt::ISODateWithMs));
					continue;
				}
				case QVariant::DateTime: {
					if (!value.toDateTime().isValid())
					{
						queryResult.insert(column, "");
						continue;
					}

					QDateTime dt = value.toDateTime();

					dt.setOffsetFromUtc(tz_info.time_zone_offset);

					queryResult.insert(column, dt.toUTC().toString(Qt::ISODateWithMs));
					continue;
				}
				default: {
					queryResult.insert(column, value.toString());
					continue;
				}
				}
			}
			results.append(queryResult);

		}

		query.finish();

		if (!isSelect)
			db.commit();

	}
	catch (const std::exception& e) {
		if (!isSelect)
			db.rollback();

		ServiceHelper().WriteToError(e.what());
	}

	if (db.isOpen())
		db.close();

	return results;
}
QList<QVariantMap> QueryManager::execute(const QString& sql, const QVariantMap& placeholders)
{ 
	return execute(sql.toStdString(), placeholders);
}
QList<QVariantMap> QueryManager::execute(const std::string& sql, const QVariantMap& placeholders)
{ 
	return execute(sql.data(), placeholders);
}

QJsonArray QueryManager::execute(const TCHAR* sql, const QJsonObject& placeholders)
{

	QJsonArray results;

	QList<QVariantMap> queryResults = execute(sql, placeholders.toVariantMap());

	for(const auto& result : queryResults)
		results.append(QJsonObject::fromVariantMap(result));

	return results;
}
QJsonArray QueryManager::execute(const QString& sql, const QJsonObject& placeholders)
{
	return execute(sql.toStdString(), placeholders);
}
QJsonArray QueryManager::execute(const std::string& sql, const QJsonObject& placeholders)
{
	return execute(sql.data(), placeholders);
}

QList<QStringMap> QueryManager::execute(const TCHAR* sql, const QStringMap& placeholders)
{

	QList<QStringMap> results;
	QVariantMap variant_placeholders;

	QMapIterator it(placeholders);

	while (it.hasNext()) {
		it.next();
		variant_placeholders[it.key()] = it.value();
	}

	QList<QVariantMap> queryResults = execute(sql, variant_placeholders);

	for (const auto& result : queryResults) {
		QStringMap results_map;
		QMapIterator it(result);
		while (it.hasNext()) {
			it.next();
			results_map[it.key()] = it.value().toString();
		}

		results.append(results_map);
	}


	return results;
}
QList<QStringMap> QueryManager::execute(const QString& sql, const QStringMap& placeholders)
{
	return execute(sql.toStdString(), placeholders);
}
QList<QStringMap> QueryManager::execute(const std::string& sql, const QStringMap& placeholders)
{
	return execute(sql.data(), placeholders);
}

std::vector<QStringMap> QueryManager::execute(const TCHAR* sql)
{
	std::vector<QStringMap> results;

	QList<QVariantMap> queryResults = execute(sql, QVariantMap());

	for (const auto& result : queryResults) {
		QStringMap stringMap;

		QMapIterator it(result);

		do {
			it.next();

			stringMap.insert(it.key(), it.value().toString());
		} while (it.hasNext());

		results.push_back(stringMap);
	}

	return results;
}
std::vector<QStringMap> QueryManager::execute(const QString& sql)
{
	return execute(sql.toStdString());
}
std::vector<QStringMap> QueryManager::execute(const std::string& sql)
{
	return execute(sql.data());
}

int QueryManager::ExecuteTargetSqlScript(const std::string& filepath)
{
	int successCount = 0;

	QSqlDatabase db(GetDatabaseConnection());

	QFile file(filepath.data());
	try {
		// HenchmanServiceException
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			throw HenchmanServiceException("Failed to open target file");

		ServiceHelper().WriteToLog("Successfully opened target file: " + filepath);

		QTextStream in(&file);
		QString sql = in.readAll();

		QStringList sqlStatements = sql.split(';', Qt::SkipEmptyParts);

		if (!db.open())
			throw HenchmanServiceException("Failed to open DB Connection");

		db.transaction();

#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			throw HenchmanServiceException("Failed to execute initial DB Query");
#endif

		QSqlQuery query(db);
		
		for (QString& statement : sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			
			if (query.exec(statement))
				successCount++;
			else {
				if (statement.length() <= 128)
					throw HenchmanServiceException("Failed executing: " + statement.toStdString() + "\nReason provided: " + query.lastError().text().toStdString());
				else
					throw HenchmanServiceException("Failed executing statement. Reason provided: " + query.lastError().text().toStdString());

			}
			query.clear();
		}

		if (!db.commit())
			db.rollback();
	}
	catch (std::exception& e)
	{
		
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
		}
		ServiceHelper().WriteToError(e.what());
	}

	if (db.isOpen())
		db.close();
	if (file.isOpen())
		file.close();

	return successCount;
}
