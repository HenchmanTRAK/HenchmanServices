#ifndef HENCHMAN_SERVICE_EXCEPTION_H
#define HENCHMAN_SERVICE_EXCEPTION_H
#pragma once

#include <exception>
#include <iostream>
#include <source_location>

#include "EventManager.h"
#include "ServiceHelper.h"

/**
 * @class HenchmanServiceException
 *
 * @brief The HenchmanServiceException class represents an exception that occurs in the HenchmanService application.
 *
 * This class provides a way to handle and report exceptions that occur during the execution of the HenchmanService application.
 *
 * @author Willem Swanepoel
 * @version 1.0
 *
 * @details
 * - The HenchmanServiceException class inherits from the std::exception class.
 * - The class provides a way to report custom events using the EventManager class.
 */
class HenchmanServiceException : public std::exception {
private:
	/**
	 * @var errorMessage
	 *
	 * @brief The error message associated with the exception.
	 */
	tstring errorMessage;

	/**
	 * @var functionName
	 *
	 * @brief The name of the function where the exception occurred.
	 */
	tstring functionName;

	/**
	 * @var caller
	 *
	 * @brief The source location of the exception.
	 */
	std::source_location caller;

public:

	/**
	 * @brief Constructs a HenchmanServiceException object.
	 *
	 * This constructor initializes the HenchmanServiceException object with the specified error message and source location.
	 *
	 * @param msg The error message.
	 * @param location The source location of the exception.
	 */
	HenchmanServiceException(
		std::string msg,
		const std::source_location& location = std::source_location::current()
	);

	HenchmanServiceException(
		std::wstring msg,
		const std::source_location& location = std::source_location::current()
	);

	/**
	 * @brief Returns a string representation of the exception.
	 *
	 * This member function returns a string representation of the exception, including the error message and source location.
	 *
	 * @return A string representation of the exception.
	 */
	const char * what() const override;


	const TCHAR *what(EventManager::CEventManager& evntManager) const;
};

#endif // !HENCHMAN_SERVICE_EXCEPTION_H
