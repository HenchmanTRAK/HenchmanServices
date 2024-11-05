#ifndef SERVICE_HELPER_H
#define SERVICE_HELPER_H
#pragma once


#include <array>
#include <iostream>
#include <source_location>
#include <string>
#include <vector>

#include <filesystem>
#include <fstream>

#include <QString>
#include <QList>

#include "HenchmanServiceException.h"
#include "RegistryManager.h"



//char* base64(std::string string);
//
//char* decodeBase64(std::string string);

/**
 * @class ServiceHelper
 *
 * @brief The ServiceHelper class provides utility functions for various tasks.
 *
 * This class provides methods for tasks such as exploding strings, getting file contents, and writing to logs.
 *
 * @author Willem Swanepoel
 * @version 1.0
 *
 * @details
 * - The ServiceHelper class provides a convenient way to perform various tasks within your application.
 *
 * @see ServiceHelper::ExplodeString
 * @see ServiceHelper::get_file_contents
 * @see ServiceHelper::WriteLog
 */
class ServiceHelper
{
public:
	/**
	 * @brief Constructs a ServiceHelper object.
	 *
	 * This constructor initializes the ServiceHelper object with the specified source location.
	 *
	 * @param caller The source location of the caller.
	 */
	ServiceHelper(const std::source_location& caller = std::source_location::current());

	/**
	 * @brief Returns a timestamp as a std::array<std::string, 2>.
	 *
	 * The first element of the array is the current date in the format "YYYY-MM-DD".
	 * The second element of the array is the current time in the format "HH:MM:SS".
	 *
	 * @return A std::array<std::string, 2> containing the timestamp.
	 *
	 * @throws None.
	 */
	std::array<std::string, 2> timestamp();

	/**
	 * @brief Returns the exports path for the given application path.
	 *
	 * If the app_path parameter is empty, the function returns the default exports path.
	 * Otherwise, it returns the exports path for the given application path.
	 *
	 * @param app_path - The path of the application (optional).
	 *
	 * @return The exports path for the given application path.
	 *
	 * @throws None.
	 */
	std::string GetExportsPath(std::string app_path = "");

	/**
	 * @brief Returns the logs path for the given application path.
	 *
	 * If the app_path parameter is empty, the function returns the default logs path.
	 * Otherwise, it returns the logs path for the given application path.
	 *
	 * @param app_path - The path of the application (optional).
	 *
	 * @return The logs path for the given application path.
	 *
	 * @throws None.
	 */
	std::string GetLogsPath(std::string app_path = "");

	/**
	 * @brief Writes the given log message to the log file.
	 *
	 * The log message is written to the log file with the current timestamp and log level.
	 *
	 * @param log - The log message to write.
	 *
	 * @throws None.
	 */
	void WriteToLog(std::string log);

	/**
	 * @brief Writes the given error message to the error file.
	 *
	 * The error message is written to the error file with the current timestamp and log level.
	 *
	 * @param log - The error message to write.
	 *
	 * @throws None.
	 */
	void WriteToError(std::string log);

	/**
	 * @brief Writes the given log message to the custom log file.
	 *
	 * The log message is written to the custom log file with the current timestamp and log level.
	 *
	 * @param log - The log message to write.
	 * @param logName - The name of the custom log file.
	 *
	 * @throws None.
	 */
	void WriteToCustomLog(std::string log, std::string logName);

	/**
	 * @brief Returns the current time in microseconds.
	 *
	 * @return The current time in microseconds.
	 *
	 * @throws None.
	 */
	long int microseconds();

	/**
	 * @brief Checks if the given string contains the search string.
	 *
	 * @param str - The string to search in.
	 * @param search - The string to search for.
	 *
	 * @return True if the string contains the search string, false otherwise.
	 *
	 * @throws None.
	 */
	bool Contain(QString str, QString search);

	/**
	 * @brief Returns the base name of the given file path.
	 *
	 * @param path - The file path.
	 *
	 * @return The base name of the file path.
	 *
	 * @throws None.
	 */
	static std::string fileBasename(const std::string& path);

	/**
	 * @brief Reads the contents of a file and returns them as a C-style string.
	 *
	 * @param filename - The name of the file to read.
	 *
	 * @return The contents of the file as a C-style string.
	 *
	 * @throws None.
	 */
	const char* get_file_contents(const char* filename);

	/**
	 * @brief Returns the file extension of the given file name.
	 *
	 * @param FileName - The name of the file.
	 *
	 * @return The file extension of the given file name.
	 *
	 * @throws None.
	 */
	const char* GetFileExtension(const QString& FileName);

	// String Sanatizer provided by Simple on Stackoverflow
	// https://stackoverflow.com/a/34221488
	/**
	 * @brief Sanitizes a string by removing any characters that are not alphanumeric or underscores.
	 * 
	 * String Sanatizer provided by Simple on Stackoverflow 
	 * https://stackoverflow.com/a/34221488
	 *
	 * @param stringValue - The string to sanitize.
	 *
	 * @throws None.
	 */
	void sanitize(std::string& stringValue);

	/**
	 * @brief Removes quotes from a string.
	 *
	 * @param stringValue - The string from which to remove quotes.
	 *
	 * @throws None.
	 */
	void removeQuotes(std::string& stringValue);

	/**
	 * @brief Explodes a string into a vector of substrings based on a specified separator.
	 *
	 * This function takes a string and a separator as input, and returns a vector of substrings.
	 *
	 * @param targetString The string to explode.
	 * @param seperator The separator to use.
	 * @param maxLen The maximum number of substrings to return.
	 *
	 * @return A vector of substrings.
	 *
	 * @throws Throws an exception if the input string or separator is invalid.
	 */
	static std::vector<std::string> ExplodeString(std::string targetString, const char seperator[], int maxLen = -1);

	static QList<QString> ExplodeString(QString targetString, const char seperator[], int maxLen = -1);

private:
	std::string functionName;

	void WriteLog(char* targetFile, std::string log);

};

#endif