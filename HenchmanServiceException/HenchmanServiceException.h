#ifndef HENCHMAN_SERVICE_EXCEPTION_H
#define HENCHMAN_SERVICE_EXCEPTION_H
#pragma once

#include <iostream>
#include <exception>

#include "EventManager.h"


class HenchmanServiceException : public std::exception {
private:
	std::string errorMessage;

public:
	HenchmanServiceException(std::string msg);
	const char * what() const override;
};

#endif // !HENCHMAN_SERVICE_EXCEPTION_H
