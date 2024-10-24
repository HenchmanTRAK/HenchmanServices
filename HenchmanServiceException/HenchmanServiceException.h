#ifndef HENCHMAN_SERVICE_EXCEPTION_H
#define HENCHMAN_SERVICE_EXCEPTION_H
#pragma once

#include <iostream>
#include <exception>
#include <source_location>

#include "EventManager.h"


class HenchmanServiceException : public std::exception {
private:
	std::string errorMessage;
	std::string functionName;
	std::source_location caller;

public:
	//HenchmanServiceException(std::string msg, std::string function = "HenchmanServiceException");
	HenchmanServiceException(
		std::string msg, 
		const std::source_location& location = std::source_location::current()
	);

	const char * what() const override;
};

#endif // !HENCHMAN_SERVICE_EXCEPTION_H
