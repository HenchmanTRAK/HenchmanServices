#pragma once

#include <SimpleIni.h>
#include "DatabaseManager.h"
#include <exception>
#include <string>
#include <qobject.h>
#include <qtmetamacros.h>

enum Trak_Type {
	unknown = 0,
	kabtrak = 1,
	cribtrak = 2,
	portatrak = 3
};

/**
 * @class TRAKManager
 *
 * @brief The TRAKManager class is responsible for managing the TRAK application and its associated functionality.
 *
 * This class provides methods for checking if the TRAK application exists, saving INI file contents to the Windows registry, and connecting to a local MySQL database.
 *
 * @author Willem Swanepoel
 * @version 1.0
 */
class TRAKManager : public QObject
{
	Q_OBJECT
private:

	/**
	 * @brief A pointer to a DatabaseManager object.
	 *
	 * This variable is used to manage the database connection.
	 */
	//DatabaseManager *databaseManager = nullptr;

	/**
	 * @brief The type of the TRAK application.
	 *
	 * This variable stores the type of the TRAK application.
	 */
	Trak_Type traktype = unknown;

	/**
	 * @brief Checks if the TRAK application exists by reading the TRAK_DIR, INI_FILE, EXE_FILE, and APP_NAME values from the given CSimpleIniA object.
	 *
	 * If the TRAK_DIR exists, it sets the appDir, iniFile, appName, and appType variables. It then writes a log message indicating the appDir and iniFile being used. Returns true if the TRAK_DIR exists, false otherwise.
	 *
	 * @param ini The CSimpleIniA object to read values from.
	 *
	 * @return True if the TRAK_DIR exists, false otherwise.
	 *
	 * @throws None.
	 */
	bool TRAKExists(CSimpleIniA& ini);
	
	/*bool kabTRAKExists();
	bool portaTRAKExists();
	bool cribTRAKExists();*/

	/**
	 * @brief Saves the INI file contents to the Windows registry.
	 *
	 * This function saves the INI file contents to the Windows registry.
	 *
	 * @throws None.
	 */
	void saveINIToRegistry();
	
	/**
	 * @brief Saves the INI file contents to the Windows registry for a specific section.
	 *
	 * This function saves the INI file contents to the Windows registry for a specific section.
	 *
	 * @param section The section to save the INI file contents for.
	 *
	 * @throws None.
	 */
	void saveINIToRegistry(std::string section);

	void conHenchmanAfterConnect();
	void conHenchmanAfterDisconnect();
	void conHenchmanConnectionLost();
	void conHenchmanError(std::exception& e);
	void conRemoteAfterConnect();
	void conRemoteAfterDisconnect();
	void conRemoteConnectionLost();
	void conRemoteError(std::exception& e);

	/**
	 * @brief Exports the general tables from the database.
	 *
	 * This function exports the general tables from the database.
	 *
	 * @return The number of tables exported.
	 *
	 * @throws None.
	 */
	int exportGeneralTables(DatabaseManager& databaseManager);

public:
	/**
	* @brief The type of the TRAK application.
	*/
	std::string appType;

	/**
	* @brief The directory of the TRAK application.
	*/
	std::string appDir;

	/**
	* @brief The name of the INI file of the TRAK application.
	*/
	std::string iniFile;

	/**
	* @brief The name of the TRAK application.
	*/
	std::string appName;
	
	/**
	 * @brief Constructs a new TRAKManager object.
	 *
	 * This constructor initializes the TRAKManager object with the given parameters.
	 *
	 * @param dbManager A pointer to a DatabaseManager object. This variable is used to manage the database connection.
	 *
	 * @throws None.
	 */
	TRAKManager();

	/**
	 * @brief Destructor for the TRAKManager class.
	 *
	 * This destructor releases any resources held by the TRAKManager object.
	 *
	 * @throws None.
	 */
	~TRAKManager();

	/**
	 * @brief Creates a data module by loading an INI file and adding its contents to the registry.
	 *
	 * This function loads an INI file from the Windows registry and checks if the TRAK application exists.
	 * If the TRAK application exists, it loads the INI file and adds its contents to the registry under specific sections.
	 *
	 * @throws HenchmanServiceException - if there is an error loading the INI file or executing the SQL script
	 *
	 */
	void CreateDataModule();

	/**
	 * @brief Uploads the current local db state to the remote server.
	 *
	 * This function uploads the current state of the local database to the remote server.
	 *
	 * @return The result of the upload operation.
	 *
	 * @throws None.
	 */
	int UploadCurrentStateToRemote(DatabaseManager& databaseManager);
};