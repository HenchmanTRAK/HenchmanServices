

#include "HenchmanServiceException.h"

using namespace std;

const char * HenchmanServiceException::what() const {

	EventManager("HenchmanService").ReportCustomEvent(functionName.data(), errorMessage);
	return errorMessage.c_str();
}

//HenchmanServiceException::HenchmanServiceException(string msg, string function)
HenchmanServiceException::HenchmanServiceException(
	string msg, 
	const source_location& location
)
{
	caller = location;
	cout << caller.file_name() << " : " << caller.line() << " : " << caller.function_name() << endl;

	int substringStart = string(caller.function_name()).find_first_of(" ")+1;
	int substringEnd = string(caller.function_name()).find_first_of("(")-substringStart;
	if (substringStart >= substringEnd)
		functionName = __FUNCTION__;
	else
		functionName = 
			string(caller.function_name())
			.substr(
				substringStart,
				substringEnd
			);
	if (functionName[0] == '_')
	{
		functionName =
			functionName
			.substr(
				functionName.find_first_of(" ")+1,
				functionName.size()
			);
	}

	errorMessage = msg;
}


