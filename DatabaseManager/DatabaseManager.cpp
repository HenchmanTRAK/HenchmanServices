
#include "DatabaseManager.h"

using namespace std;

static array<string, 2> timeStamp;

static bool doNotRunCloudUpdate = 0;
static bool parseCloudUpdate = 1;
static bool pushCloudUpdate = 1;

string getValidDrivers()
{
	stringstream results;
	for (const auto& str : QSqlDatabase::drivers())
	{
		results << " - " << str.toStdString() << "\n";
	}
	return results.str();
}
//
//static int checkValidConnections(QString &targetConnection)
//{
//	for (auto& str : QSqlDatabase::connectionNames())
//	{
//		std::cout << " - " << str.toUtf8().data() << endl;
//		if (str == targetConnection)
//			return TRUE;
//	}
//
//	return FALSE;
//}

static std::array<QString, 2> GetTrakDirAndIni(RegistryManager::CRegistryManager & rtManager)
{
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	QString trakDir;
	QString iniFile;
	try {
		rtManager.GetVal("TRAK_DIR", REG_SZ, (TCHAR*)buffer, 1024);
		trakDir = buffer;
		rtManager.GetVal("INI_FILE", REG_SZ, (TCHAR*)buffer, 1024);
		iniFile = buffer;
	}
	//catch (std::exception& e)
	catch(const HenchmanServiceException& e)
	{
		ServiceHelper().WriteToError(e.what());
	}

	return { trakDir, iniFile };
}

DatabaseManager::DatabaseManager(QObject* parent) 
: QObject(parent), sqliteManager(parent)
{
	timeStamp = ServiceHelper().timestamp();
	//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);

	rtManager.GetVal("INSTALL_DIR", REG_SZ, (char*)buffer, size);
	std::string installDir(buffer);

	QSettings ini(installDir.append("\\service.ini").data(), QSettings::IniFormat, this);
	ini.sync();
	testingDBManager = ini.value("DEVELOPMENT/testingDBManager", 0).toBool();

	ini.beginGroup("API");
	queryLimit = ini.value("NumberOfQueries", 10).toInt();
	apiUsername = ini.value("Username", "").toString();
	apiPassword = ini.value("Password", "").toString();
	apiKey = ini.value("apiKey", "").toString();
	ini.endGroup();

	ini.beginGroup("SYSTEM");
	databaseDriver = ini.value("databaseDriver", "").toString();
	ini.endGroup();

	if (testingDBManager)
		apiUrl = ini.value("DEVELOPMENT/URL", "http://localhost/webapi/public/api/portals/exec_query").toString();
	else
		apiUrl = ini.value("API/URL", "http://localhost/webapi/public/api/portals/exec_query").toString();

	LOG << apiUrl;

	LOG << "init db manager";
	
	targetApp = "";
	requestRunning = false;
	for (auto i = databaseTablesChecked.cbegin(); i != databaseTablesChecked.cend(); ++i)
	{
		try {
			if(rtManager.GetVal(i.key().toStdString().append("Checked").c_str(), REG_DWORD, (DWORD *)&databaseTablesChecked[i.key()], sizeof(DWORD)))
				rtManager.SetVal(i.key().toStdString().append("Checked").c_str(), REG_DWORD, (DWORD *)&databaseTablesChecked[i.key()], sizeof(DWORD));
		}
		//catch (std::exception& e)
		catch(const HenchmanServiceException& e)
		{
			ServiceHelper().WriteToError(e.what());

		}
	}

	netManager = new QNetworkAccessManager(this->parent());
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	//connect(netManager, &QNetworkAccessManager::finished, this->parent(), &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this->parent());
	
	if (isInternetConnected())
		authenticateSession();
}

DatabaseManager::~DatabaseManager() 
{
	LOG << "Deleting DatabaseManager";

	performCleanup();
}

int DatabaseManager::authenticateSession(const QString& url) {
	QString placeholder;
	if (url.isEmpty()) {
		placeholder = apiUrl;
		placeholder = placeholder.slice(0, apiUrl.lastIndexOf("/")) + "/auth/key";
	}
	else
		placeholder = url;
	return makePostRequest(placeholder);
}

void DatabaseManager::loadTrakDetailsFromRegistry()
{
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal("APP_NAME", REG_SZ, (TCHAR*)buffer, size);
	trakType = buffer;

	RegistryManager::CRegistryManager rtManagerCustomer(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + trakType + "\\Customer").data());

	size = 1024;
	rtManagerCustomer.GetVal("trakID", REG_SZ, (TCHAR*)buffer, size);
	//string trakId = RegistryManager::GetStrVal(hKey, "trakID", REG_SZ);
	trakId = buffer;

	size = 1024;
	rtManagerCustomer.GetVal("ID", REG_SZ, (TCHAR*)buffer, size);
	//string custId = RegistryManager::GetStrVal(hKey, "ID", REG_SZ);
	std::string strCustId(buffer);
	custId = std::stoi(strCustId);

	size = 1024;
	rtManagerCustomer.GetVal(trakId.data(), REG_SZ, (TCHAR*)buffer, size);
	//string idNum = RegistryManager::GetStrVal(hKey, trakId.data(), REG_SZ);
	std::string idNum(buffer);
	trakIdNum = idNum.data();

	trakId.resize(trakId.size() - 2);
	trakId.append("Id");
}

bool DatabaseManager::isInternetConnected()
{
	QTcpSocket* sock = new QTcpSocket(this->parent());
	sock->connectToHost("www.google.com", 80);
	bool connected = sock->waitForConnected(30000);//ms
	sock->disconnectFromHost();
	/*if (!connected)
	{
		sock->abort();
	}
	else {
		sock->close();
	}*/
	sock->deleteLater();
	sock = nullptr;
	return connected;
}

int indentCount = 0;
std::string DatabaseManager::parseData(QJsonArray array)
{
	stringstream dataRes;
	if (array.count() <= 0) {
		return "";
	}
	for (const auto& result : array) {
		QString res = "";
		if (result.isString())
		{
			res += result.toString();
		}
		if (result.isObject())
		{	
			res += QString::fromStdString(parseData(result.toObject()));
		}
		if (result.isArray())
		{
			res += QString::fromStdString(parseData(result.toArray()));
		}
		dataRes << "[ " << res.toStdString() << "]" << endl;
		continue;
	}

	return dataRes.str();
}

std::string DatabaseManager::parseData(QJsonObject object)
{
	
	stringstream dataRes;
	if (object.keys().count() <= 0) {
		return "";
	}
	for (auto& key : object.keys()) {
		QString res = "";
		QJsonValue value = object.value(key);
		/*if(indentCount)
			res.append("\n");*/
		for (int i = 0; i < indentCount; ++i) {
			res.append("\t");
			//res += "\t";
		}
		res.append(" - " + key + ": ");
		if (value.isString())
		{
			res += value.toString();
		}
		else if (value.isDouble()) {
			res += QString::number(value.toDouble());
		}
		else if (value.isObject())
		{
			//res = "\n";
			//res.push_front("\n");
			indentCount++;
			res += "{";
			res.append(parseData(value.toObject()));
			indentCount--;
			res += "}";
		} 
		else if (value.isArray())
		{
			indentCount++;
			res += "\n\t";
			res.append(parseData(value.toArray()));
			indentCount--;
		}
		else {
			res += value.toString();
		}
		
		dataRes << res.toStdString() << "" << endl;

		continue;
	}
	return dataRes.str();
}

int retryCount = 0;

int DatabaseManager::makeGetRequest(const QString& url, const QStringMap& queryMap, QJsonDocument* results)
{
	int result = 0;
	QEventLoop loop(this->parent());

	// Generate auth header for request.
	LOG << url;
	// Create network request object.
	request.setUrl(QUrl(url));
	request.setRawHeader("Authorization", "Bearer " + apiKey.toLocal8Bit());
	request.setRawHeader("Content-Type", "application/json");


	ServiceHelper().WriteToCustomLog(
		"Making query to: " + url.toStdString(),
		timeStamp[0] + "-queries");


	QNetworkReply* reply = restManager->get(request, this, [this, &result, &results](QRestReply& reply) {
		LOG << "networkrequested";
		try {
			qDebug() << reply.error();
			if (reply.error() != QNetworkReply::NoError) {
				throw HenchmanServiceException("A Network error has occured: " + reply.errorString().toStdString());
			}

			ServiceHelper().WriteToLog((reply.isHttpStatusSuccess() ? "Request was successful: " : "An HTTP error has occured: ") + std::to_string(reply.httpStatus()) + " \"" + reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString() + "\"");
			ServiceHelper().WriteToLog((string)"Parsing Response");

			QByteArray jsonRes = reply.readBody();

			int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
			int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
			LOG << "starting index: " << startingIndex << " ending index: " << endingIndex;

			optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);
			if (!json) {
				ServiceHelper().WriteToLog((string)"Recieved empty data or failed to parse JSON.");
				return;
			}
			string parsedVal;
			if (json->isArray()) {
				parsedVal = parseData(json->array());
			}
			else if (json->isObject()) {
				//QJsonObject retVal = json->object();
				parsedVal = parseData(json->object());
			}
			indentCount = 0;

			ServiceHelper().WriteToCustomLog("Webportal response: \n" + parsedVal, timeStamp[0] + "-queries");
			if (results)
				json.value().swap(*results);

			if (!result)
				//result = reply.isSuccess();
				result = reply.isSuccess();

			if (reply.isSuccess())
			{
				LOG << "network request success";
			}
			else {
				LOG << "network request failed";
			}
			reply.networkReply()->close();
		}
		catch (exception& e) {
			ServiceHelper().WriteToError(e.what());
			reply.networkReply()->abort();
			result = 0;

		}
		//reply.networkReply()->finished();

		});

	qDebug() << "Reply Get is finished? " << reply->isFinished();

	//netManager->finished(reply);
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
	qDebug() << "Reply Get is finished? " << reply->isFinished();
	//QThread::sleep(1);
	return result;
}

int DatabaseManager::makePostRequest(const QString& url, const QStringMap& queryMap, const QJsonObject& body, QJsonDocument* results)
{		
	int result = 0;
	QEventLoop loop(this->parent());

	// Generate auth header for request.
	LOG << url;
	// Create network request object.
	request.setUrl(QUrl(url));
	request.setRawHeader("Authorization", "Bearer " + apiKey.toLocal8Bit());
	request.setRawHeader("Content-Type", "application/json");

	

	//data.fromVariantMap();

	/*QJsonObject body;
	body["data"] = body["data"];*/
	/*data["values"] = */
	//data["sql"] = query["query"];
	
	QJsonDocument doc(body);

	ServiceHelper().WriteToCustomLog(
		"Making Post request to: " + url.toStdString() +
		"\nRunning query number : " + queryMap["number"].toStdString() +
		"\nquery : " + doc.toJson().toStdString(),
		timeStamp[0] + "-queries");

	QNetworkReply* reply = restManager->post(request, doc, this, [this, &result, &results, &loop](QRestReply& reply) {
		LOG << "networkrequested";
		try {
			qDebug() << reply.error();
			if (reply.error() == QNetworkReply::OperationCanceledError && retryCount < 3) {
				throw HenchmanServiceException(reply.errorString().toStdString());
			}
			if (reply.error() != QNetworkReply::NoError) {
				throw HenchmanServiceException("A Network error has occured: " + reply.errorString().toStdString());
			}
			
			ServiceHelper().WriteToLog((reply.isHttpStatusSuccess() ? "Request was successful: " : "An HTTP error has occured: ") + std::to_string(reply.httpStatus()) + " \"" + reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString() + "\"");
			ServiceHelper().WriteToLog((string)"Parsing Response");

			QByteArray jsonRes = reply.readBody();

			int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
			int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
			LOG << "starting index: " << startingIndex << " ending index: " << endingIndex;

			optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);
			if (!json) {
				ServiceHelper().WriteToLog((string)"Recieved empty data or failed to parse JSON.");
				return;
			}
			//string parsedVal;
			//if (json->isArray()) {
			//	parsedVal = parseData(json->array());
			//}
			//else if (json->isObject()) {
			//	QJsonObject retVal = json->object();
			//	/*if (retVal.contains("error") && retVal.find("error").value().isObject() && retVal.find("error").value().toObject().find("status").value().toString() == "23000")
			//		result = 1;*/
			//	parsedVal = parseData(json->object());
			//}
			indentCount = 0;

			ServiceHelper().WriteToCustomLog("Webportal response: \n" + json->toJson().toStdString(), timeStamp[0] + "-queries");
			if (results)
				json.value().swap(*results);

			if (!result)
				//result = reply.isSuccess();
				result = reply.isSuccess();

			if (reply.isSuccess())
			{
				LOG << "network request success";
			}
			else {
				LOG << "network request failed";
			}
			reply.networkReply()->close();
		}
		catch (exception& e) {
			std::string error(e.what());
			/*if (error == "QNetworkReply::OperationCanceledError" && retryCount < 3) {
				result = makePostRequest(url, queryMap, body, results);
				if (result) {
					reply.networkReply()->close();
					return;
				}
			}*/
			ServiceHelper().WriteToError(error);
			reply.networkReply()->abort();
			result = 0;

		}
		//reply.networkReply()->finished();
		//QTimer::singleShot(1, reply.networkReply(), &QNetworkReply::finished);
		});

	qDebug() << "Reply post is finished? " << reply->isFinished();

	//netManager->finished(reply);
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
	qDebug() << "Reply post is finished? " << reply->isFinished();

	//reply->deleteLater();
	retryCount = 0;
	//QThread::sleep(1);
	return result;
}

int DatabaseManager::makePatchRequest(const QString& url, const QStringMap& queryMap, const QJsonObject& body, QJsonDocument* results)
{
	int result = 0;
	QEventLoop loop(this->parent());

	// Generate auth header for request.
	LOG << url;
	// Create network request object.
	request.setUrl(QUrl(url));
	request.setRawHeader("Authorization", "Bearer " + apiKey.toLocal8Bit());
	request.setRawHeader("Content-Type", "application/json");

	//data.fromVariantMap();

	/*QJsonObject body;
	body["data"] = body["data"];*/
	/*data["values"] = */
	//data["sql"] = query["query"];

	QJsonDocument doc(body);

	ServiceHelper().WriteToCustomLog(
		"Making query to: " + url.toStdString() +
		"\nRunning query number : " + queryMap["number"].toStdString() +
		"\nquery : " + doc.toJson().toStdString(),
		timeStamp[0] + "-queries");


	QNetworkReply* reply = restManager->patch(request, doc, this, [this, &result, &results](QRestReply& reply) {
		LOG << "networkrequested";
		try {
			qDebug() << reply.error();
			if (reply.error() != QNetworkReply::NoError) {
				throw HenchmanServiceException("A Network error has occured: " + reply.errorString().toStdString());
			}

			ServiceHelper().WriteToLog((reply.isHttpStatusSuccess() ? "Request was successful: " : "An HTTP error has occured: ") + std::to_string(reply.httpStatus()) + " \"" + reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString() + "\"");
			ServiceHelper().WriteToLog((string)"Parsing Response");

			QByteArray jsonRes = reply.readBody();

			int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
			int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
			LOG << "starting index: " << startingIndex << " ending index: " << endingIndex;

			optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);
			if (!json) {
				ServiceHelper().WriteToLog((string)"Recieved empty data or failed to parse JSON.");
				return;
			}
			//string parsedVal;
			//if (json->isArray()) {
			//	parsedVal = parseData(json->array());
			//}
			//else if (json->isObject()) {
			//	QJsonObject retVal = json->object();
			//	/*if (retVal.contains("error") && retVal.find("error").value().isObject() && retVal.find("error").value().toObject().find("status").value().toString() == "23000")
			//		result = 1;*/
			//	parsedVal = parseData(json->object());
			//}
			indentCount = 0;

			ServiceHelper().WriteToCustomLog("Webportal response: \n" + json->toJson().toStdString(), timeStamp[0] + "-queries");
			if (results)
				json.value().swap(*results);

			if (!result)
				//result = reply.isSuccess();
				result = reply.isSuccess();

			if (reply.isSuccess())
			{
				LOG << "network request success";
			}
			else {
				LOG << "network request failed";
			}
			reply.networkReply()->close();
		}
		catch (exception& e) {
			ServiceHelper().WriteToError(e.what());
			reply.networkReply()->abort();
			result = 0;

		}
		//reply.networkReply()->finished();

		});

	qDebug() << "Reply patch is finished? " << reply->isFinished();

	//netManager->finished(reply);
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
	qDebug() << "Reply patch is finished? " << reply->isFinished();
	//QThread::sleep(1);
	return result;
}

