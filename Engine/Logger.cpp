#include "Logger.h"
#include<ostream>
#include<Windows.h>

//Log関数
namespace Logger {
	void Log(std::ostream& os, const std::string& message) {
		//string->wstring
		std::wstring ConvertString(const std::string & str);
		//wstring->string
		std::string ConvertString(const std::wstring & str);
		os << message << std::endl;
		OutputDebugStringA(message.c_str());
	}

}
