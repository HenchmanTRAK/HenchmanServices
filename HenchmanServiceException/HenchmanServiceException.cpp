

#include "HenchmanServiceException.h"

using namespace std;

static tstring getCallerFunctionName(const source_location& location)
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
#ifdef UNICODE
	return ServiceHelper::s2ws(functionName);
#else
	return functionName;
#endif
}

const char *HenchmanServiceException::what() const {

	//EventManager("HenchmanService").ReportCustomEvent(functionName.data(), errorMessage);
	//EventManager::CEventManager("HenchmanService").ReportCustomEvent(functionName.data(), errorMessage.data(), 3);
#ifdef UNICODE
	return ServiceHelper::ws2s(errorMessage).data();
#else
	return errorMessage.data();
#endif
}

const TCHAR* HenchmanServiceException::what(EventManager::CEventManager& evntManager) const {

	//EventManager("HenchmanService").ReportCustomEvent(functionName.data(), errorMessage);
	evntManager.ReportCustomEvent(functionName.data(), errorMessage.data(), 3);
	return errorMessage.data();
}

//HenchmanServiceException::HenchmanServiceException(string msg, string function)
//cout << caller.file_name() << " : " << caller.line() << " : " << caller.function_name() << endl;

HenchmanServiceException::HenchmanServiceException(
	std::string msg,
	const source_location& location
)
	:errorMessage(t2tstr(msg)),
	functionName(getCallerFunctionName(location))
{
	/*functionName = getCallerFunctionName(location);
	errorMessage = msg;*/
}

HenchmanServiceException::HenchmanServiceException(
	std::wstring msg,
	const source_location& location
) 
	:errorMessage(t2tstr(msg)),
	functionName(getCallerFunctionName(location))
{

}

//HenchmanServiceException::HenchmanServiceException(
//	wstring msg,
//	const source_location& location
//)
//{
//	functionName = getCallerFunctionName(location);
//	errorMessage = string(msg.begin(), msg.end());
//}