int DatabaseManager::makeDeleteRequest(const QString& url, const QStringMap& queryMap, const QJsonObject& body, QJsonDocument* results)
{
	int result = 0;
	QEventLoop loop(this->parent());

	// Generate auth header for request.
	LOG << url;
	// Create network request object.
	QUrl targetUrl(url);
	request.setRawHeader("Authorization", "Bearer " + apiKey.toLocal8Bit());
	request.setRawHeader("Content-Type", "application/json");

	//data.fromVariantMap();

	/*QJsonObject body;
	body["data"] = body["data"];*/
	/*data["values"] = */
	//data["sql"] = query["query"];

	QJsonDocument doc(body);
	QUrlQuery query;
	query.addQueryItem("target", doc.toJson().toBase64());

	targetUrl.setQuery(query);

	request.setUrl(targetUrl);


	ServiceHelper().WriteToCustomLog(
		("Making query to: " + targetUrl.toString() +
		"\nRunning query number : " + queryMap["number"] +
		"\nquery : " + doc.toJson()).toStdString(),
		timeStamp[0] + "-queries");


	QNetworkReply* reply = restManager->deleteResource(request, this, [this, &result, &results](QRestReply& reply) {
		LOG << "networkrequested";
		try {
			qDebug() << reply.error();
			if (reply.error() != QNetworkReply::NoError) {
				throw HenchmanServiceException("A Network error has occured: " + reply.errorString().toStdString());
			}

			ServiceHelper().WriteToLog((reply.isHttpStatusSuccess() ? "Request was successful: " : "An HTTP error has occured: ") + std::to_string(reply.httpStatus()) + " \"" + reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString() + "\"");
			ServiceHelper().WriteToLog((string)"Parsing Response");

			QByteArray jsonRes = reply.readBody();

			int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
			int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
			LOG << "starting index: " << startingIndex << " ending index: " << endingIndex;

			optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);
			if (!json) {
				ServiceHelper().WriteToLog((string)"Recieved empty data or failed to parse JSON.");
				return;
			}
			//string parsedVal;
			//if (json->isArray()) {
			//	parsedVal = parseData(json->array());
			//}
			//else if (json->isObject()) {
			//	QJsonObject retVal = json->object();
			//	/*if (retVal.contains("error") && retVal.find("error").value().isObject() && retVal.find("error").value().toObject().find("status").value().toString() == "23000")
			//		result = 1;*/
			//	parsedVal = parseData(json->object());
			//}
			indentCount = 0;

			ServiceHelper().WriteToCustomLog("Webportal response: \n" + json->toJson().toStdString(), timeStamp[0] + "-queries");
			if (results)
				json.value().swap(*results);

			if (!result)
				//result = reply.isSuccess();
				result = reply.isSuccess();

			if (reply.isSuccess())
			{
				LOG << "network request success";
			}
			else {
				LOG << "network request failed";
			}
			reply.networkReply()->close();
		}
		catch (exception& e) {
			ServiceHelper().WriteToError(e.what());
			reply.networkReply()->abort();
			result = 0;

		}
		//reply.networkReply()->finished();

		});

	qDebug() << "Reply patch is finished? " << reply->isFinished();

	//netManager->finished(reply);
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
	qDebug() << "Reply patch is finished? " << reply->isFinished();
	//QThread::sleep(1);
	return result;
}

int DatabaseManager::makeNetworkRequest(const QString &url, QStringMap &query, QJsonDocument *results)
{
	int result = 0;
	QEventLoop loop(this->parent());

	// Generate auth header for request.
	//QString concatenated = apiUsername+":"+apiPassword;
	//QByteArray credentials = concatenated.toLocal8Bit().toBase64();
	//QString headerData = "Basic " + credentials;
	LOG << url;
	// Create network request object.
	request.setUrl(QUrl(url));
	request.setRawHeader("Authorization", "Bearer "+apiKey.toLocal8Bit());
	request.setRawHeader("Content-Type", "application/json");
	
	//QJsonObject data;
	
	//data.fromVariantMap();

	QJsonObject body;
	body["data"] = query["data"];
	/*data["values"] = */
	//data["sql"] = query["query"];
	QJsonDocument doc(body);

	ServiceHelper().WriteToCustomLog(
		"Making query to: " +url.toStdString() + 
		"\nRunning query number : " + query["number"].toStdString() + 
		"\nquery : " + doc.toJson().toStdString(), 
		timeStamp[0]+ "-queries");

	QNetworkReply* reply = restManager->post(request, doc, this, [this, &result, &results](QRestReply& reply) {
		LOG << "networkrequested";
		try {
			qDebug() << reply.networkReply()->request().headers().toMultiMap();
			qDebug() << reply.networkReply()->headers().toListOfPairs();
			if (reply.error() != QNetworkReply::NoError) {
				// HenchmanServiceException
				throw HenchmanServiceException("A Network error has occured: " + reply.errorString().toStdString());
				//ServiceHelper().WriteToError("A Network error has occured: " + reply.errorString().toStdString());
				//return;
			}
			int status = reply.httpStatus();
			LOG << status;
			QString reason = reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

			if (!reply.isHttpStatusSuccess()) {
				ServiceHelper().WriteToLog("An HTTP error has occured: " + to_string(status) + " \"" + reason.toStdString() + "\"");
			}

			if (reply.isHttpStatusSuccess()) {
				ServiceHelper().WriteToLog("Request was successful : " + to_string(status) + " \"" + reason.toStdString() + "\"");
			}
			ServiceHelper().WriteToLog((string)"Parsing Response");

			QByteArray jsonRes = reply.readBody();

			int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
			int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
			LOG << "starting index: " << startingIndex << " ending index: " << endingIndex;

			optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);
			if (!json) {
				ServiceHelper().WriteToLog((string)"Recieved empty data or failed to parse JSON.");
				return;
			}
			string parsedVal;
			if (json->isArray()) {
				parsedVal = parseData(json->array());
			}
			else if (json->isObject()) {
				QJsonObject retVal = json->object();
				/*if (retVal.contains("error") && retVal.find("error").value().isObject() && retVal.find("error").value().toObject().find("status").value().toString() == "23000")
					result = 1;*/
				parsedVal = parseData(json->object());
			}
			indentCount = 0;

			ServiceHelper().WriteToCustomLog("Webportal response: \n" + parsedVal, timeStamp[0] + "-queries");
			if (results)
				json.value().swap(*results);
			
			if(!result)
				result = reply.isSuccess();

			if (reply.isSuccess())
			{
				LOG << "network request success";
			}
			else {
				LOG << "network request failed";
			}
			reply.networkReply()->close();
		}
		catch (exception& e) {
			ServiceHelper().WriteToError(e.what());
			reply.networkReply()->abort();

		}
		reply.networkReply()->finished();
		
		});

	//connect(reply, &QNetworkReply::finished, restManager->networkAccessManager(), &QNetworkAccessManager::finished);
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	//connect(&loop, &QEventLoop::quit, restManager->networkAccessManager(), &QNetworkAccessManager::finished);
	loop.exec();
	//QThread::sleep(1);
	return result;
}

//int queryRemoteDatabase(string url, string query)
//{
//
//}

// Misc Syncs
int DatabaseManager::addToolsIfNotExists()
{
	LOG << "Adding Tools to Webportal";
	QString targetKey = "tools";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM tools");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE  '% tools%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Tools");
	string query = 
		"SELECT * from tools ORDER BY id DESC LIMIT " + 
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);


	std::string colQuery =
		"SHOW COLUMNS from tools";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "tools";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year"};
	QStringList uniqueIndexCols = { "custId", "toolId", "stockcode", "PartNo" };

	int hadToolId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_"+ uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(", ").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();
	
	for (auto &result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		result["toolId"] = result["id"];

		//QString results[2];

		//processKeysAndValues(result, results);

		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;

		body["data"] = data;
		
		QJsonDocument reply;

		if (makePostRequest(apiUrl + "/tools", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}
		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

		//if (makeNetworkRequest(apiUrl+"/tools", result, &reply)) {
		//	if (!reply.isObject())
		//		continue;
		//	QJsonObject result = reply.object();
		//	if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
		//		LOG << result["result"].toString().toStdString();
		//		databaseTablesChecked[targetKey]++;
		//		continue;
		//	}
		//	LOG << "No rows were altered on db";
		//	databaseTablesChecked[targetKey]++;
		//	//databaseTablesChecked[targetKey] += queryLimit;
		//	//Sleep(100);
		//	//break;
		//	continue;
		//}

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);
	//performCleanup();
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE  '% tools%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::addUsersIfNotExists()
{
	LOG << "Adding Users to Webportal";
	QString targetKey = "users";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM users");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% users%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Users");
	string query =
		"SELECT * from users ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from users";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;


	string tableName = "users";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "userId", "kabId", "cribId", "scaleId"};

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_"+ uniqueIndexCols.join("_").toStdString() +" ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;

		QStringMap res;
		res["id"] = result["id"];
		//result.remove("id");

		//qDebug() << result;

		if (trakId != "kabId") {
			result["kabId"] = "";
			result["accessCountKab"] = 0;
		}
		if (trakId != "cribId") {
			result["cribId"] = "";
			result["accessCountCrib"] = 0;
		}
		if (trakId != "scaleId") {
			result["scaleId"] = "";
			result["accessCountPorta"] = 0;
		}
		
		if (result.value(trakId.data()).isEmpty())
			result[trakId.data()] = trakIdNum;


		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;

		body["data"] = data;

		QJsonDocument reply;
		
		if (makePostRequest(apiUrl + "/users", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
		
		//if (makeNetworkRequest(apiUrl+"/users", result, &reply)) {
		//	if (!reply.isObject())
		//		continue;
		//	QJsonObject result = reply.object();
		//	LOG << result["result"].toString().toStdString();
		//	if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
		//		databaseTablesChecked[targetKey]++;
		//		continue;
		//	}
		//	LOG << "No rows were altered on db";
		//	databaseTablesChecked[targetKey]++;
		//	continue;
		//	//break;
		//}

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% users%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::addEmployeesIfNotExists()
{
	LOG << "Adding Employees to Webportal";
	QString targetKey = "employees";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM employees");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% employees%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Employees");
	string query =
		"SELECT * from employees ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from employees";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "employees";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "userId"};

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		//qDebug() << result;
		QStringMap res;
		res["id"] = result["id"];

		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (makePostRequest(apiUrl + "/employees", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

		//if (makeNetworkRequest(apiUrl, res, &reply)) {
		//	if (!reply.isObject())
		//		continue;
		//	QJsonObject result = reply.object();
		//	LOG << result["result"].toString().toStdString();
		//	if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
		//		databaseTablesChecked[targetKey]++;
		//		continue;
		//	}
		//	LOG << "No rows were altered on db";
		//	databaseTablesChecked[targetKey]++;
		//	//databaseTablesChecked[targetKey] += queryLimit;
		//	//Sleep(100);
		//	continue;
		//	//break;
		//}

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% employees%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::addJobsIfNotExists()
{
	LOG << "Adding Jobs to Webportal";
	QString targetKey = "jobs";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM jobs");
	vector colsCheck = ExecuteTargetSql("SHOW KEYS FROM jobs WHERE Key_name = 'PRIMARY'");
	QString indexingCol = colsCheck[1].value("Column_name");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey] || colsCheck.size() <= 1 || !colsCheck[0].value("success").toInt())
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% jobs%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Jobs");
	string query =
		"SELECT * from jobs ORDER BY "+ indexingCol.toStdString() + " ASC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from jobs";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "jobs";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt"};
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "trailId", "description", "remark"};

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result[indexingCol];

		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (makePostRequest(apiUrl + "/jobs", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

		//QJsonDocument reply;
		//if (makeNetworkRequest(apiUrl, res, &reply)) {
		//	if (!reply.isObject())
		//		continue;
		//	QJsonObject result = reply.object();
		//	LOG << result["result"].toString().toStdString();
		//	if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
		//		databaseTablesChecked[targetKey]++;
		//		continue;
		//	}
		//	LOG << "No rows were altered on db";
		//	databaseTablesChecked[targetKey]++;
		//	//databaseTablesChecked[targetKey] += queryLimit;
		//	//Sleep(100);
		//	continue;
		//	//break;
		//}

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey] || colsCheck.size() <= 1 || !colsCheck[0].value("success").toInt())
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% jobs%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}

