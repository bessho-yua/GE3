#pragma once
#include<ostream>
#include<string>

//ログ出力
namespace Logger {
	void Log(std::ostream& os, const std::string& message);
}