#pragma once
#include<string>

namespace StringUtility 
{
	//string->wstring
	std::wstring ConvertString(const std::string& str);
	//wstring->string
	std::string ConvertString(const std::wstring& str);
}

