# HenchmanService

HenchmanService is a Windows service that provides functionality for managing and controlling the HenchmanTRAK application. It is written in C++ and utilizes the OpenSSL library for SSL/TLS communication.

## Features

- Installs, controls, and manages the HenchmanService.
- Allows the service to be installed, started, stopped, and deleted.
- Connects the various features from other scripts into a centralized point.

## Getting Started

To get started with HenchmanService, follow these steps:

1. Clone the repository: `git clone https://github.com/HenchmanTRAK/HenchmanService.git`.
2. Move yourself into it, eg. `cd HenchmanService`.
2. Build the project using `cmake --build .`.
3. Create a service.ini file and define the required fields.
4. Install the service by running the executable with the `--install` argument.
5. Start the service by running the executable with the `--start` argument.

## Dependencies

HenchmanService requires the following dependencies to build and run:

 - OpenSSL library: Used for SSL/TLS communication.
 - Qt6: Used for various aspects of the application such as interacting with a database and network requests.
 - Doxygen: For generating documentation at build time.

## Usage

To use HenchmanService, you can interact with it through the command line or through the Windows Service Manager.

### Command Line

To interact with HenchmanService through the command line, you can use the following commands:

 - `--install`: Installs the service.
 - `--remove`: Removes the service.
 - `--start`: Starts the service.
 - `--stop`: Stops the service.

### Windows Service Manager

To interact with HenchmanService through the Windows Service Manager, you can follow these steps:

1. Open the Windows Service Manager.
2. Look for the "HenchmanService" service.
3. Perform the desired action (start, stop, etc.) on the service.

## Configuration

HenchmanService requires a configuration file in .ini format to function properly. The configuration file should be named `HenchmanService.ini` and should be placed in the same directory as the executable.

The following is an example of what needs to be included in the .ini file:

```ini
[EMAIL]
Username = your_mail_username
Password = your_mail_password

[WAMP]
MySQL_DIR = path_to_mysql_exe
Apache_DIR = path_to_apache_exe
PHP_DIR = path_to_php_exe

[TRAK]
TRAK_DIR = path_to_trak_executable
INI_FILE = name_of_trak_ini
EXE_FILE = name_of_trak_exe
APP_NAME = name_of_trak_application

[DEVELOPMENT]
testingMain=0|1
testingDBManager=0|1
```

 - `[EMAIL]` section:
	- `Username` key: Specifies the username for the mail login.
	- `Password` key: Specifies the password for the mail login.

 - `[WAMP]` section:
	- `MySQL_DIR` key: Specifies the path to the folder that contains the MySQL executable.
	- `Apache_DIR` key: Specifies the path to the folder that contains the Apache executable.
	- `PHP_DIR` key: Specifies the path to the folder that contains the PHP executable.

 - `[TRAK]` section:
	- `TRAK_DIR` key: Specifies the path to the TRAK executable.
	- `INI_FILE` key: Specifies the name of the TRAK ini file.
	- `EXE_FILE` key: Specifies the name of the TRAK executable.
	- `APP_NAME` key: Specifies the name of the TRAK application.

 - `[DEVELOPMENT]` section:
	- `testingMain` key: 0 or 1; specifies if app should build in testing mode
	- `testingDBManager` key: 0 or 1; specifies if app db manager should build in testing mode

Make sure to replace include outlined with the appropriate values for your setup to work.

## Troubleshooting

If you encounter any issues while using HenchmanService, here are some troubleshooting tips:

 - Make sure the configuration file (service.ini) is present and properly configured.
 - Check the log files for any error messages or warnings.
 - Verify that the required dependencies are installed.
 - Check the system logs for any related errors.

## Contributing

Contributions are welcome! If you find any issues or have suggestions for improvements, please open an issue or submit a pull request.

## License

HenchmanService is licensed under the [MIT License](LICENSE).

## Contact

For any questions or inquiries, you can reach out to [wjaco.swanepoel@gmail.com](mailto:wjaco.swanepoel@gmail.com).

I hope this helps! Let me know if you need any further assistance.
