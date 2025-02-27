#include "HenchmanServiceController.h"

int main()
{
	std::string service = "HenchmanService";
	std::cout << "\n";

	std::cout << "Select an option below;\n";
	std::cout << "	[1] Install service\n";
	std::cout << "	[2] Start service\n";
	std::cout << "	[3] Stop service\n";
	std::cout << "	[4] Remove service\n";
	std::cout << "Or use CTRL+C to exit...\n";
	service.append(".exe");
	int c = getchar();
	if (c == '\n' || c == EOF)
		return 0;
	if (c == '1')
		ServiceHelper::ShellExecuteApp(service, " --install");
	if (c == '2')
		ServiceHelper::ShellExecuteApp(service, " --start");
	if (c == '3')
		ServiceHelper::ShellExecuteApp(service, " --stop");
	if (c == '4')
		ServiceHelper::ShellExecuteApp(service, " --remove");
	
	std::cout << "Press key to exit..." << std::endl;
	c = getchar();
	return 0;
}