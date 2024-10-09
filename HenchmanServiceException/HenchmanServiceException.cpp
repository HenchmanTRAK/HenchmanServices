

#include "HenchmanServiceException.h"

using namespace std;

const char * HenchmanServiceException::what() const {
	EventManager("HenchmanService").ReportCustomEvent("HenchmanServiceException", errorMessage);
	return errorMessage.c_str();
}

HenchmanServiceException::HenchmanServiceException(string msg)
{
	errorMessage = msg;
}


