

#include "HenchmanServiceException.h"

using namespace std;

static string getCallerFunctionName(const source_location& location)
{
	int substringStart = string(location.function_name()).find_first_of(" ") + 1;
	int substringEnd = string(location.function_name()).find_first_of("(") - substringStart;
	vector exploded = ServiceHelper::ExplodeString((
		substringStart >= substringEnd
		? __FUNCTION__
		: string(location.function_name())
		.substr(
			substringStart,
			substringEnd
		)), " ");

	exploded = ServiceHelper::ExplodeString(exploded[exploded.size() - 1], "::", 2);
	string functionName = "";
	for (int i = 0; i < exploded.size(); i++)
	{
		functionName.append(exploded[i]);
		if (i < exploded.size() - 1)
			functionName.append("::");
	}
	return functionName;
}

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
	functionName = getCallerFunctionName(location);
	errorMessage = msg;
}

//HenchmanServiceException::HenchmanServiceException(
//	wstring msg,
//	const source_location& location
//)
//{
//	functionName = getCallerFunctionName(location);
//	errorMessage = string(msg.begin(), msg.end());
//}

