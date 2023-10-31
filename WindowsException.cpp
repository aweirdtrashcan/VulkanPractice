#include "WindowsException.h"

#include <sstream>
#include <string>

WindowsException::WindowsException(const char* message, const char* codeSection, const char* fileName, int line)
{
	std::stringstream buffer;
	buffer << message << "\n" << "[Filename]: " << fileName << " at " << line << "\n";
	buffer << "[Code Section]: " << codeSection;
	std::string str = buffer.str();
	errorMessage = (const char*)malloc(str.size());
	memcpy((void*)errorMessage, str.c_str(), str.size());
	heapAllocated = true;
}

WindowsException::~WindowsException()
{
	if (heapAllocated)
	{
		free((void*)errorMessage);
	}
}
