#ifndef HENCHMAN_SERVICE_EXCEPTION_H
#define HENCHMAN_SERVICE_EXCEPTION_H
#pragma once

#include <iostream>
using namespace std;

class HenchmanServiceException : public exception {
private:
	string errorMessage;
public:
	HenchmanServiceException(string msg);
	const char * what() const override;
};

#endif // !HENCHMAN_SERVICE_EXCEPTION_H