// KabTRAK Syncs
int DatabaseManager::addKabsIfNotExists()
{
	LOG << "Adding Kabs to Webportal";
	QString targetKey = "kabs";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM itemkabs");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);

	std::string trakDir;
	std::string iniFile;
	QString trakModelNumber;
	if (rowCheck.size() > 1 && rowCheck[1][rowCheck[1].firstKey()].toInt() < 1) {

		rtManager.GetVal("TRAK_DIR", REG_SZ, (char*)buffer, size);
		trakDir = buffer;
		size = 1024;

		rtManager.GetVal("INI_FILE", REG_SZ, (char*)buffer, size);
		iniFile = buffer;
		size = 1024;

		QSettings ini((trakDir + "\\" + iniFile).data(), QSettings::IniFormat, this);
		ini.sync();
		trakModelNumber = ini.value("Customer/ModelNumber", "").toString();
		std::string kabId = ini.value("Customer/kabID", trakIdNum).toString().toStdString();
		std::string description = ini.value("Unit/Description", "KT-" + trakIdNum.last(3)).toString().toStdString();
		std::string serialNo = ini.value("Unit/SerialNo", "KT-" + trakIdNum.last(3)).toString().toStdString();
		std::string cols = "custId";
		std::string vals = "'" + std::to_string(custId) + "'";
		if (!kabId.empty()) {
			cols += ", kabId";
			vals += ", '" + kabId + "'";
		}
		if (!description.empty()) {
			cols += ", description";
			vals += ", '" + description + "'";
		}
		if (!serialNo.empty()) {
			cols += ", serialNumber";
			vals += ", '" + serialNo + "'";
		}
		if (!trakModelNumber.isEmpty()) {
			cols += ", modelNumber";
			vals += ", '" + trakModelNumber.toStdString() + "'";
		}
		ExecuteTargetSql("INSERT INTO itemkabs (" + cols + ") VALUES (" + vals + ")");

		databaseTablesChecked[targetKey]--;
	}

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkabs%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}

	if (databaseTablesChecked[targetKey] < 0)
		databaseTablesChecked[targetKey]++;

	ServiceHelper().WriteToLog("Exporting Kabs");
	string query =
		"SELECT * from itemkabs ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() > 0) {
		rtManager.GetVal("TRAK_DIR", REG_SZ, (char*)buffer, size);
		trakDir = buffer;
		size = 1024;

		rtManager.GetVal("INI_FILE", REG_SZ, (char*)buffer, size);
		iniFile = buffer;
		//size = 1024;

		QSettings ini((trakDir + "\\" + iniFile).data(), QSettings::IniFormat, this);
		ini.sync();
		trakModelNumber = ini.value("Customer/ModelNumber", "").toString();
	}

	std::string colQuery =
		"SHOW COLUMNS from itemkabs";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "itemkabs";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "kabId" };

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	QStringList ensureValForTargetCols = { "description", "serialNumber", "modelNumber" };

	for (auto &result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QJsonObject data;
		map<string, string> toolData;
		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (ensureValForTargetCols.contains(key) && (val.isEmpty() || val== "''")) {
				if (key == "description") {
					val = "KT-" + trakIdNum.last(3);
				}
				if (key == "serialNumber") {
					val = "KT-" + trakIdNum.last(3);
				}
				if (key == "modelNumber" && !trakModelNumber.isEmpty()) {
					val = trakModelNumber;
				}
			}
			data[key] = val;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (makePostRequest(apiUrl + "/kabtrak", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkabs%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::addDrawersIfNotExists()
{
	LOG << "Adding Drawers to Webportal";
	QString targetKey = "kabDrawers";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM itemkabdrawers");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkabdrawers%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kab Drawers");
	string query =
		"SELECT * from itemkabdrawers ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from itemkabdrawers";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "itemkabdrawers";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "kabId", "drawerCode"};

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	for (auto &result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (makePostRequest(apiUrl + "/kabtrak/drawers", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numDrawersChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);*/

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkabdrawers%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::addToolsInDrawersIfNotExists()
{
	LOG << "Adding Kab Tools to Webportal";
	QString targetKey = "kabDrawerBins";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM itemkabdrawerbins");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey]) {
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkabdrawerbins%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kab tools in bins");
	string query =
		"SELECT i.*, t.id as toolId from itemkabdrawerbins AS i LEFT JOIN tools AS t ON t.PartNo LIKE i.itemId OR t.stockcode LIKE i.itemId ORDER BY i.id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from itemkabdrawerbins";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "itemkabdrawerbins";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "kabId", "toolId", "itemId", "drawerNum", "toolNumber"};

	int hadToolId = 0;
	
	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	for (auto & result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		qDebug() << body;

		QJsonDocument reply;
		qDebug() << "Tool: itemId: " << data.value("itemId") << " toolId:" << data.value("toolId") << " drawerNum: " << data.value("drawerNum") << " toolNumber: " << data.value("toolNumber");
		if (data.value("drawerNum").toString() == "7" && data.value("toolNumber").toString() == "79") {
			qDebug() << "forced breakout";
		}

		if (makePostRequest(apiUrl + "/kabtrak/tools", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}
	
	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey]) {
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkabdrawerbins%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::createKabtrakTransactionsTable() {
	LOG << "Adding Kabtrak Transactions Table to Service SQLite Database";

	std::string tableName = "kabemployeeitemtransactions";
	std::string colQuery =
		"SHOW COLUMNS from " + tableName;
	std::vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::vector<stringmap> results;

	sqliteManager.ExecQuery(
		"PRAGMA table_info(" + tableName + ")",
		results
	);

	if (colQueryResults.size() <= 1 || !results.empty())
		return 0;

	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "kabId", "drawerNum", "toolNum", "userId", "trailId", "itemId", "transDate", "transTime", "transType", "toolId"};

	int hadToolId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();
	return 1;
}
// CribTRAK Syncs
int DatabaseManager::addCribsIfNotExists()
{
	LOG << "Adding Cribs to Webportal";
	QString targetKey = "cribs";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM cribs");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);

	std::string trakDir;
	std::string iniFile;
	QString trakModelNumber;
	if (rowCheck.size() > 1 && rowCheck[1][rowCheck[1].firstKey()].toInt() < 1) {

		rtManager.GetVal("TRAK_DIR", REG_SZ, (char*)buffer, size);
		trakDir = buffer;
		size = 1024;

		rtManager.GetVal("INI_FILE", REG_SZ, (char*)buffer, size);
		iniFile = buffer;
		size = 1024;

		QSettings ini((trakDir + "\\" + iniFile).data(), QSettings::IniFormat, this);
		ini.sync();
		trakModelNumber = ini.value("Customer/ModelNumber", "").toString();
		std::string cribId = ini.value("Customer/cribID", trakIdNum).toString().toStdString();
		std::string description = ini.value("Unit/Description", "CT-" + trakIdNum.last(3)).toString().toStdString();
		std::string serialNo = ini.value("Customer/cribSerial", "CT-" + trakIdNum.last(3)).toString().toStdString();
		std::string cols = "custId";
		std::string vals = "'" + std::to_string(custId) + "'";
		if (!cribId.empty()) {
			cols += ", cribId";
			vals += ", '" + cribId + "'";
		}
		if (!description.empty()) {
			cols += ", description";
			vals += ", '" + description + "'";
		}
		if (!serialNo.empty()) {
			cols += ", serialNumber";
			vals += ", '" + serialNo + "'";
		}
		if (!trakModelNumber.isEmpty()) {
			cols += ", modelNumber";
			vals += ", '" + trakModelNumber.toStdString() + "'";
		}
		ExecuteTargetSql("INSERT INTO cribs (" + cols + ") VALUES (" + vals + ")");

		databaseTablesChecked[targetKey]--;
	}

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% cribs%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}

	if (databaseTablesChecked[targetKey] < 0)
		databaseTablesChecked[targetKey]++;

	ServiceHelper().WriteToLog("Exporting CribTRAKS from crib");
	string query =
		"SELECT * from cribs ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() > 0) {
		rtManager.GetVal("TRAK_DIR", REG_SZ, (char*)buffer, size);
		trakDir = buffer;
		size = 1024;

		rtManager.GetVal("INI_FILE", REG_SZ, (char*)buffer, size);
		iniFile = buffer;
		//size = 1024;

		QSettings ini((trakDir + "\\" + iniFile).data(), QSettings::IniFormat, this);
		ini.sync();
		trakModelNumber = ini.value("Customer/ModelNumber", "").toString();
	}

	std::string colQuery =
		"SHOW COLUMNS from cribs";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "cribs";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId"};

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	QStringList ensureValForTargetCols = { "description", "serialNumber", "modelNumber" };
	
	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QJsonObject data;
		map<string, string> toolData;
		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (ensureValForTargetCols.contains(key) && (val.isEmpty() || val == "''")) {
				if (key == "description") {
					val = "CT-" + trakIdNum.last(3);
				}
				if (key == "serialNumber") {
					val = "CT-" + trakIdNum.last(3);
				}
				if (key == "modelNumber" && !trakModelNumber.isEmpty()) {
					val = trakModelNumber;
				}
			}
			data[key] = val;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (makePostRequest(apiUrl + "/cribtrak", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% cribs%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::addCribToolLocationIfNotExists()
{
	LOG << "Adding CribToolLocations to Webportal";
	QString targetKey = "toolLocation";
	timeStamp = ServiceHelper().timestamp();
	vector tableCheck = ExecuteTargetSql("show tables like 'cribtoollocation'");
	//qDebug() << tableCheck;
	QString cribtoollocationTable;
	if (tableCheck.size() > 1) {
		cribtoollocationTable = "cribtoollocation";
	}
	else {
		cribtoollocationTable = "cribtoollocations";
	}
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM " + cribtoollocationTable.toStdString());
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% " + cribtoollocationTable.toStdString() + "%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Tool Location from CribTRAK");
	string query =
		"SELECT * from "+ cribtoollocationTable.toStdString() +" ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from " + cribtoollocationTable.toStdString();
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = cribtoollocationTable.toStdString();
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId", "locationId" };
	
	int hadLocationId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "locationId") {
			hadLocationId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadLocationId)
		columns.push_back("locationId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		result["locationId"] = result["id"];

		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (makePostRequest(apiUrl + "/cribtrak/tools/locations", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% " + cribtoollocationTable.toStdString() + "%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::addCribToolsIfNotExists()
{
	LOG << "Adding CribTools to Webportal";
	QString targetKey = "cribtools";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM cribtools");
	vector colsCheck = ExecuteTargetSql("SHOW KEYS FROM cribtools WHERE Key_name = 'PRIMARY'");
	QString indexingCol = colsCheck[1].value("Column_name");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE (SQLString  LIKE '% cribtools %' OR SQLString LIKE '% cribtools%') AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Tools from crib");
	string query =
		"SELECT * from cribtools ORDER BY "+ indexingCol.toStdString() + " DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from cribtools";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "cribtools";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId", "toolId", "barcodeTAG", "itemId"};

	int hadToolId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	for (auto & result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId"))
			continue;
		QStringMap res;

		std::vector<QStringMap> fetchTool;
		if (!result.contains("id")) {
			res["id"] = result["toolId"];
			result["id"] = res["id"];
			fetchTool = ExecuteTargetSql(("SELECT id, PartNo FROM tools WHERE ((PartNo IS NOT NULL OR PartNo <> '') AND PartNo = '" + result["itemId"] + "') OR ((serialNo IS NOT NULL OR serialNo <> '') AND serialNo = '" + result["serialNo"] + "') GROUP BY description ORDER BY id DESC LIMIT 1;").toStdString());
			result["toolId"] = "";
		}
		else {
			res["id"] = result["id"];
			//result["toolId"] = result["id"];
			fetchTool = ExecuteTargetSql(("SELECT id, PartNo FROM tools WHERE ((PartNo IS NOT NULL OR PartNo <> '') AND PartNo = '" + result["itemId"] + "') OR ((serialNo IS NOT NULL OR serialNo <> '') AND serialNo = '" + result["serialNo"] + "') OR (id = '" + result["toolId"] + "') GROUP BY description ORDER BY id DESC LIMIT 1;").toStdString());
		}

		qDebug() << fetchTool;

		if((fetchTool.size() > 1 && !fetchTool[1].value("id").isEmpty()) && (!result.contains("toolId") || result.value("toolId").isEmpty() || result.value("toolId") == "''" || result.value("toolId") == "0"))
			result["toolId"] = fetchTool[1].value("id");

		if ((fetchTool.size() > 1 && !fetchTool[1].value("PartNo").isEmpty()) && (result.contains("itemId") && result.value("itemId").isEmpty() || result.value("itemId") == "''")) {
			result["itemId"] = fetchTool[1].value("PartNo");
		}
		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		if (result.contains("nextcalibrationdate")) {
			data["currentcalibrationdate"] = result["nextcalibrationdate"];
			//result.remove("nextcalibrationdate");
		}

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		qDebug() << fetchTool;
		LOG << QJsonDocument(data).toJson();

		if (result.value("id") == "27987") {
			LOG << "breakpoint";
		}

		if (makePostRequest(apiUrl + "/cribtrak/tools", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE (SQLString  LIKE '% cribtools %' OR SQLString LIKE '% cribtools%') AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::addCribToolTransferIfNotExists()
{
	LOG << "Adding CribToolTransfer to Webportal";
	QString targetKey = "tooltransfer";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM tooltransfer");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% tooltransfer%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Tool Transfers from crib");
	string query =
		"SELECT * from tooltransfer ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from tooltransfer";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "tooltransfer";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId", "transferId", "barcodeTAG", "userId", "transfer_userId", "tailId", "transfer_tailId"};

	int hadTransferId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "transferId") {
			hadTransferId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadTransferId)
		columns.push_back("transferId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId"))
			continue;
		QStringMap res;
		res["id"] = result["id"];

		if(!result.contains("transferId"))
			result["transferId"] = result["id"];

		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		LOG << QJsonDocument(body).toJson();

		if (makePostRequest(apiUrl + "/cribtrak/tools/transfer", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey]) {
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% tooltransfer%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::addCribConsumablesIfNotExists()
{
	LOG << "Adding Cribtrak Consumables to Webportal";
	QString targetKey = "cribconsumables";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM cribconsumables");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE (SQLString  LIKE '% cribconsumables %' OR SQLString LIKE '% cribconsumables%') AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");

		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Consumables from crib");
	string query =
		"SELECT * from cribconsumables ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from cribconsumables";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "cribconsumables";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId", "toolId", "barcode", "userId", "tailId"};

	int hadToolId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId")) {
			continue;
		}
		QStringMap res;
		/*if (!result.contains("id")) {
			res["id"] = result["toolId"];
		}
		else {*/
			res["id"] = result["id"];
			//result["toolId"] = result["id"];
		//}
		vector fetchTool = ExecuteTargetSql(("SELECT t.id FROM cribtools AS ct LEFT JOIN tools AS t ON ((t.PartNo IS NOT NULL OR t.PartNo <> '') AND t.PartNo = ct.itemId) OR ((t.serialNo IS NOT NULL OR t.serialNo <> '') AND t.serialNo = ct.serialNo) OR (t.id = ct.toolId) WHERE ct.barcodeTAG LIKE '" + result["barcode"]+"'").toStdString());
		if (fetchTool.size() <= 1) {
			databaseTablesChecked[targetKey]++;
			continue;
		}
		if (!fetchTool[1]["id"].isEmpty())
			result["toolId"] = fetchTool[1]["id"];

		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		/*LOG << QJsonDocument(body).toJson();
		if (data.value("barcodeTAG").toString() == "29600526") {
			LOG << "";
		}*/

		if (makePostRequest(apiUrl + "/cribtrak/consumables", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE (SQLString  LIKE '% cribconsumables %' OR SQLString LIKE '% cribconsumables%') AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::addCribKitsIfNotExists()
{
	LOG << "Adding CribTools to Webportal";
	QString targetKey = "kittools";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM kittools");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE (SQLString  LIKE '% kittools %' OR SQLString LIKE '% kittools%') AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kits from crib");
	string query =
		"SELECT * from kittools ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from kittools";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "kittools";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId", "kitBarcode", "toolId", "itemId", "barcodeTAG"};

	int hadToolId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId"))
			continue;
		QStringMap res;
		res["id"] = result["id"];
		
		vector fetchTool = ExecuteTargetSql(("SELECT t.PartNo as itemId, ct.serialNo, t.id AS toolId FROM cribtools AS ct LEFT JOIN tools AS t ON ((t.PartNo IS NOT NULL OR t.PartNo <> '') AND t.PartNo = ct.itemId) OR ((t.serialNo IS NOT NULL OR t.serialNo <> '') AND t.serialNo = ct.serialNo) OR (t.id = ct.toolId) WHERE ct.barcodeTAG LIKE '" + result["kitBarcode"] + "'").toStdString());
		
		if (fetchTool.size() > 1) {
			if(!fetchTool[1]["itemId"].isEmpty())
				result["itemId"] = fetchTool[1]["itemId"];
			if(!fetchTool[1]["serialNo"].isEmpty() && result["serialNo"].isEmpty())
				result["serialNo"] = fetchTool[1]["serialNo"];
			if (!fetchTool[1]["toolId"].isEmpty())
				result["toolId"] = fetchTool[1]["toolId"];
		}


		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		LOG << QJsonDocument(body).toJson();

		if (makePostRequest(apiUrl + "/cribtrak/kits", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE (SQLString  LIKE '% kittools %' OR SQLString LIKE '% kittools%') AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::createCribtrakTransactionsTable() {
	LOG << "Adding Cribtrak Transactions Table to Service SQLite Database";

	std::string tableName = "cribemployeeitemtransactions";
	std::string colQuery =
		"SHOW COLUMNS from " + tableName;
	std::vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::vector<stringmap> results;

	sqliteManager.ExecQuery(
		"PRAGMA table_info(" + tableName + ")",
		results
	);

	if (colQueryResults.size() <= 1 || !results.empty())
		return 0;

	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId", "toolId", "itemId", "barcode", "trailId", "issuedBy", "returnBy", "transDate", "transTime", "transType"};

	int hadToolId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	return 1;
}

/* TODO
 - upload cribtoollockers
*/

// PortaTRAK Syncs
int DatabaseManager::addPortasIfNotExists()
{
	LOG << "Adding Portas to Webportal";
	QString targetKey = "scales";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM itemscale");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	std::string trakDir;
	std::string iniFile;
	QString trakModelNumber;
	if (rowCheck.size() > 1 && rowCheck[1][rowCheck[1].firstKey()].toInt() < 1) {
		
		rtManager.GetVal("TRAK_DIR", REG_SZ, (char*)buffer, size);
		trakDir = buffer;
		size = 1024;

		rtManager.GetVal("INI_FILE", REG_SZ, (char*)buffer, size);
		iniFile = buffer;
		size = 1024;

		QSettings ini((trakDir + "\\" + iniFile).data(), QSettings::IniFormat, this);
		ini.sync();
		trakModelNumber = ini.value("Customer/ModelNumber", "").toString();
		std::string scaleId = ini.value("Customer/scaleID", trakIdNum).toString().toStdString();
		std::string description = ini.value("Unit/Description", "PT-" + trakIdNum.last(3)).toString().toStdString();
		std::string serialNo = ini.value("Unit/SerialNo", "PT-" + trakIdNum.last(3)).toString().toStdString();
		std::string cols = "custId";
		std::string vals = "'"+std::to_string(custId)+"'";
		if (!scaleId.empty()) {
			cols += ", scaleId";
			vals += ", '" + scaleId+"'";
		}
		if (!description.empty()) {
			cols += ", description";
			vals += ", '" + description + "'";
		}
		if (!serialNo.empty()) {
			cols += ", serialNumber";
				vals += ", '" + serialNo + "'";
		}
		if (!trakModelNumber.isEmpty()) {
			cols += ", modelNumber";
				vals += ", '" + trakModelNumber.toStdString() + "'";
		}
		ExecuteTargetSql("INSERT INTO itemscale (" + cols + ") VALUES (" + vals + ")");

		databaseTablesChecked[targetKey]--;
	}


	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemscale%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}

	if (databaseTablesChecked[targetKey] < 0)
		databaseTablesChecked[targetKey]++;
	
	ServiceHelper().WriteToLog("Exporting PortaTRAKS from itemscale");
	string query =
		"SELECT * from itemscale ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() > 0) {
		rtManager.GetVal("TRAK_DIR", REG_SZ, (char*)buffer, size);
		trakDir = buffer;
		size = 1024;

		rtManager.GetVal("INI_FILE", REG_SZ, (char*)buffer, size);
		iniFile = buffer;
		//size = 1024;

		QSettings ini((trakDir + "\\" + iniFile).data(), QSettings::IniFormat, this);
		ini.sync();
		trakModelNumber = ini.value("Customer/ModelNumber", "").toString();
	}

	std::string colQuery =
		"SHOW COLUMNS from itemscale";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "itemscale";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "scaleId" };

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	QStringList ensureValForTargetCols = { "description", "serialNumber", "modelNumber" };

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QJsonObject data;
		map<string, string> toolData;
		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (ensureValForTargetCols.contains(key) && (val.isEmpty() || val == "''")) {
				if (key == "description") {
					val = "PT-" + trakIdNum.last(3);
				}
				if (key == "serialNumber") {
					val = "PT-" + trakIdNum.last(3);
				}
				if (key == "modelNumber" && !trakModelNumber.isEmpty()) {
					val = trakModelNumber;
				}
			}
			if (val.isEmpty() || val == "''")
				continue;
			data[key] = val;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (makePostRequest(apiUrl + "/portatrak", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemscale%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::addItemKitsIfNotExists()
{
	LOG << "Adding Kits to Webportal";
	QString targetKey = "itemkits";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM itemkits");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkits%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Itemkits from PortaTRAK");
	string query =
		"SELECT * from itemkits ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from itemkits";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "itemkits";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year"};
	QStringList uniqueIndexCols = { "custId", "scaleId", "kitTAG", "kitId"};

	int hadKitId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "kitId") {
			hadKitId = 1;
			column["Type"] = "TEXT";
		}

		if (dates.contains(column.value("Type").toLower()) || column.value("Field") == "userId")
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadKitId)
		columns.push_back("kitId TEXT NOT NULL DEFAULT ''");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId"))
			continue;
		QStringMap res;
		res["id"] = result["id"];

		if (!result.contains("kitId") || result.value("kitId").isEmpty()) {
			result["kitId"] = QString("000").slice(QString::number(custId).length()) + QString::number(custId) + QString("000").slice(result["id"].length()) + result["id"];
		}

		if (!result.contains("userId") || result.value("userId").isEmpty() || result.value("userId") == "0") {
			std::string userIdQuery = "SELECT userId FROM portaemployeeitemtransactions WHERE kitTAG = '" + result.value("kitTAG").toStdString() + "' AND transType = '3' ORDER BY id DESC LIMIT 1";
			vector queryRes = ExecuteTargetSql(userIdQuery);
			if (queryRes.size() > 1 && queryRes[1].contains("userId") && (!queryRes[1].value("userId").isEmpty() || queryRes[1].value("userId") != "0")) {
				result["userId"] = queryRes[1].value("userId");
			}
		}

		qDebug() << result;

		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (makePostRequest(apiUrl + "/portatrak/kit", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkits%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::addKitCategoryIfNotExists()
{
	LOG << "Adding Kit Categories to Webportal";
	QString targetKey = "kitCategory";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM kitcategory");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% kitcategory%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kit Categories from PortaTRAK");
	string query =
		"SELECT * from kitcategory ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from kitcategory";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "kitcategory";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "scaleId", "categoryId"};
	
	int hadCategoryId = 0;
	int hadScaleId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "categoryId") {
			hadCategoryId = 1;
			column["Type"] = "INT";
		}

		if (column.value("Field") == "scaleId") {
			hadScaleId = 1;
			column["Type"] = "TEXT";
		}		

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadCategoryId)
		columns.push_back("categoryId INT NOT NULL DEFAULT 0");

	if(!hadScaleId)
		columns.push_back("scaleId TEXT NOT NULL DEFAULT ''");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;

		QStringMap res;
		res["id"] = result["id"];

		result["categoryId"] = result["id"];

		if (!result.contains("scaleId") || result.value("scaleId").isEmpty())
			result["scaleId"] = trakIdNum;
		
		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (makePostRequest(apiUrl + "/portatrak/kit/category", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% kitcategory%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::addKitLocationIfNotExists()
{
	LOG << "Adding Kit Locations to Webportal";
	QString targetKey = "kitLocation";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM kitlocation");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% kitlocation%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kit Location from PortaTRAK");
	string query =
		"SELECT * from kitlocation ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from kitlocation";
	vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	string tableName = "kitlocation";
	vector<string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "scaleId", "locationId"};

	int hadLocationId = 0;
	int hadScaleId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "locationId") {
			hadLocationId = 1;
			column["Type"] = "INT";
		}

		if (column.value("Field") == "scaleId") {
			hadScaleId = 1;
			column["Type"] = "TEXT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if (!hadLocationId)
		columns.push_back("locationId INT NOT NULL DEFAULT 0");

	if (!hadScaleId)
		columns.push_back("scaleId TEXT NOT NULL DEFAULT ''");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);

	if (isInternetConnected())
		authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		result["locationId"] = result["id"];

		if (!result.contains("scaleId") || result.value("scaleId").isEmpty())
			result["scaleId"] = trakIdNum;

		QJsonObject data;
		map<string, string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (makePostRequest(apiUrl + "/portatrak/kit/location", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% kitlocation%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}

	return 1;
}
int DatabaseManager::createPortatrakTransactionsTable() {
	LOG << "Adding Portatrak Transactions Table to Service SQLite Database";

	std::string tableName = "portaemployeeitemtransactions";
	std::string colQuery =
		"SHOW COLUMNS from " + tableName;
	std::vector colQueryResults = ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::vector<stringmap> results;

	sqliteManager.ExecQuery(
		"PRAGMA table_info(" + tableName + ")",
		results
	);

	if (colQueryResults.size() <= 1 || !results.empty())
		return 0;

	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "scaleId", "kitTAG", "kitId", "userId", "transType", "trailId", "issuedBy", "returnBy", "transDate", "transTime"};

	int hadKitId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "kitId") {
			hadKitId = 1;
			column["Type"] = "TEXT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadKitId)
		columns.push_back("kitId TEXT NOT NULL DEFAULT ''");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	return 1;
}
int DatabaseManager::connectToRemoteDB()
{
	ServiceHelper().WriteToLog(string("Attempting to connect to Remote Database"));
	timeStamp = ServiceHelper().timestamp();
	QString targetSchema;
	QSqlDatabase db;
	bool result = false;
	try {

		//HKEY hKeyLocal = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));
		RegistryManager::CRegistryManager rtManagerAddDB(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\"+ targetApp + "\\Database").c_str());

		/*targetSchema = QString::fromStdString(RegistryManager::GetStrVal(hKeyLocal, "Schema", REG_SZ));
		RegCloseKey(hKeyLocal);*/

		TCHAR buffer[1024] = "\0";
		DWORD size = sizeof(buffer);
		rtManagerAddDB.GetVal("Schema", REG_SZ, (char*)buffer, size);
		targetSchema = buffer;

		LOG << "Checking if database has been previously defined";
		// HenchmanServiceException
		if (!QSqlDatabase::contains(targetSchema))
			throw HenchmanServiceException("Provided schema not valid");
	
		if (apiUrl.trimmed().isEmpty())
			throw HenchmanServiceException("No target Database Url provided");
		
		ServiceHelper().WriteToLog("Creating session to db " + targetSchema.toStdString());
		
		db = QSqlDatabase::database(targetSchema);
		// HenchmanServiceException
		if (!db.open())
			throw HenchmanServiceException("Failed to open DB Connection");

		requestRunning = true;		

		vector<QStringMap> queries;
		QSqlQuery query(db);
		QString queryText = testingDBManager && doNotRunCloudUpdate ? "SHOW TABLES" : "SELECT * FROM cloudupdate WHERE posted = 0 OR posted = 2 ORDER BY DatePosted ASC, id ASC LIMIT " + QString::number(queryLimit);
		LOG << queryText;
		query.prepare(queryText);
		if (!query.exec())
		{
			query.finish();
			ServiceHelper().WriteToLog(string("Closing DB Session"));
			throw HenchmanServiceException("Failed to exec query: " + query.executedQuery().toStdString());
		}
		
		if(query.numRowsAffected() > 0)
			ServiceHelper().WriteToCustomLog("Starting network requests to: " + apiUrl.toStdString(), timeStamp[0] + "-queries");

		if (restManager == nullptr)
			restManager = new QRestAccessManager(netManager, this->parent());

		if (isInternetConnected())
			authenticateSession();

		int count = 0;

		//while (testingDBManager ? count < 5 : query.next())
		while (query.next())
		{
			count++;
			bool skipQuery = false;
			bool retryingQuery = false;
			QStringMap res;
			res["number"] = QString::number(count);

			QString queryType;
			QJsonObject data;
			
			if (parseCloudUpdate) {
				res["id"] = query.value(0).toString();
				res["query"] = query
					.value(2)
					.toString()
					.replace(
						QRegularExpression(
							"(NOW|CURDATE|CURTIME)+",
							QRegularExpression::ExtendedPatternSyntaxOption
						),
						"\'" + query
						.value(3)
						.toString()
						.replace("T", " ")
						//".000Z"
						.replace(QRegularExpression(
							"\....Z",
							QRegularExpression::ExtendedPatternSyntaxOption
						),"") + "\'"
					).replace("()", "").simplified();
				retryingQuery = query.value(4).toInt() == 2;
				ServiceHelper().WriteToCustomLog("Query fetched from database: " + res["query"].toStdString(), timeStamp[0] + "-queries");

#if false
				if (res["query"].contains(";") && res["query"].split(";").size() > 2 && !res["query"].split(";")[1].isEmpty() && !skipQuery) {
					LOG << res["query"].split(";").size();
					res["query"] = res["query"].split(";")[0];
					LOG << res["query"];
					/*skipQuery = true;
					goto parsedQuery;*/
				}
#endif				
				// Check if query contains customer id for verification. Containing custId that is not assigned to trak renders query unsafe.

				LOG << res["query"];
				if (res["query"].contains("insert", Qt::CaseInsensitive)) {
					queryType = "insert";
				}
				else if (res["query"].contains("update", Qt::CaseInsensitive)) {
					queryType = "update";
				}
				else if (res["query"].contains("delete", Qt::CaseInsensitive)) {
					queryType = "delete";
				}
				else {
					queryType = "select";
				}
				
				// Handle queries that aren't skipped and are inserting into the Database
				if (queryType == "insert" && !skipQuery) {
					ServiceHelper().WriteToLog("Parsing insert to prevent duplication creation");
					processInsertStatement(res["query"], data, skipQuery);
					LOG << res["query"];
					if(skipQuery)
						goto parsedQuery;
				}
				
				// handles queries that aren't skipped and are attempting to update existing enteries in the database
				if (queryType == "update" && !skipQuery) {
					ServiceHelper().WriteToLog("Parsing update to prevent altering entries not for current device");
					LOG << res["query"];
					processUpdateStatement(res["query"], data, skipQuery);
					LOG << res["query"];

					if (data.value("table").toString() == "cribtrak/transactions") {
						queryType = "insert";
						data.remove("update");
						data.remove("where");
					}

					if (skipQuery)
						goto parsedQuery;
				}

				// handles queries that aren't skipped and are attempting to delete existing enteries in the database
				if (queryType == "delete" && !skipQuery) {
					ServiceHelper().WriteToLog("Parsing delete to prevent removing entries not for current device");
					LOG << res["query"];
					processDeleteStatement(res["query"], data, skipQuery);
					LOG << res["query"];
					if (skipQuery)
						goto parsedQuery;
				}
			}
			else {
				res["id"] = "0";
				res["query"] = "SHOW TABLES";
			}


		parsedQuery:
			LOG << res["query"];
			qDebug() << data;
			ServiceHelper().WriteToCustomLog("Query parsed. " + std::string(skipQuery ? "Query is getting skipped" : "Query is being run"), timeStamp[0] + "-queries");
			ServiceHelper().WriteToCustomLog("Query after being parsed: \n" + QJsonDocument(data).toJson().toStdString(), timeStamp[0] + "-queries");
			
			if (!pushCloudUpdate || skipQuery) {
				string sqlQuery = "UPDATE cloudupdate SET posted = " + std::string(skipQuery ? (retryingQuery ? "3" : "2") : "1") + " WHERE posted <> 1 AND id = " + res["id"].toStdString();

				ServiceHelper().WriteToCustomLog("Updating skipped query with id: " + res["id"].toStdString() + " with posted status " + std::string(skipQuery ? (retryingQuery ? "3" : "2") : "1"), timeStamp[0] + "-queries");
				if (skipQuery && !retryingQuery)
					ServiceHelper().WriteToCustomLog("Skipping query to try again later:\n\tid: " + res["id"].toStdString() + "\n\t query: " + res["query"].toStdString(), timeStamp[0] + "-queries-skipped");
				vector queryResult = ExecuteTargetSql(sqlQuery);
				if (queryResult.size() > 0) {
					for (auto result : queryResult)
						LOG << result["success"];
				}
				continue;
			}
			
			QString targetTable = "";
			if (data.contains("table")) {
				targetTable = data.value("table").toString();
				data.remove("table");
			}

			QStringList tablesNotToDebug = { "kabtrak", "kabtrak/tools", "kabtrak/drawers", "kabtrak/transactions", "users", "employees", "cribtrak/transactions", "portatrak/transaction", "cribtrak/tools", "tools", "cribtrak/consumables", "jobs"};
			
			qDebug() << targetTable;
			qDebug() << queryType;
			LOG << QJsonDocument(data).toJson();
			
			if (!tablesNotToDebug.contains(targetTable)) {
				int tempVal = 0;
			}

			QJsonObject body;
			body["data"] = data;


			QJsonDocument reply;
			
			switch (query_types[queryType]) {
			case INSERT: {
				if (!makePostRequest(apiUrl + "/" + targetTable, res, body, &reply)) {
					LOG << "reply: " << reply.isEmpty();
					if (reply.isEmpty() || !reply.isObject())
						throw HenchmanServiceException("Failed to make post request");
					LOG << "request failed";
					QJsonObject response = reply.object();
					QString sqlQuery;
					if (response.contains("message") && response.value("message").toString() == "Route has not been defined") {
						sqlQuery = "UPDATE cloudupdate SET posted = " + QString::number(2) + " WHERE posted <> 1 AND id = " + res["id"];
						ServiceHelper().WriteToLog("Updating query with id: " + res["id"].toStdString() + " to posted status 2");
					} else {
						sqlQuery = "UPDATE cloudupdate SET posted = " + QString::number(retryingQuery ? 3 : 2) + " WHERE posted <> 1 AND id = " + res["id"];
						ServiceHelper().WriteToLog("Updating query with id: " + res["id"].toStdString() + " to posted status " + std::string(retryingQuery ? "3" : "2"));
					}
					vector queryResult = ExecuteTargetSql(sqlQuery);
					if (queryResult.size() > 0) {
						for (auto result : queryResult)
							LOG << result["success"];
					}
					continue;
				}
				break;
			}
			case UPDATE: {
				if (!makePatchRequest(apiUrl + "/" + targetTable, res, body, &reply)) {
					LOG << "reply: " << reply.isEmpty();
					if (reply.isEmpty() || !reply.isObject())
						throw HenchmanServiceException("Failed to make patch request");
					LOG << "request failed";
					QJsonObject response = reply.object();
					QString sqlQuery;
					if (response.contains("message") && response.value("message").toString() == "Route has not been defined") {
						sqlQuery = "UPDATE cloudupdate SET posted = " + QString::number(2) + " WHERE posted <> 1 AND id = " + res["id"];
						ServiceHelper().WriteToLog("Updating query with id: " + res["id"].toStdString() + " to posted status 2");
					} else {
						sqlQuery = "UPDATE cloudupdate SET posted = " + QString::number(retryingQuery ? 3 : 2) + " WHERE posted <> 1 AND id = " + res["id"];
						ServiceHelper().WriteToLog("Updating query with id: " + res["id"].toStdString() + " to posted status " + std::string(retryingQuery ? "3" : "2"));
					}
					/*if (response.value("error").toObject().value("result").toObject().value("affectedRows").toInt())*/
						//LOG << "Breakpoint";
					vector queryResult = ExecuteTargetSql(sqlQuery);
					if (queryResult.size() > 0) {
						for (auto result : queryResult)
							LOG << result["success"];
					}
					continue;
				}
				break;
			}
			case REMOVE: {
				if (!makeDeleteRequest(apiUrl + "/" + targetTable, res, body, &reply)) {
					LOG << "reply: " << reply.isEmpty();
					if (reply.isEmpty() || !reply.isObject())
						throw HenchmanServiceException("Failed to make patch request");
					LOG << "request failed";
					QJsonObject response = reply.object();
					QString sqlQuery;
					if (response.contains("message") && response.value("message").toString() == "Route has not been defined") {
						sqlQuery = "UPDATE cloudupdate SET posted = " + QString::number(2) + " WHERE posted <> 1 AND id = " + res["id"];
						ServiceHelper().WriteToLog("Updating query with id: " + res["id"].toStdString() + " to posted status 2");
					}
					else {
						sqlQuery = "UPDATE cloudupdate SET posted = " + QString::number(retryingQuery ? 3 : 2) + " WHERE posted <> 1 AND id = " + res["id"];
						ServiceHelper().WriteToLog("Updating query with id: " + res["id"].toStdString() + " to posted status " + std::string(retryingQuery ? "3" : "2"));
					}
					vector queryResult = ExecuteTargetSql(sqlQuery);
					if (queryResult.size() > 0) {
						for (auto result : queryResult)
							LOG << result["success"];
					}
					continue;
				}
				break;
			}
			default: {
				if (!makeNetworkRequest(apiUrl, res, &reply))
				{	
					LOG << "request failed";
					LOG << "reply: " << reply.isEmpty();
					/*res["query"] = queryMap.value("rollback");
					makeNetworkRequest(apiUrl, res);*/
					QString sqlQuery = "UPDATE cloudupdate SET posted = 3 WHERE posted <> 1 AND id = " + res["id"];

					ServiceHelper().WriteToCustomLog("Updating query with id: " + res["id"].toStdString() + " to posted status 3", timeStamp[0] + "-queries");
					vector queryResult = ExecuteTargetSql(sqlQuery);
					if (queryResult.size() > 0) {
						for (auto result : queryResult)
							LOG << result["success"];
					}
					continue;
				
				}
				break;
			}
			}
		
			QString sqlQuery = "UPDATE cloudupdate SET posted = 1 WHERE posted <> 1 AND id = " + res["id"];

			ServiceHelper().WriteToCustomLog("Updating query with id: " + res["id"].toStdString() + " to posted status 1", timeStamp[0] + "-queries");
			vector queryResult = ExecuteTargetSql(sqlQuery);
			if (queryResult.size() > 0) {
				for (auto result : queryResult)
					LOG << result["success"];
			}
		}
		
		if (query.numRowsAffected() > 0)
			ServiceHelper().WriteToCustomLog("Finished network requests", timeStamp[0] + "-queries");

		query.clear();
		query.finish();
		
		db.close();
		//performCleanup();

		result = true;

	}
	catch (exception& e)
	{
		if(db.isOpen())
			db.close();
		ServiceHelper().WriteToError(e.what());
	}

	//netManager->finished(nullptr);
	
	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	
	return result;
}

int DatabaseManager::connectToLocalDB()
{
	timeStamp = ServiceHelper().timestamp();
	QSqlDatabase db;

	LOG << "Test Log";
	try {
		//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));
		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").c_str());
		TCHAR buffer[1024];
		DWORD size = 1024;
		if (databaseDriver == "") {

			//QString dbtype = RegistryManager::GetStrVal(hKey, "Database", REG_SZ).data();
			rtManager.GetVal("Database", REG_SZ, (TCHAR *)buffer, size);
			databaseDriver = buffer;
		}

		LOG << databaseDriver << " | " << databaseDriver.size();
		if (!QSqlDatabase::isDriverAvailable(databaseDriver))
		{
			//ServiceHelper().WriteToError((string)("Provided Database Driver is not available"));
			//RegCloseKey(hKey);
			/*ServiceHelper().WriteToError((string)("The following Databases are supported"));
			ServiceHelper().WriteToError(checkValidDrivers());
			return 0;*/
			// HenchmanServiceException
			throw HenchmanServiceException("Provided database driver is not available");
		}

		//QString schema = RegistryManager::GetStrVal(hKey, "Schema", REG_SZ).data();
		size = 1024;
		rtManager.GetVal("Schema", REG_SZ, (TCHAR*)buffer, size);
		QString schema(buffer);
		LOG << schema;

		if (!QSqlDatabase::contains(schema))
		{

			//QString server = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Server", REG_SZ));
			size = 1024;
			rtManager.GetVal("Server", REG_SZ, (TCHAR *)buffer, size);
			QString server(buffer);

			//int port = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Port", REG_SZ)).toInt();
			size = 1024;
			rtManager.GetVal("Port", REG_SZ, (TCHAR *)buffer, size);
			int port = QString(buffer).toInt();
			//int installDir(buffer);

			//QString user = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Username", REG_SZ));
			size = 1024;
			rtManager.GetVal("Username", REG_SZ, (char*)buffer, size);
			QString user(buffer);

			//string pass = RegistryManager::GetStrVal(hKey, "Password", REG_SZ);
			size = 1024;
			rtManager.GetVal("Password", REG_SZ, (char*)buffer, size);
			QString pass(buffer);

			ServiceHelper().WriteToLog((string)"Creating session to db");
					
			db = QSqlDatabase::addDatabase(databaseDriver, schema);
			db.setHostName(server);
			db.setPort(port);
			db.setUserName(user);
			if (!pass.isEmpty())
				db.setPassword(pass);
			db.setConnectOptions("CLIENT_COMPRESS;");
		}
		else {
			db = QSqlDatabase::database(schema);
		}
		//RegCloseKey(hKey);

		if (!db.open())
			throw HenchmanServiceException("DB Connection failed to open");

		ServiceHelper().WriteToLog((string)"DB Connection successfully opened");
		
		if (!db.driver()->hasFeature(QSqlDriver::Transactions))
			throw HenchmanServiceException("Selected Driver does not support transactions");

		db.transaction();
		QSqlQuery query(db);


		query.exec("SHOW DATABASES;");
		bool dbFound = false;
		while (query.next())
		{
			QString res = query.value(0).toString();
			LOG << res;
			if (res == schema) {
				dbFound = true;
				break;
			}
		}
		query.clear();

		if (!dbFound) {
			ServiceHelper().WriteToLog((string)"Generating Database");
			QString targetQuery = "CREATE DATABASE " + schema + " CHARACTER SET utf8 COLLATE utf8_general_ci";
			LOG << targetQuery;
			if (!query.exec(targetQuery)) {
				ServiceHelper().WriteToError((string)"Failed to create database");
			}
			else {
				ServiceHelper().WriteToLog((string)"Successfully created Database");
			}
		}

		query.clear();
		query.finish();

		if (!db.commit())
			db.rollback();

		db.close();

		db.setDatabaseName(schema);

	}
	catch (exception& e)
	{

		if (db.isOpen()) {
			db.rollback();
			db.close();
		}

		ServiceHelper().WriteToError(e.what());
		return 0;
	}
	return 1;
}

int DatabaseManager::ExecuteTargetSqlScript(std::string& filepath)
{
	int successCount = 0;
	//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").data());
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").c_str());
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	//QString schema = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Schema", REG_SZ));
	rtManager.GetVal("Schema", REG_SZ, (char*)buffer, size);
	QString schema(buffer);
	//RegCloseKey(hKey);
	QSqlDatabase db = QSqlDatabase::database(schema);
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
		QSqlQuery query(db);

		if (!query.exec("USE " + schema + ";"))
			throw HenchmanServiceException("Failed to execute initial DB Query");

		for(QString &statement : sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			if (query.exec(statement))
				successCount++;
			else {
				if(statement.length() <= 128)
					throw HenchmanServiceException("Failed executing: " + statement.toStdString() + "\nReason provided: " + query.lastError().text().toStdString());
				else
					throw HenchmanServiceException("Failed executing statement. Reason provided: " + query.lastError().text().toStdString());

			}
			query.clear();
		}
		query.finish();
		if (!db.commit())
			db.rollback();
		db.close();
		file.close();
	}
	catch (exception& e)
	{
		if (file.isOpen())
			file.close();
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
			db.close();
		}
		ServiceHelper().WriteToError(e.what());
	}
	return successCount;
}

vector<QStringMap> DatabaseManager::ExecuteTargetSql(std::string sqlQuery)
{
	int successCount = 0;
	vector<QStringMap> resultVector;
	QStringMap queryResult;
	queryResult["success"] = "0";
	//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").data());
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").c_str());
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	//QString schema = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Schema", REG_SZ));
	rtManager.GetVal("Schema", REG_SZ, (char*)buffer, size);
	QString schema(buffer);
	//RegCloseKey(hKey);
	QSqlDatabase db = QSqlDatabase::database(schema);

	try {

		if (!db.open())
		{
			// HenchmanServiceException
			throw HenchmanServiceException("Failed to open DB Connection");
		}
		db.transaction();

		QSqlQuery query(db);

		QString sql = sqlQuery.data();

		QStringList sqlStatements = sql.split(';', Qt::SkipEmptyParts);

		if (!query.exec("USE " + schema + ";"))
			//throw HenchmanServiceException("Failed to execute DB Query: USE " + schema.toStdString() + ";");
			ServiceHelper().WriteToError("Failed to execute DB Query: USE " + schema.toStdString() + ";");

		for(QString &statement:sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			LOG << "Executing: " << statement.toStdString();
			if (query.exec(statement))
			{
				successCount++;
				QSqlRecord record(query.record());
				
				queryResult["success"] = QString::number(successCount);
				resultVector.push_back(queryResult);
				
				while (query.next())
				{
					queryResult.clear();
					for (int i = 0; i <= record.count() - 1; i++)
					{
						queryResult[record.fieldName(i)] = query.value(i).toString();
					}

					resultVector.push_back(queryResult);
				}
			}
			else {
				throw HenchmanServiceException("Failed executing: " + statement.toStdString() + "\nReason provided: " + query.lastError().text().toStdString());
			}
			query.clear();
		}
		query.finish();
		if (!db.commit())
			db.rollback();
		db.close();
	}
	catch (exception& e)
	{
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
			db.close();
		}
		ServiceHelper().WriteToError(e.what());
		resultVector[0] = queryResult;

	}

	return resultVector;
}

vector<QStringMap> DatabaseManager::ExecuteTargetSql(QString sqlQuery)
{
	return ExecuteTargetSql(sqlQuery.toStdString());
}

vector<QStringMap> DatabaseManager::ExecuteTargetSql(const TCHAR* sqlQuery)
{
	return ExecuteTargetSql((std::string)sqlQuery);
}

void DatabaseManager::parseData(QNetworkReply *netReply)
{
	QRestReply restReply(netReply);
	optional json = restReply.readJson();
	optional <QJsonObject> response = json->object();
	string sqlQuery = "UPDATE cloudupdate SET posted = 1 WHERE posted = 0 ORDER BY id LIMIT " + QString::number(queryLimit).toStdString();
	stringstream errorRes;
	stringstream dataRes;

	if (restReply.error() != QNetworkReply::NoError) {
		qWarning() << "A Network error has occured: " << restReply.error() << restReply.errorString();
		ServiceHelper().WriteToError("A Network error has occured: " + restReply.errorString().toStdString());
		goto exit;
	}
	if (!restReply.isHttpStatusSuccess()) {
		int status = restReply.httpStatus();
		QString reason = restReply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
		qWarning() << "A HTTP error has occured: " << status << reason;
		ServiceHelper().WriteToError("An HTTP error has occured: " + to_string(status) + " \"" + reason.toStdString() + "\"");
	}
	if (restReply.isHttpStatusSuccess()) {
		int status = restReply.httpStatus();
		QString reason = restReply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
		ServiceHelper().WriteToLog("Request was successful : " + to_string(status) + " \"" + reason.toStdString() + "\"");
	}
	ServiceHelper().WriteToLog((string)"Parsing Response");

	ExecuteTargetSql(sqlQuery);

	if (!json) {
		ServiceHelper().WriteToError((string)"Recieved empty data or failed to parse JSON.");
		goto exit;
	}

	if (response.value()["error"].toArray().count() > 0) {
		for (const auto& result : response.value()["error"].toArray()) {
			for (auto i = 0; i < result.toArray().size(); i++) {
				errorRes << " - " << result.toArray()[i].toString().toStdString() << endl;
			}
		}

		ServiceHelper().WriteToError("Server responded with error: " + errorRes.str());
	}

	if (response.value()["data"].toArray().count() > 0) {
		for (const auto& result : response.value()["data"].toArray()) {
			for (auto i = 0; i < result.toArray().size(); i++) {
				dataRes << " - " << result.toArray()[i].toString().toStdString() << endl;
			}

		}
		ServiceHelper().WriteToLog("Server responded with data: " + dataRes.str());
	}

exit:
	json.reset();
	//performCleanup();

	requestRunning = false;

	return;
}

void DatabaseManager::processKeysAndValues(QStringMap &map, QString (&results)[])
{
	QString queryKeys = "";
	QString queryValues = "";
	//QString conditionals = "";
	int count = map.size();
	QStringList keys = map.keys();
	
	for (auto& key : map.keys()) {
		count--;
		if (key == "id" || map.value(key).isEmpty() || map.value(key) == "0" || map.value(key) == "'0'")
			continue;
		if (QRegularExpression("\\d\\d\\d\\d-\\d\\d-\\d\\dT\\d\\d:\\d\\d:\\d\\d.\\d\\d\\dZ").match(map.value(key)).hasMatch())
			map[key] = 
			"'" + QRegularExpression("\\d\\d\\d\\d-\\d\\d-\\d\\d").match(map.value(key)).captured(0) + 
			" " + 
			QRegularExpression("\\d\\d:\\d\\d:\\d\\d").match(map.value(key)).captured(0) + "'";
		else
			map[key] = "'" + map.value(key) + "'";
		queryKeys.append((queryKeys.size() > 0 ? ", " : "") + ("`" + key + "`"));
		queryValues.append((queryValues.size() > 0 ? ", " : "") + map.value(key));
	}
	
	results[0] = queryKeys;
	results[1] = queryValues;
}

QString parseTimeValue(const QString& time) {
	QStringList splitTime;

	if (time.contains(" "))
		splitTime = time.split(" ").last().split(":");
	else
		splitTime = time.split(":");

	QStringList secondsSplit = splitTime.last().split(".");
	int hours = splitTime.first().toInt();
	int minutes = splitTime.at(1).toInt();
	int seconds = secondsSplit.first().toInt();
	if (secondsSplit.length() > 1 && QString::number(secondsSplit.last().toInt()).slice(0, 1).toInt() >= 5) {
		seconds++;
	}
	if (seconds >= 60) {
		seconds = 0;
		minutes++;
	}
	if (minutes >= 60) {
		minutes = 0;
		hours++;
	}
	if (hours >= 24) {
		hours = 0;
	}
	splitTime.last() = (seconds < 10 ? "0" : "") + QString::number(seconds);
	splitTime[1] = (minutes < 10 ? "0" : "") + QString::number(minutes);
	splitTime.first() = (hours < 10 ? "0" : "") + QString::number(hours);
	return splitTime.join(":");
}

void DatabaseManager::processInsertStatement(QString& query, QJsonObject& data,  bool& skipQuery)
{
	
	QString targetQuery = query;
	
	QStringList splitQuery;
	splitQuery.append(targetQuery.slice(0, query.indexOf("(")).trimmed());
	splitQuery.append(query.slice(query.indexOf("(")).trimmed().split("VALUES", Qt::SkipEmptyParts, Qt::CaseInsensitive));

	bool hasCustId = 0;
	bool hasTrakId = 0;
	bool hasId = 0;
	const char* idColumn = "id";
	bool switchToConditions = 0;
	int idFromQuery = 0;

	QJsonArray cols;
	QJsonArray vals;

	QStringList insertColumns = splitQuery.at(1).trimmed().split(",", Qt::SkipEmptyParts);
	for (auto it = insertColumns.cbegin(); it != insertColumns.cend(); ++it) {
		QString targetCol = it->trimmed();
		if (targetCol.startsWith("("))
			targetCol.slice(1);
		if (targetCol.endsWith(")"))
			targetCol.slice(0, targetCol.length() - 1);

		cols.append(targetCol);
	}

	QStringList insertValues = splitQuery.at(2).trimmed().split(",", Qt::SkipEmptyParts);
	QString tempVal;
	for (auto it = insertValues.cbegin(); it != insertValues.cend(); ++it) {
		QString targetVal = it->trimmed();
		if (targetVal.startsWith("("))
			targetVal.slice(1);
		if (targetVal.endsWith(")"))
			targetVal.slice(0, targetVal.length() - 1);

		if (!tempVal.isEmpty()) {
			tempVal.append(",");
			//tempVal.append(targetVal);
			//continue;
		}

		targetVal = targetVal.trimmed();
		
		if (targetVal.startsWith("'") && !targetVal.endsWith("'")) {
			tempVal.append(targetVal.slice(1));
			continue;
		}

		if (targetVal.endsWith("'") && !targetVal.startsWith("'")) {
			tempVal.append(targetVal.slice(0, targetVal.length() - 1));
			vals.append(tempVal);
			tempVal = "";
			continue;
		}

		if (targetVal.startsWith("'") && targetVal.endsWith("'")) {
			targetVal.slice(1, targetVal.length() - 2);
		}

		if (targetVal.contains("NULL"))
			continue;

		if (!tempVal.isEmpty())
			tempVal.append(targetVal);
		else
			vals.append(targetVal);

	}

	QJsonObject entry;

	int limit = cols.count();
	if (vals.count() > limit)
		limit = vals.count();

	for (int i = 0; i < limit; i++) {
		QString key = cols.at(i).toString().trimmed();
		QString val = vals.at(i).toString().trimmed();
		if (val.isEmpty())
			continue;
		entry[key] = val;
	}

	if (entry.contains("custid")) {
		entry["custId"] = entry.value("custid");
		entry.remove("custid");
	}

	hasCustId = entry.contains("custId");
	hasTrakId = entry.contains(trakId.data());
	if (entry.keys().contains("id", Qt::CaseInsensitive) || entry.keys().contains("toolId", Qt::CaseInsensitive)) {
		hasId = 1;
		idColumn = entry.keys().contains("id", Qt::CaseInsensitive) ? "id" : "toolId";
	}

	if (entry.contains("transTime") && !entry.value("transTime").toString().isEmpty()) {
		entry["transTime"] = parseTimeValue(entry.value("transTime").toString());
	}
	if (entry.contains("outTime") && !entry.value("outTime").toString().isEmpty()) {
		entry["outTime"] = parseTimeValue(entry.value("outTime").toString());
	}
	if (entry.contains("inTime") && !entry.value("inTime").toString().isEmpty()) {
		entry["inTime"] = parseTimeValue(entry.value("inTime").toString());
	}

	QStringList skipTargetCols = { "id", "createdAt", "updatedAt", "table"};

	QMap<QString, QString> map;
	QString columns;
	QString values;
	QString newQuery;
	std::string targetTable = splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed().toStdString();

	switch (table_map[splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed()])
	{
	case tools:
	{
		if (!hasCustId)
			entry["custId"] = custId;
		if(hasId && idColumn == "id" && (!entry.contains("toolId") || entry.value("toolId").toString().isEmpty()))
			entry["toolId"] = entry.value("id").toString();


		break;
	}
	case users:
	{
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		break;
	}
	case employees:
	{
		if (!hasCustId)
			entry["custId"] = custId;

		break;
	}
	case jobs:
	{
		if (!hasCustId)
			entry["custId"] = custId;
		
		if (!entry.contains("trailId")) {

			QString jobQuery = "SELECT * FROM jobs WHERE ";
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty())
					continue;
				if (value.startsWith("'"))
					jobQuery.append(it.key() + " = " + value);
				else
					jobQuery.append(it.key() + " = '" + value + "'");
				if ((it + 1) != entry.constEnd())
					jobQuery.append(" AND ");
			}

			vector fetchedJob = ExecuteTargetSql(jobQuery);

			if (fetchedJob.size() <= 1) {
				skipQuery = true;
				break;
			}

			for (auto it = fetchedJob[1].cbegin(); it != fetchedJob[1].cend(); ++it) {
				if (entry.contains(it.key()) || it.value().isEmpty())
					continue;
				entry[it.key()] = it.value();
			}
		}
		

		break;
	}
	case kabs:
	{
		entry["table"] = "kabtrak";
		if(!hasCustId)
			entry["custId"] = custId;
		if(!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		break;
	}
	case drawers:
	{
		entry["table"] = "kabtrak/drawers";
		if(!hasCustId)
			entry["custId"] = custId;
		if(!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		break;
	}
	case toolbins:
	{
		entry["table"] = "kabtrak/tools";
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;
		
		if (!entry.contains("toolId") || entry.value("toolId").toString().isEmpty()) {
			QString kabToolQuery = "SELECT t.id as toolId FROM itemkabdrawerbins AS kt INNER JOIN tools AS t ON t.custId = kt.custId AND (kt.itemId LIKE t.PartNo OR kt.itemId LIKE t.stockcode) WHERE ";
			QStringList targetKeys = { "custId", "kabId", "drawerNum", "toolNumber", "itemId" };
			QStringList queryConditions;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				if (value.startsWith("'"))
					queryConditions.append("kt." + key + " = " + value);
				else
					queryConditions.append("kt." + key + " = '" + value + "'");
			}
			kabToolQuery.append(queryConditions.join(" AND "));
			//qDebug() << kabToolQuery;
			vector fetchedKabTool = ExecuteTargetSql(kabToolQuery);

			if (fetchedKabTool.size() <= 1) {
				skipQuery = true;
				break;
			}

			for (auto it = fetchedKabTool[1].cbegin(); it != fetchedKabTool[1].cend(); ++it) {
				if (entry.contains(it.key()) || it.value().isEmpty())
					continue;
				entry[it.key()] = it.value();
			}
		}

		break;
	}
	case cribs: {

		entry["table"] = "cribtrak";
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		break;
	}
	case cribconsumables:
	{
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if (!entry.contains("toolId") || entry.value("toolId").toString().isEmpty()) {
			QString toolQuery = "SELECT t.id as toolId FROM cribtools AS ct INNER JOIN tools AS t ON t.custId = ct.custId AND (ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo) WHERE ";
			QStringList targetKeys = { "custId", "cribId", "barcode"};
			QStringList queryConditions;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				if (key == "barcode")
					key = "barcodeTAG";
				if (value.startsWith("'"))
					queryConditions.append("ct." + key + " = " + value);
				else
					queryConditions.append("ct." + key + " = '" + value + "'");
			}
			toolQuery.append(queryConditions.join(" AND "));
			toolQuery.append(" ORDER BY barcodeTAG DESC LIMIT 1");
			//qDebug() << toolQuery;
			vector fetchedTool = ExecuteTargetSql(toolQuery);

			if (fetchedTool.size() <= 1) {
				skipQuery = true;
				break;
			}

			for (auto it = fetchedTool[1].cbegin(); it != fetchedTool[1].cend(); ++it) {
				if (entry.contains(it.key()) || it.value().isEmpty())
					continue;
				entry[it.key()] = it.value();
			}
		}

		entry["table"] = "cribtrak/consumables";

		break;
	}
	case cribtoollocation:
	{
		entry["table"] = "cribtrak/tools/locations";
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if (!entry.contains("locationId") && entry.contains("id"))
			entry["locationId"] = entry.value("id");
		else if (!entry.contains("locationId") && !entry.contains("id")) {
			std::vector locationId = ExecuteTargetSql(QString("SELECT id FROM cribtoollocation WHERE description = '").append(entry.value("description").toString() + "'").toStdString());
			entry["locationId"] = locationId.at(1).value("id");
		}

		break;
	}
	//case cribtoollockers:
	case cribtools:
	{
		entry["table"] = "cribtrak/tools";
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		vector colsCheck = ExecuteTargetSql("SHOW KEYS FROM cribtools WHERE Key_name = 'PRIMARY'");
		QString indexingCol = colsCheck[1].value("Column_name");

		QString cribToolQuery = "SELECT ct."+ indexingCol +" AS id, t.id as toolId FROM cribtools AS ct INNER JOIN tools AS t ON t.custId = ct.custId AND (ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo) WHERE ";
		QStringList targetKeys = { "custId", "cribId", "barcodeTAG" };
		QStringList queryConditions;
		for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
			QString value = it.value().toString();
			if (value.isEmpty() || !targetKeys.contains(it.key()))
				continue;
			QString key = it.key();
			if (value.startsWith("'"))
				queryConditions.append("ct." + key + " = " + value);
			else
				queryConditions.append("ct." + key + " = '" + value + "'");
		}
		cribToolQuery.append(queryConditions.join(" AND "));
		//qDebug() << cribToolQuery;
		vector fetchedKabTool = ExecuteTargetSql(cribToolQuery);

		if (fetchedKabTool.size() <= 1) {
			skipQuery = true;
			break;
		}

		for (auto it = fetchedKabTool[1].cbegin(); it != fetchedKabTool[1].cend(); ++it) {
			if (!targetKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
				continue;
			entry[it.key()] = it.value();
		}

		break;
	}
	case kittools: {
		
		entry["table"] = "cribtrak/kits";
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		break;
	}
	case tooltransfer:
	{
		entry["table"] = "cribtrak/tools/transfer";
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if(!entry.contains("transferId") && entry.contains("id"))
			entry["transferId"] = entry.value("id");
		else if (!entry.contains("transferId") && !entry.contains("id")) {
			std::vector locationId = ExecuteTargetSql(QString("SELECT id FROM tooltransfer WHERE barcodeTAG = '").append(entry.value("barcodeTAG").toString()).toStdString() + "'");
			if (locationId.size() <= 1) {
				skipQuery = true;
				break;
			}
			entry["transferId"] = locationId.at(1).value("id");
		}
		
		break;
	}
	case itemkits: {
		entry["table"] = "portatrak/kit";

		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if (!entry.contains("kitId") || entry.value("kitId").toString().isEmpty()) {

			QString kitQuery = "SELECT id as kitId FROM itemkits WHERE ";
			QStringList targetKeys = { "custId", "scaleId", "kitTAG" };
			QStringList queryConditions;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				QString key = it.key();
				if (value.isEmpty() || !targetKeys.contains(key))
					continue;
				if (value.startsWith("'"))
					queryConditions.append(key + " = " + value);
				else
					queryConditions.append(key + " = '" + value + "'");
			}
			kitQuery.append(queryConditions.join(" AND "));
			//qDebug() << kitQuery;
			vector fetchedRes = ExecuteTargetSql(kitQuery);

			if (fetchedRes.size() <= 1) {
				skipQuery = true;
				break;
			}
			QStringList targerResponseKeys = { "kitId" };
			for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
				if (!targerResponseKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
					continue;
				if (it.key() == "kitId") {
					entry[it.key()] = QString("000").slice(QString::number(custId).length()) + QString::number(custId) + QString("000").slice(it.value().length()) + it.value();
				}
				else {
					entry[it.key()] = it.value();
				}
			}
		}
		
		break;
	}
	case kitcategory:
	{
		entry["table"] = "portatrak/kit/category";

		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if (!entry.contains("categoryId") || entry.value("categoryId").toString().isEmpty()) {

			QString categoryQuery = "SELECT id as categoryId FROM kitcategory WHERE ";
			QStringList targetKeys = { "custId", "description" };
			QStringList queryConditions;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString().simplified();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				if (value.startsWith("'") && value.endsWith("'"))
					queryConditions.append(key + " = " + value);
				else
					queryConditions.append(key + " = '" + value + "'");
			}
			categoryQuery.append(queryConditions.join(" AND "));
			//qDebug() << categoryQuery;
			vector fetchedRes = ExecuteTargetSql(categoryQuery);

			if (fetchedRes.size() <= 1) {
				skipQuery = true;
				break;
			}
			QStringList targerResponseKeys = { "categoryId" };
			for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
				if (!targerResponseKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
					continue;
				entry[it.key()] = it.value();
			}
		}

		break;
	}
	case kitlocation:
	{
		entry["table"] = "portatrak/kit/location";

		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if (!entry.contains("locationId") || entry.value("locationId").toString().isEmpty()) {

			QString locationQuery = "SELECT id as locationId FROM kitlocation WHERE ";
			QStringList targetKeys = { "custId", "scaleId", "description" };
			QStringList queryConditions;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString().simplified();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				if (value.startsWith("'") && value.endsWith("'"))
					queryConditions.append(key + " = " + value);
				else
					queryConditions.append(key + " = '" + value + "'");
			}
			locationQuery.append(queryConditions.join(" AND "));
			//qDebug() << locationQuery;
			vector fetchedRes = ExecuteTargetSql(locationQuery);

			if (fetchedRes.size() <= 1) {
				skipQuery = true;
				break;
			}
			QStringList targerResponseKeys = { "locationId" };
			for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
				if (!targerResponseKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
					continue;
				entry[it.key()] = it.value();
			}
		}

		break;
	}
	case kabemployeeitemtransactions: {
		entry["table"] = "kabtrak/transactions";

		if (!entry.contains("itemId") || entry.value("itemId").toString().isEmpty()) {

			QString kabToolQuery = "SELECT itemId FROM itemkabdrawerbins WHERE ";
			QStringList targetKeys = { "custId", "kabId", "drawerNum", "toolNum", "itemId" };
			QStringList queryConditions;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				if (key == "toolNum")
					key = "toolNumber";
				if (value.startsWith("'"))
					//queryConditions[key] = value;
					queryConditions.append(key + " = " + value);
				else
					//queryConditions[key] = "'"+value+"'";
					queryConditions.append(key + " = '" + value + "'");
			}
			kabToolQuery.append(queryConditions.join(" AND "));
			kabToolQuery.append(" LIMIT 1");
			//qDebug() << kabToolQuery;
			vector fetchedKabTool = ExecuteTargetSql(kabToolQuery);

			if (fetchedKabTool.size() > 1) {
				for (auto it = fetchedKabTool[1].cbegin(); it != fetchedKabTool[1].cend(); ++it) {
					if (!targetKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
						continue;
					entry[it.key()] = it.value();
				}
			}

		}

		if (!entry.contains("toolId") || !entry.value("itemId").toString().isEmpty()) {

			QString kabToolQuery = "SELECT id AS toolId FROM tools WHERE PartNo LIKE '" + entry.value("itemId").toString() + "' OR stockcode LIKE '" + entry.value("itemId").toString() + "'";
			//qDebug() << kabToolQuery;
			vector fetchedKabTool = ExecuteTargetSql(kabToolQuery);

			if (fetchedKabTool.size() > 1) {
				for (auto it = fetchedKabTool[1].cbegin(); it != fetchedKabTool[1].cend(); ++it) {
					if ((entry.contains(it.key()) || it.value().isEmpty()))
						continue;
					entry[it.key()] = it.value();
				}
			}

		}

		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		break;
	}
	case cribemployeeitemtransactions: {
		entry["table"] = "cribtrak/transactions";

		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if ((!entry.contains("toolId") || entry.value("toolId").toString().isEmpty()) && ((entry.contains("barcode") && !entry.value("barcode").toString().isEmpty()) || (entry.contains("itemId") && !entry.value("itemId").toString().isEmpty()))) {
			std::vector<std::string> conditions;
			if (entry.contains("barcode") && !entry.value("barcode").toString().isEmpty())
				conditions.push_back("barcodeTAG = '" + entry.value("barcode").toString().toStdString() + "'");
			if (entry.contains("itemId") && !entry.value("itemId").toString().isEmpty())
				conditions.push_back("itemId = '" + entry.value("itemId").toString().toStdString() + "'");
			
			std::vector<stringmap> returnedData = sqliteManager.GetEntry(
				"cribtools",
				{ "itemId", "serialNo", "toolId" },
				conditions
			);

			//qDebug() << returnedData;

			if (!returnedData.empty()) {
				QStringList transactionQueryList = { };

				if(returnedData[0].contains("itemId"))
					transactionQueryList.append(QString::fromStdString("PartNo LIKE '" + returnedData[0].at("itemId") + "'"));
				if (returnedData[0].contains("serialNo"))
					transactionQueryList.append(QString::fromStdString("serialNo LIKE '" + returnedData[0].at("serialNo") + "'"));
				if (returnedData[0].contains("toolId"))
					transactionQueryList.append(QString::fromStdString("id = '" + returnedData[0].at("toolId") + "'"));

				/*QString transactionQuery = "SELECT t.id as toolId FROM cribtools AS ct LEFT JOIN tools AS t ON t.custId = ct.custId AND (ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo OR ct.toolId = t.id) WHERE ct.barcodeTAG = '" + conditionPairs.value("barcode").toString() + "' LIMIT 1";*/
				QString transactionQuery = "SELECT id as toolId FROM tools WHERE " + transactionQueryList.join(" OR ") + " LIMIT 1";

				//QString kabToolQuery = "SELECT t.id as toolId FROM cribtools as ct INNER JOIN tools AS t ON t.custId = ct.custId AND (ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo OR ct.toolId = t.id) WHERE ";
				QStringList targetKeys = { "custId", "cribId", "barcode" };
				/*QStringList queryConditions;
				for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
					QString value = it.value().toString();
					if (value.isEmpty() || !targetKeys.contains(it.key()))
						continue;
					QString key = it.key();
					if (key == "barcode")
						key = "barcodeTAG";
					if (value.startsWith("'"))
						queryConditions.append("ct." + key + " = " + value);
					else
						queryConditions.append("ct." + key + " = '" + value + "'");
				}
				kabToolQuery.append(queryConditions.join(" AND "));
				kabToolQuery.append(" LIMIT 1");*/
				//qDebug() << transactionQuery;
				vector fetchedKabTool = ExecuteTargetSql(transactionQuery);

				if (fetchedKabTool.size() > 1) {
					for (auto it = fetchedKabTool[1].cbegin(); it != fetchedKabTool[1].cend(); ++it) {
						if (!targetKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
							continue;
						entry[it.key()] = it.value();
					}
				}
			}
		}

		if (
			(!entry.contains("itemId") || entry.value("itemId").toString().isEmpty()) && 
			(entry.contains("toolId") && !entry.value("toolId").toString().isEmpty())) {
			QString transactionQuery = "SELECT PartNo as itemId FROM tools WHERE id = '" + entry.value("toolId").toString() + "'";

			//LOG << transactionQuery;
			vector queryRes = ExecuteTargetSql(transactionQuery);
			//qDebug() << queryRes;
			if (queryRes.size() > 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					entry[key] = it.value();
				}
			}
		}

		if ((!entry.contains("issuedBy") || entry.value("issuedBy").toString().isEmpty()) && entry.contains("userId")) {
			entry["issuedBy"] = entry.value("userId");
		}

		if ((!entry.contains("tailId") || entry.value("tailId").toString().isEmpty()) && entry.contains("trailId")) {
			vector colsCheck = ExecuteTargetSql("SHOW KEYS FROM jobs WHERE Key_name = 'PRIMARY'");
			QString indexingCol = colsCheck[1].value("Column_name");

			QString transactionQuery = "SELECT description as tailId FROM jobs WHERE " + indexingCol + " = '" + entry.value("trailId").toString() + "'";

			//LOG << transactionQuery;
			vector queryRes = ExecuteTargetSql(transactionQuery);
			//qDebug() << queryRes;
			if (queryRes.size() > 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					entry[key] = it.value();
				}
			}

		}

		if ((!entry.contains("trailId") || entry.value("trailId").toString().isEmpty() || entry.value("trailId").toString() == "0") && (entry.contains("tailId") && !entry.value("tailId").toString().isEmpty())) {
			vector colsCheck = ExecuteTargetSql("SHOW KEYS FROM jobs WHERE Key_name = 'PRIMARY'");
			QString indexingCol = colsCheck[1].value("Column_name");

			QString transactionQuery = "SELECT " + indexingCol + " FROM jobs WHERE description LIKE '" + entry.value("tailId").toString() + "'";

			//LOG << transactionQuery;
			vector queryRes = ExecuteTargetSql(transactionQuery);
			//qDebug() << queryRes;
			if (queryRes.size() <= 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					entry[key] = it.value();
				}
			}
		}

		break;
	}
	case portaemployeeitemtransactions: {
		entry["table"] = "portatrak/transaction";

		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if (
			(entry.contains("kitTAG") && !entry.value("kitTAG").toString().isEmpty()) 
			&&
			(!entry.contains("kitId") || entry.value("kitId").toString().isEmpty())
			) {
			QString kitQuery = "SELECT id as kitId FROM itemkits WHERE ";
			QStringList targetKeys = { "custId", "scaleId", "kitTAG" };
			QStringList queryConditions;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				if (value.startsWith("'"))
					queryConditions.append(key + " = " + value);
				else
					queryConditions.append(key + " = '" + value + "'");
			}
			kitQuery.append(queryConditions.join(" AND "));
			//qDebug() << kitQuery;
			vector fetchedRes = ExecuteTargetSql(kitQuery);
			//qDebug() << fetchedRes;
			if (fetchedRes.size() > 1) {	
				QStringList targerResponseKeys = { "kitId" };
				for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
					if (!targerResponseKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
						continue;
					if (it.key() == "kitId") {
						entry[it.key()] = QString("000").slice(QString::number(custId).length()) + QString::number(custId) + QString("000").slice(it.value().length()) + it.value();
					}
					else {
						entry[it.key()] = it.value();
					}
				}
			}
		}

		if (
			(entry.contains("tailId") && !entry.value("tailId").toString().isEmpty())
			&&
			(!entry.contains("trailId") || entry.value("trailId").toString().isEmpty())
			) {
			vector colsCheck = ExecuteTargetSql("SHOW KEYS FROM jobs WHERE Key_name = 'PRIMARY'");
			QString indexingCol = colsCheck[1].value("Column_name");
			QString jobQuery = "SELECT "+ indexingCol +" as trailId FROM jobs WHERE ";
			QStringList targetKeys = { "custId", "tailId" };
			QStringList queryConditions;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				if (key == "tailId")
					key = "description";
				if (value.startsWith("'"))
					queryConditions.append(key + " = " + value);
				else
					queryConditions.append(key + " = '" + value + "'");
			}
			jobQuery.append(queryConditions.join(" AND "));
			//qDebug() << jobQuery;
			vector fetchedRes = ExecuteTargetSql(jobQuery);
			//qDebug() << fetchedRes;
			if (fetchedRes.size() > 1) {
				QStringList targerResponseKeys = { "trailId" };
				for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
					if (!targerResponseKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
						continue;
				
					entry[it.key()] = it.value();
				}
			}
		}

		break;
	}
	//case lokkaemployeeitemtransactions:
	default:
		skipQuery = true;
		break;
	}

	if (skipQuery)
		return;

	if(entry.value("table").toString().isEmpty())
		entry["table"] = splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed();

	std::map<std::string, std::string> toolData;

	for (auto it = entry.constBegin(); it != entry.constEnd(); ++it)
	{
		QString key = it.key();
		QString val = it.value().toString().trimmed().simplified();
		if (val.isEmpty() || val == "''" || key == "table")
			continue;
		if (!skipTargetCols.contains(key))
			toolData[key.toStdString()] = val.toStdString();
	}

	//QList<const char*> tablesToSkip = { "kabemployeeitemtransactions", "cribemployeeitemtransactions", "portaemployeeitemtransactions" };
	//if (!tablesToSkip.contains(targetTable)) {
	//qDebug() << toolData;
	sqliteManager.AddEntry(
		splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed().toStdString(),
		toolData
	);
	//}

	toolData.clear();

	data.swap(entry);

	//LOG << query;
	return;
}

