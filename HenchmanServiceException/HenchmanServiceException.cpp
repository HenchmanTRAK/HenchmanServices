

#include "HenchmanServiceException.h"
using namespace std;



const char * HenchmanServiceException::what() const {
	return errorMessage.c_str();
}

HenchmanServiceException::HenchmanServiceException(string msg) : errorMessage(msg) {}
