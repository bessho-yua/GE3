#include "Logger.h"
#include<ostream>
#include<Windows.h>

//Log関数
namespace Logger {

	void Log(std::ostream& os, const std::string& message) {
		os << message << std::endl;
		OutputDebugStringA(message.c_str());
	}

}