void DatabaseManager::processUpdateStatement(QString& query, QJsonObject& data, bool& skipQuery)
{
	QStringList splitQueryForParsing = ServiceHelper::ExplodeString(query, " ");
	//qDebug() << splitQueryForParsing;
	QStringList querySections = query.split(" SET ", Qt::SkipEmptyParts, Qt::CaseInsensitive);
	//QStringList querySections = ServiceHelper::ExplodeString(query, splitBy.data());
	//qDebug() << querySections;
	if (querySections.size() > 2) {
		QStringList tempQuerySections = querySections;
		tempQuerySections.removeAt(0);
		querySections[1] = tempQuerySections.join(" SET ");
		querySections.remove(2, querySections.size() - 2);
	}
	//qDebug() << querySections;

	querySections.append(querySections[1].split(" WHERE ", Qt::SkipEmptyParts, Qt::CaseInsensitive));
	querySections.removeAt(1);
	//qDebug() << querySections;
	if (querySections.size() > 3) {
		QStringList tempQuerySections = querySections;
		tempQuerySections.removeAt(2);
		querySections[2] = tempQuerySections.join(" WHERE ");
		querySections.remove(3, querySections.size() - 3);
	}
	//qDebug() << querySections;
	QStringList returnVal;
	//QMap<QString, QString> setPairs;
	QJsonObject setPairs;
	//QMap<QString, QString> conditionPairs;
	QJsonObject conditionPairs;
	QStringList parsedSets;
	QStringList parsedConditionals;
	bool switchToConditions = 0;
	bool hadCustId = 0;
	bool hadTrakId = 0;
	bool hadId = 0;
	int idFromQuery = 0;

	QStringList querySetSection = querySections[1].split("=", Qt::SkipEmptyParts);
	QString priorVal;
	for (auto it = querySetSection.cbegin(); it != querySetSection.cend(); ++it)
	{	
		QString setTrimmed = it->trimmed();
		if (priorVal.isEmpty()) {
			//qDebug() << "priorVal.isEmpty() " << setTrimmed;
			priorVal = setTrimmed;
			continue;
		}
		
		auto nextIt = it + 1;
		//qDebug() << nextIt << " : " << querySetSection.cend() << (nextIt == querySetSection.cend());
		if (nextIt != querySetSection.cend()) {
			QString currVal = setTrimmed;
			currVal = currVal.slice(0, setTrimmed.lastIndexOf(",")).trimmed();
			if (currVal.startsWith("'") && currVal.endsWith("'")) {
				currVal.slice(1, currVal.length() - 2);
			}
			//qDebug() << "currVal "<<currVal;
			if (currVal == "NULL") {
				skipQuery = true;
				break;
			}
			setPairs[priorVal] = currVal;
			QString nextVal = setTrimmed;
			nextVal = nextVal.slice(setTrimmed.lastIndexOf(",")+1).trimmed();
			//qDebug() << "nextVal " <<nextVal;
			priorVal = nextVal;

		}
		else {
			if (setTrimmed.startsWith("'") && setTrimmed.endsWith("'")) {
				setTrimmed.slice(1, setTrimmed.length() - 2);
			}
			//qDebug() << "setTrimmed " << setTrimmed;
			setPairs[priorVal] = setTrimmed;
		}

		
	}

	//// Parse conditionals
	QStringList queryConditionalSections;
		queryConditionalSections = querySections[2].split("AND", Qt::SkipEmptyParts, Qt::CaseInsensitive);
	priorVal = "";
	for (auto it = queryConditionalSections.cbegin(); it != queryConditionalSections.cend(); ++it)
	{
		QString conditionTrimmed = it->trimmed();
		if (conditionTrimmed.startsWith("("))
			conditionTrimmed = conditionTrimmed.slice(1);
		if (conditionTrimmed.endsWith(")"))
			conditionTrimmed = conditionTrimmed.slice(0, conditionTrimmed.length() - 1);
		QStringList colAndVal;
		QStringList listOfSplittableOperators = { "<>", "<=", ">=", "<", ">", "!=", "=", " "};
		QString splitOpUsed;
		for (const auto& splitOperator : listOfSplittableOperators) {

			if (!conditionTrimmed.contains(splitOperator))
				continue;

			colAndVal = conditionTrimmed.split(splitOperator);
			splitOpUsed = splitOperator;
			break;

		}


		QString key = colAndVal.at(0).trimmed();
		QString val;
		if (colAndVal.length() > 2) {
			colAndVal.pop_front();
			val = colAndVal.join(splitOpUsed);
		}
		else {
			val = colAndVal.at(1);
		}
		val = val.trimmed();
		if (val.startsWith("'") && val.endsWith("'")) {
			val.slice(1, val.length() - 2);
		}

		qDebug() << key << ": " << val;
		if (val.contains("NULL"))
			continue;
		
		//conditionPairs.insert(key, val);
		conditionPairs[key] = val;
	}

	if (skipQuery)
		return;

	if (conditionPairs.contains("custid")) {
		conditionPairs["custId"] = conditionPairs.value("custid");
		conditionPairs.remove("custid");
	}
	
	hadCustId = conditionPairs.contains("custId");
	hadTrakId = conditionPairs.contains(trakId.data());
	if (conditionPairs.keys().contains("id", Qt::CaseInsensitive))
		hadId = 1;

	if (setPairs.contains("transTime") && !setPairs.value("transTime").toString().isEmpty()) {
		setPairs["transTime"] = parseTimeValue(setPairs.value("transTime").toString());
	}
	if (setPairs.contains("outTime") && !setPairs.value("outTime").toString().isEmpty()) {
		setPairs["outTime"] = parseTimeValue(setPairs.value("outTime").toString());
	}
	if (setPairs.contains("inTime") && !setPairs.value("inTime").toString().isEmpty()) {
		setPairs["inTime"] = parseTimeValue(setPairs.value("inTime").toString());
	}

	if (conditionPairs.contains("transTime") && !conditionPairs.value("transTime").toString().isEmpty()) {
		conditionPairs["transTime"] = parseTimeValue(conditionPairs.value("transTime").toString());
	}
	if (conditionPairs.contains("outTime") && !conditionPairs.value("outTime").toString().isEmpty()) {
		conditionPairs["outTime"] = parseTimeValue(conditionPairs.value("outTime").toString());
	}
	if (conditionPairs.contains("inTime") && !conditionPairs.value("inTime").toString().isEmpty()) {
		conditionPairs["inTime"] = parseTimeValue(conditionPairs.value("inTime").toString());
	}

	/*qDebug() << setPairs;
	qDebug() << conditionPairs;
	qDebug() << data;
	qDebug() << querySections[0].split(" ").at(1) << " | " << table_map[querySections[0].split(" ").at(1)];*/

	QString targetTable = querySections[0].split(" ").at(1);
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt", "table" };

	switch (table_map[targetTable])
	{
	case tools:
	{

		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (hadId && (!conditionPairs.contains("toolId") || conditionPairs.value("toolId").toString().isEmpty()))
			conditionPairs["toolId"] = conditionPairs.value("id").toString();

		if (!hadId && !conditionPairs.contains("toolId") && conditionPairs.contains("stockcode") && !conditionPairs.value("stockcode").toString().isEmpty()) {
			std::vector response = ExecuteTargetSql("SELECT id as toolId FROM tools WHERE stockcode = '" + conditionPairs.value("stockcode").toString() + "'");
			if (response.size() > 1)
				conditionPairs["toolId"] = response[1].value("toolId");
		}

		break;
	}
	case users:
	{

		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case employees: {
		if (!hadCustId)
			conditionPairs["custId"] = custId;

		break;
	}
	case jobs:
	{
		if (!hadCustId)
			conditionPairs["custId"] = custId;

		break;
	}
	case customer: 
	{

		if (!hadId)
			conditionPairs["id"] = custId;
		break;
	}
	case kabs: {
		data["table"] = "kabtrak";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if(!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;


		break;
	}
	case drawers: {
		data["table"] = "kabtrak/drawers";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case toolbins: 
	{

		data["table"] = "kabtrak/tools";

		if (setPairs.contains("toolNumber")) {
			skipQuery = true;
			break;
		}

		QString itemDrawerQuery = "SELECT i.*, t.id as toolId FROM itemkabdrawerbins as i LEFT JOIN tools as t ON i.itemId LIKE t.PartNo OR i.itemId LIKE t.stockcode WHERE ";
		for (auto it = conditionPairs.constBegin(); it != conditionPairs.constEnd(); ++it) {
			QString value = it.value().toString();
			if (value.isEmpty())
				continue;
			if (value.startsWith("'"))
				itemDrawerQuery.append("i." + it.key() + " = " + value);
			else
				itemDrawerQuery.append("i." + it.key() + " = '" + value + "'");
			if ((it + 1) != conditionPairs.constEnd())
				itemDrawerQuery.append(" AND ");
		}

		vector itemDrawerRes = ExecuteTargetSql(itemDrawerQuery);
		//qDebug() << itemDrawerRes;
		if (itemDrawerRes.size() > 1) {	
			QStringList allowedKeyList = { "drawerNum", "toolNumber", "itemId", "toolId" };
			for (auto it = itemDrawerRes[1].cbegin(); it != itemDrawerRes[1].cend(); ++it) {
				if (!allowedKeyList.contains(it.key()) || conditionPairs.contains(it.key()) || it.value().isEmpty())
					continue;

				qDebug() << it.key() << ": " << setPairs.value(it.key()).toString() << " | " << it.value();
				if (setPairs.contains(it.key()) && setPairs.value(it.key()).toString() == it.value()) {
					skipQuery = true;
					break;
				}
				conditionPairs[it.key()] = it.value();
			
			}
		}

		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case cribs: 
	{
		data["table"] = "cribtrak";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case cribconsumables: {
		data["table"] = "cribtrak/consumables";

		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	//case cribtoollocation:
	//case cribtoollockers:
	case cribtools: 
	{
		data["table"] = "cribtrak/tools";

		vector colsCheck = ExecuteTargetSql("SHOW KEYS FROM cribtools WHERE Key_name = 'PRIMARY'");
		QString indexingCol = colsCheck[1].value("Column_name");

		if (indexingCol == "toolId" && conditionPairs.contains("toolId")) {
			QString targetQuery = "SELECT ct.itemId, ct.barcodeTAG, t.id as toolId FROM cribtools AS ct LEFT JOIN tools AS t ON ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo WHERE ct.toolId = '" + conditionPairs.value("toolId").toString() + "'";

			vector response = ExecuteTargetSql(targetQuery);
			if (response.size() > 1) {
				if(!conditionPairs.contains("barcodeTAG"))
					conditionPairs["barcodeTAG"] = response[1].value("barcodeTAG");
				if (!conditionPairs.contains("itemId"))
					conditionPairs["itemId"] = response[1].value("itemId");
				//if (!conditionPairs.contains("barcodeTAG"))
				conditionPairs["toolId"] = response[1].value("toolId");
			}
		}

		if (!conditionPairs.contains("barcodeTAG") && setPairs.contains("serialNo") && !setPairs.value("serialNo").toString().isEmpty()) {
			/*if (conditionPairs.contains("toolId"))
				conditionPairs.remove("toolId");*/
			QString targetQuery = "SELECT ct.barcodeTAG FROM cribtools AS ct WHERE ";
			QStringList conditionals;

			for (auto it = setPairs.constBegin(); it != setPairs.constEnd(); ++it) {
				QString key = it.key();
				QString value = it.value().toString();
				if (value.isEmpty() || key == "toolId" || key == "currentcalibrationdate")
					continue;

				if (value.contains("NULL"))
					conditionals.append("ct." + key + " " + value);
				else if (value.startsWith("'"))
					conditionals.append("ct." + key + " = " + value);
				else
					conditionals.append("ct." + key + " = '" + value + "'");
				/*if ((it + 1) != conditionPairs.constEnd())
					targetQuery.append(" AND ");*/
			}

			for (auto it = conditionPairs.constBegin(); it != conditionPairs.constEnd(); ++it) {
				QString key = it.key();
				QString value = it.value().toString();
				if (setPairs.contains(key) || value.isEmpty() || key == "toolId" || key == "currentcalibrationdate")
					continue;

				if (value.contains("NULL"))
					conditionals.append("ct." + key + " " + value);
				else if (value.startsWith("'"))
					conditionals.append("ct." + key + " = " + value);
				else
					conditionals.append("ct." + key + " = '" + value + "'");
				/*if ((it + 1) != conditionPairs.constEnd())
					targetQuery.append(" AND ");*/
			}
			targetQuery.append(conditionals.join(" AND "));
			targetQuery += " ORDER BY ct.createdDate DESC LIMIT 1";

			std::vector response = ExecuteTargetSql(targetQuery);

			qDebug() << response;

			if (response.size() > 1) {
				conditionPairs["barcodeTAG"] = response[1].value("barcodeTAG");
			}
		}

		if ((indexingCol == "toolId" || !conditionPairs.contains("toolId")) && (conditionPairs.contains("barcodeTAG") && !conditionPairs.value("barcodeTAG").toString().isEmpty())) {
			/*if (conditionPairs.contains("toolId"))
				conditionPairs.remove("toolId");*/
			QString targetQuery = "SELECT t.id as toolId FROM cribtools AS ct LEFT JOIN tools AS t ON ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo " + QString(indexingCol == "toolId" ? "" : "ct.toolId = t.id ") + "WHERE ";
			QStringList conditionals;

			for (auto it = setPairs.constBegin(); it != setPairs.constEnd(); ++it) {
				QString key = it.key();
				QString value = it.value().toString();
				if (value.isEmpty() || key == "toolId" || key == "currentcalibrationdate")
					continue;

				if (value.contains("NULL"))
					conditionals.append("ct." + key + " " + value);
				else if (value.startsWith("'"))
					conditionals.append("ct." + key + " = " + value);
				else
					conditionals.append("ct." + key + " = '" + value + "'");
				/*if ((it + 1) != conditionPairs.constEnd())
					targetQuery.append(" AND ");*/
			}

			for (auto it = conditionPairs.constBegin(); it != conditionPairs.constEnd(); ++it) {
				QString key = it.key();
				QString value = it.value().toString();
				if (setPairs.contains(key) || value.isEmpty() || key == "toolId" || key == "currentcalibrationdate")
					continue;

				if(value.contains("NULL"))
					conditionals.append("ct." + key + " " + value);
				else if (value.startsWith("'"))
					conditionals.append("ct." + key + " = " + value);
				else
					conditionals.append("ct." + key + " = '" + value + "'");
				/*if ((it + 1) != conditionPairs.constEnd())
					targetQuery.append(" AND ");*/
			}
			targetQuery.append(conditionals.join(" AND "));
			targetQuery += " ORDER BY ct.createdDate DESC LIMIT 1";

			std::vector response = ExecuteTargetSql(targetQuery);

			//qDebug() << response;

			if (response.size() > 1) {
				conditionPairs["toolId"] = response[1].value("toolId");
			}
		}

		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case kittools: {

		data["table"] = "cribtrak/kits";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case tooltransfer:
	{
		data["table"] = "cribtrak/tools/transfer";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case itemscale: {
		data["table"] = "portatrak";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case itemkits: {
		data["table"] = "portatrak/kit";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case kitcategory: {
		data["table"] = "portatrak/kit/category";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		if (conditionPairs.contains("categoryId") && !conditionPairs.value("categoryId").toString().isEmpty())
			break;

		QString categoryQuery = "SELECT id as categoryId FROM kitcategory WHERE ";
		QStringList targetKeys = { "category" };
		QStringList queryConditions;
		for (auto it = conditionPairs.constBegin(); it != conditionPairs.constEnd(); ++it) {
			QString value = it.value().toString();
			if (value.isEmpty() || !targetKeys.contains(it.key()))
				continue;
			QString key = it.key();
			if (key == "category")
				key = "id";
			if (value.startsWith("'"))
				queryConditions.append(key + " = " + value);
			else
				queryConditions.append(key + " = '" + value + "'");
		}
		categoryQuery.append(queryConditions.join(" AND "));
		//qDebug() << categoryQuery;
		vector fetchedRes = ExecuteTargetSql(categoryQuery);

		if (fetchedRes.size() <= 1) {
			skipQuery = true;
			break;
		}
		QStringList targerResponseKeys = { "categoryId" };
		for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
			if (!targerResponseKeys.contains(it.key()) && (conditionPairs.contains(it.key()) || it.value().isEmpty()))
				continue;

			conditionPairs[it.key()] = it.value();
		}

		break;
	}
	case kitlocation: {
		data["table"] = "portatrak/kit/location";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		if (!conditionPairs.contains("locationId") || conditionPairs.value("locationId").toString().isEmpty()) {

			QString categoryQuery = "SELECT id as locationId FROM kitlocation WHERE ";
			QStringList targetKeys = { "location" };
			QStringList queryConditions;
			for (auto it = conditionPairs.constBegin(); it != conditionPairs.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				if (key == "location")
					key = "id";
				if (value.startsWith("'"))
					queryConditions.append(key + " = " + value);
				else
					queryConditions.append(key + " = '" + value + "'");
			}
			categoryQuery.append(queryConditions.join(" AND "));
			//qDebug() << categoryQuery;
			vector fetchedRes = ExecuteTargetSql(categoryQuery);

			if (fetchedRes.size() <= 1) {
				skipQuery = true;
				break;
			}
			QStringList targerResponseKeys = { "locationId" };
			for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
				if (!targerResponseKeys.contains(it.key()) && (conditionPairs.contains(it.key()) || it.value().isEmpty()))
					continue;

				conditionPairs[it.key()] = it.value();
			}
		}

		break;
	}
	//case tblcounterid:
	//case kabemployeeitemtransactions:
	case cribemployeeitemtransactions:
	{

		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;
		
		QString transactionQuery = "SELECT * FROM cribemployeeitemtransactions WHERE ";

		if (hadId)
			transactionQuery += "id = " + conditionPairs.value("id").toString();
		else {

			QStringList conditionsList;

			for (auto it = setPairs.constBegin(); it != setPairs.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty())
					continue;
				if (value.startsWith("'"))
					conditionsList.append(it.key() + " = " + value);
				else
					conditionsList.append(it.key() + " = '" + value + "'");
				/*if ((it + 1) != setPairs.constEnd())
					transactionQuery.append(" AND ");*/
			}

			for (auto it = conditionPairs.constBegin(); it != conditionPairs.constEnd(); ++it) {
				/*if (it == conditionPairs.constBegin())
					transactionQuery.append(" AND ");*/
				QString value = it.value().toString();
				if (value.isEmpty() || setPairs.contains(it.key()))
					continue;
				if (value.startsWith("'"))
					conditionsList.append(it.key() + " = " + value);
				else
					conditionsList.append(it.key() + " = '" + value + "'");
				/*if ((it + 1) != conditionPairs.constEnd())
					transactionQuery.append(" AND ");*/
			}
			transactionQuery.append(conditionsList.join(" AND "));
			transactionQuery.append(" ORDER BY id ASC LIMIT 1");
		}

		//LOG << transactionQuery;
		vector queryRes = ExecuteTargetSql(transactionQuery);
		//qDebug() << conditionPairs;
		//qDebug() << queryRes;
		if (queryRes.size() > 1) {
			for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
				QString key = it.key();
				if (it.value().isEmpty())
					continue;
				//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
				if (key == "inDate") {
					if (queryRes[1].contains("transDate"))
						queryRes[1]["transDate"] = "";
						//queryRes[1].remove("transDate");
					conditionPairs["transDate"] = it.value();
				}
				if (key == "inTime") {
					if (queryRes[1].contains("transTime"))
						queryRes[1]["transTime"] = "";
					conditionPairs["transTime"] = it.value();
				}
				conditionPairs[key] = it.value();
			}
		} else if (hadId || setPairs.contains("barcode")) {
			skipQuery = true;
			break;
		}
		else {
			QString transactionQuery = "SELECT * FROM cribemployeeitemtransactions WHERE ";

			QStringList conditionsList;

			for (auto it = setPairs.constBegin(); it != setPairs.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty())
					continue;
				if (value.startsWith("'"))
					conditionsList.append(it.key() + " = " + value);
				else
					conditionsList.append(it.key() + " = '" + value + "'");
				/*if ((it + 1) != setPairs.constEnd())
					transactionQuery.append(" AND ");*/
			}

			transactionQuery.append(conditionsList.join(" AND "));
			transactionQuery.append(" ORDER BY id ASC LIMIT 1");

			//LOG << transactionQuery;
			vector queryRes = ExecuteTargetSql(transactionQuery);
			//qDebug() << conditionPairs;
			//qDebug() << queryRes;
			if (queryRes.size() > 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
					if (key == "inDate") {
						if (queryRes[1].contains("transDate"))
							queryRes[1]["transDate"] = "";
							//queryRes[1].remove("transDate");
						conditionPairs["transDate"] = it.value();
					}
					if (key == "inTime") {
						if (queryRes[1].contains("transTime"))
							queryRes[1]["transTime"] = "";
						conditionPairs["transTime"] = it.value();
					}
					conditionPairs[key] = it.value();
				}
				//qDebug() << conditionPairs;
				//LOG << "Breakpoint";
			}
		}

		/*QStringList allowedKeyList = { "custId", "cribId", "toolId", "itemId", "barcode", "trailId", "tailId", "issuedBy", "returnBy", "transType", "inDate", "inTime" };*/
		

		if (conditionPairs.contains("transType") && conditionPairs.value("transType").toString() == "1") {
			if((!conditionPairs.contains("returnBy") || conditionPairs.value("returnBy").toString().isEmpty()) && conditionPairs.contains("userId")) {
				conditionPairs["returnBy"] = conditionPairs.value("userId");
			}
		}
		else {
			if ((!conditionPairs.contains("issuedBy") || conditionPairs.value("issuedBy").toString().isEmpty()) && conditionPairs.contains("userId")) {
				conditionPairs["issuedBy"] = conditionPairs.value("userId");
			}
		}

		if ((!conditionPairs.contains("toolId") || conditionPairs.value("toolId").toString().isEmpty()) && ((conditionPairs.contains("barcode") && !conditionPairs.value("barcode").toString().isEmpty()) || (conditionPairs.contains("itemId") && !conditionPairs.value("itemId").toString().isEmpty()))) {

			std::vector<std::string> conditions;
			if (conditionPairs.contains("barcode") && !conditionPairs.value("barcode").toString().isEmpty())
				conditions.push_back("barcodeTAG = '" + conditionPairs.value("barcode").toString().toStdString() + "'");
			if (conditionPairs.contains("itemId") && !conditionPairs.value("itemId").toString().isEmpty())
				conditions.push_back("itemId = '" + conditionPairs.value("itemId").toString().toStdString() + "'");

			std::vector<stringmap> returnedData = sqliteManager.GetEntry(
				"cribtools",
				{ "itemId", "serialNo", "toolId" },
				conditions
			);

			//qDebug() << returnedData;

			if (!returnedData.empty()) {
				QStringList transactionQueryList = { };

				if (returnedData[0].contains("itemId"))
					transactionQueryList.append(QString::fromStdString("PartNo LIKE '" + returnedData[0].at("itemId") + "'"));
				if (returnedData[0].contains("serialNo"))
					transactionQueryList.append(QString::fromStdString("serialNo LIKE '" + returnedData[0].at("serialNo") + "'"));
				if (returnedData[0].contains("toolId"))
					transactionQueryList.append(QString::fromStdString("id = '" + returnedData[0].at("toolId") + "'"));

				QString transactionQuery = "SELECT id as toolId FROM tools WHERE " + transactionQueryList.join(" OR ") + " LIMIT 1";

				//LOG << transactionQuery;
				vector queryRes = ExecuteTargetSql(transactionQuery);
				//qDebug() << queryRes;
				if (queryRes.size() > 1) {
					for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
						QString key = it.key();
						if (it.value().isEmpty())
							continue;
						//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
						conditionPairs[key] = it.value();
					}
				}
			}
		}

		if ((!conditionPairs.contains("itemId") || conditionPairs.value("itemId").toString().isEmpty()) && (conditionPairs.contains("barcode") && !conditionPairs.value("barcode").toString().isEmpty())) {

			std::vector<std::string> conditions;
			if (conditionPairs.contains("barcode") && !conditionPairs.value("barcode").toString().isEmpty())
				conditions.push_back("barcodeTAG = '" + conditionPairs.value("barcode").toString().toStdString() + "'");
			/*if (entry.contains("itemId") && !entry.value("itemId").toString().isEmpty())
				conditions.push_back("itemId = '" + entry.value("itemId").toString().toStdString() + "'");*/

			std::vector<stringmap> returnedData = sqliteManager.GetEntry(
				"cribtools",
				{ "itemId", "serialNo", "toolId" },
				conditions
			);

			//qDebug() << returnedData;

			if (!returnedData.empty()) {
				QStringList transactionQueryList = { };

				if (returnedData[0].contains("itemId"))
					transactionQueryList.append(QString::fromStdString("PartNo LIKE '" + returnedData[0].at("itemId") + "'"));
				if (returnedData[0].contains("serialNo"))
					transactionQueryList.append(QString::fromStdString("serialNo LIKE '" + returnedData[0].at("serialNo") + "'"));
				if (returnedData[0].contains("toolId"))
					transactionQueryList.append(QString::fromStdString("id = '" + returnedData[0].at("toolId") + "'"));

				QString transactionQuery = "SELECT PartNo as itemId FROM tools WHERE " + transactionQueryList.join(" OR ") + " LIMIT 1";

				//LOG << transactionQuery;
				vector queryRes = ExecuteTargetSql(transactionQuery);
				//qDebug() << queryRes;
				if (queryRes.size() > 1) {
					for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
						QString key = it.key();
						if (it.value().isEmpty())
							continue;
						//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
						conditionPairs[key] = it.value();
					}
				}
			}
		}

		/*
		if ((!conditionPairs.contains("itemId") || conditionPairs.value("itemId").toString().isEmpty()) && conditionPairs.contains("barcode")) {
			std::vector<stringmap> returnedData = sqliteManager.GetEntry(
				"cribtools",
				{ "itemId", "serialNo", "toolId" },
				{ "barcodeTAG = '" + conditionPairs.value("barcode").toString().toStdString() + "'" }
			);

			qDebug() << returnedData;
			
			// QString transactionQuery = "SELECT t.PartNo as itemId FROM cribtools AS ct LEFT JOIN tools AS t ON t.custId = ct.custId AND (ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo OR ct.toolId = t.id) WHERE ct.barcodeTAG = '" + conditionPairs.value("barcode").toString() + "' LIMIT 1";
			QString transactionQuery = QString::fromStdString("SELECT PartNo as itemId FROM tools WHERE PartNo LIKE '" + returnedData[0].at("itemId") + "' OR serialNo LIKE '" + returnedData[0].at("serialNo") + "' OR id = '" + returnedData[0].at("toolId") + "' LIMIT 1");

			LOG << transactionQuery;
			vector queryRes = ExecuteTargetSql(transactionQuery);
			qDebug() << queryRes;
			if (queryRes.size() > 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
					conditionPairs[key] = it.value();
				}
			}
		}
		*/

		if ((!conditionPairs.contains("tailId") || conditionPairs.value("tailId").toString().isEmpty()) && conditionPairs.contains("trailId")) {
			vector colsCheck = ExecuteTargetSql("SHOW KEYS FROM jobs WHERE Key_name = 'PRIMARY'");
			QString indexingCol = colsCheck[1].value("Column_name");

			QString transactionQuery = "SELECT description as tailId FROM jobs WHERE "+ indexingCol +" = '" + conditionPairs.value("trailId").toString() + "'";

			//LOG << transactionQuery;
			vector queryRes = ExecuteTargetSql(transactionQuery);
			//qDebug() << queryRes;
			if (queryRes.size() > 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
					conditionPairs[key] = it.value();
				}
			}
		}

		if ((!conditionPairs.contains("trailId") || conditionPairs.value("trailId").toString().isEmpty() || conditionPairs.value("trailId").toString() == "0") && conditionPairs.contains("tailId")) {
			vector colsCheck = ExecuteTargetSql("SHOW KEYS FROM jobs WHERE Key_name = 'PRIMARY'");
			QString indexingCol = colsCheck[1].value("Column_name");

			QString transactionQuery = "SELECT "+ indexingCol +" FROM jobs WHERE description LIKE '" + conditionPairs.value("tailId").toString() + "'";

			//LOG << transactionQuery;
			vector queryRes = ExecuteTargetSql(transactionQuery);
			//qDebug() << queryRes;
			if (queryRes.size() > 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
					conditionPairs[key] = it.value();
				}
			}
		}

		qDebug() << conditionPairs;

		if (!conditionPairs.contains("transDate")) {
			LOG << "breakpoint";
		}

		data.swap(conditionPairs);
		data["table"] = "cribtrak/transactions";

		break;
	}
	//case portaemployeeitemtransactions:
	//case lokkaemployeeitemtransactions:
	default:
		skipQuery = true;
		break;
	}

	if (skipQuery)
		return;

	if (data.value("table").toString().isEmpty())
		data["table"] = querySections[0].split(" ").at(1);

	QList<const char*> tablesToSkip = { "kabemployeeitemtransactions", "cribemployeeitemtransactions", "portaemployeeitemtransactions" };
	if (tablesToSkip.contains(targetTable)) {
		std::map<std::string, std::string> toolData;

		for (auto it = data.constBegin(); it != data.constEnd(); ++it)
		{
			QString key = it.key();
			QString val = it.value().toString().trimmed().simplified();
			if (val.isEmpty() || val == "''" || key == "table")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
		}

		//QList<const char*> tablesToSkip = { "kabemployeeitemtransactions", "cribemployeeitemtransactions", "portaemployeeitemtransactions" };
		//if (!tablesToSkip.contains(targetTable)) {
		//qDebug() << toolData;
		sqliteManager.AddEntry(
			targetTable.toStdString(),
			toolData
		);

		return;
	}
	else {
		//std::vector<std::string> setVector;
		stringmap setMap;
		for (auto it = setPairs.constBegin(); it != setPairs.constEnd(); ++it)
		{
			std::string key = it.key().toStdString();
			std::string val = it.value().toString().trimmed().simplified().toStdString();

			if (val.empty() || val == "''" || key == "table")
				continue;

			if (val.starts_with("'")) {
				setMap[key] = val;
			}
			else {
				setMap[key] = "'" + val + "'";
			}
			//setVector.push_back(set.toStdString());
			/*if (val.isEmpty() || val == "''" || key == "table")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();*/
		}

		//stringmap conditionMap;
		std::vector<std::string> conditions;
		for (auto it = queryConditionalSections.cbegin(); it != queryConditionalSections.cend(); ++it)
		{
			/*QString key = it.key();
			QString val = it.value().toString().trimmed().simplified();*/

			/*if (val.isEmpty() || val == "''" || key == "table")
				continue;*/
			/*if (!skipTargetCols.contains(key))
				conditionMap[key.toStdString()] = val.toStdString();*/
			conditions.push_back(it->trimmed().toStdString());
		}


		sqliteManager.UpdateEntry(
			targetTable.toStdString(),
			conditions,
			setMap
		);
	}

	/*sqliteManager.AddEntry(
		splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed().toStdString(),
		toolData
	);*/
	
	data["update"] = setPairs;
	data["where"] = conditionPairs;

	//qDebug() << data;

	query = returnVal.join(" ");
}

void DatabaseManager::processDeleteStatement(QString& query, QJsonObject& data, bool& skipQuery)
{
	
	QStringList splitQuery = query.split(" WHERE ", Qt::SkipEmptyParts, Qt::CaseInsensitive);
	//QStringList querySections = ServiceHelper::ExplodeString(query, splitBy.data());
	//qDebug() << splitQuery;
	if (splitQuery.size() > 2) {
		QStringList tempQuerySections = splitQuery;
		tempQuerySections.removeAt(0);
		splitQuery[1] = tempQuerySections.join(" WHERE ");
		splitQuery.remove(2, splitQuery.size() - 2);
	}
	//qDebug() << splitQuery;

	bool hasCustId = 0;
	bool hasTrakId = 0;
	bool hasId = 0;
	const char* idColumn = "id";

	QStringList queryConditionalSections = splitQuery[1].split("AND", Qt::SkipEmptyParts, Qt::CaseInsensitive);

	for (auto it = queryConditionalSections.cbegin(); it != queryConditionalSections.cend(); ++it)
	{
		QString conditionTrimmed = it->trimmed();
		QStringList colAndVal = conditionTrimmed.split(" ");

		QString key = colAndVal.at(0);
		QString val = conditionTrimmed.slice(colAndVal.at(0).length() + colAndVal.at(1).length() + 2);
		val = val.trimmed();
		if(key.startsWith("("))
			key.slice(1);
		if (val.endsWith(")"))
			val.slice(0, val.length() - 1);
		val = val.trimmed();
		if (val.startsWith("'"))
			val.slice(1);
		if (val.endsWith("'"))
			val.slice(0, val.length() - 1);
		val = val.trimmed();
		//qDebug() << key << ": " << val;
		//conditionPairs.insert(key, val);
		data[key] = val;
	}

	if (data.contains("custid")) {
		data["custId"] = data.value("custid");
		data.remove("custid");
	}

	hasCustId = data.contains("custId");
	hasTrakId = data.contains(trakId.data());
	if (data.keys().contains("id", Qt::CaseInsensitive) || data.keys().contains("toolId", Qt::CaseInsensitive)) {
		hasId = 1;
		idColumn = data.keys().contains("id", Qt::CaseInsensitive) ? "id" : "toolId";
	}

	switch (table_map[splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed()])
	{
	case tools:
	{
		if (!hasCustId)
			data["custId"] = custId;
		if (hasId && idColumn == "id" && (!data.contains("toolId") || data.value("toolId").toString().isEmpty()))
			data["toolId"] = data.value("id").toString();

		break;
	}
	case users:
	{

		if (!data.contains("userId")) {
			QString targetQuery = "SELECT userId FROM users WHERE ";
			QStringList conditionals;
			for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
				QString key = it.key();
				QString value = it.value().toString().trimmed();
				if (value.isEmpty())
					continue;
				conditionals.append(key + " = " + value);
			}
			targetQuery.append(conditionals.join(" AND "));

			std::vector results = ExecuteTargetSql(targetQuery);
			if (results.size() > 1) {
				for (auto it = results[1].cbegin(); it != results[1].cend(); ++it) {
					QString key = it.key();
					QString value = it.value();
					if (value.isEmpty())
						continue;

					data[key] = value;
				}
			}
		}

		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;

		break;
	}
	case employees:
	{
		if (!data.contains("userId")) {
			QString targetQuery = "SELECT userId FROM employees WHERE ";
			QStringList conditionals;
			for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
				QString key = it.key();
				QString value = it.value().toString().trimmed();
				if (value.isEmpty())
					continue;
				conditionals.append(key + " = " + value);
			}
			targetQuery.append(conditionals.join(" AND "));

			std::vector results = ExecuteTargetSql(targetQuery);
			if (results.size() > 1) {
				for (auto it = results[1].cbegin(); it != results[1].cend(); ++it) {
					QString key = it.key();
					QString value = it.value();
					if (value.isEmpty())
						continue;

					data[key] = value;
				}
			}
		}

		if (!hasCustId)
			data["custId"] = custId;

		break;
	}
	case jobs:
	{
		if (!hasCustId)
			data["custId"] = custId;

		if (data.contains("trailId"))
			break;

		QString jobQuery = "SELECT * FROM jobs WHERE ";
		for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
			QString value = it.value().toString();
			if (value.isEmpty())
				continue;
			if (value.startsWith("'"))
				jobQuery.append(it.key() + " = " + value);
			else
				jobQuery.append(it.key() + " = '" + value + "'");
			if ((it + 1) != data.constEnd())
				jobQuery.append(" AND ");
		}
		//qDebug() << jobQuery;
		vector fetchedJob = ExecuteTargetSql(jobQuery);

		if (fetchedJob.size() <= 1) {
			skipQuery = true;
			break;
		}

		for (auto it = fetchedJob[1].cbegin(); it != fetchedJob[1].cend(); ++it) {
			if (data.contains(it.key()) || it.value().isEmpty())
				continue;
			data[it.key()] = it.value();
		}

		break;
	}
	case kabs:
	{
		data["table"] = "kabtrak";
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;
		
		break;
	}
	case drawers:
	{
		data["table"] = "kabtrak/drawers";
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;
		
		break;
	}
	case toolbins:
	{

		data["table"] = "kabtrak/tools";
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;

		break;
	}
	//case cribs:
	//case cribconsumables:
	//case cribtoollocation:
	//case cribtoollockers:
	//case cribtools:
	//case kittools:
	//case tooltransfer:
	case itemkits: {
		data["table"] = "portatrak/kit";
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;
		break;
	}
	case kitcategory: {
		data["table"] = "portatrak/kit/category";
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;
		break;
	}
	case kitlocation: {
		data["table"] = "portatrak/kit/location";
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;
		break;
	}
	//case kabemployeeitemtransactions:
	//case cribemployeeitemtransactions:
	//case portaemployeeitemtransactions:
	//case lokkaemployeeitemtransactions:
	default:
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;

		break;
	}

	if (skipQuery)
		return;

	if (data.value("table").toString().isEmpty())
		data["table"] = splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed();

	//qDebug() << data;
	return;
}

void DatabaseManager::performCleanup()
{
	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}
	if (netManager) {
		netManager->deleteLater();
		netManager = nullptr;
	}
}
