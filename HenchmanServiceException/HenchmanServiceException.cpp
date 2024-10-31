

#include "HenchmanServiceException.h"

using namespace std;

const char * HenchmanServiceException::what() const {

	EventManager("HenchmanService").ReportCustomEvent(functionName.data(), errorMessage);
	return errorMessage.c_str();
}

//HenchmanServiceException::HenchmanServiceException(string msg, string function)
//cout << caller.file_name() << " : " << caller.line() << " : " << caller.function_name() << endl;

HenchmanServiceException::HenchmanServiceException(
	string msg, 
	const source_location& location
)
{
	caller = location;

	int substringStart = string(caller.function_name()).find_first_of(" ")+1;
	int substringEnd = string(caller.function_name()).find_first_of("(")-substringStart;
	vector exploded = ExplodeString((
		substringStart >= substringEnd
		? __FUNCTION__
		: string(caller.function_name())
		.substr(
			substringStart,
			substringEnd
		)), " ");

	exploded = ExplodeString(exploded[exploded.size() - 1], "::", 2);
	functionName = "";
	for (int i = 0; i < exploded.size(); i++)
	{
		functionName.append(exploded[i]);
		if (i < exploded.size() - 1)
			functionName.append("::");
	}

	errorMessage = msg;
}


