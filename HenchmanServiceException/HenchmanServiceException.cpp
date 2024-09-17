

#include "HenchmanServiceException.h"


const char * HenchmanServiceException::what() const {
	return errorMessage.c_str();
}

HenchmanServiceException::HenchmanServiceException(string msg) : errorMessage(msg) {}
