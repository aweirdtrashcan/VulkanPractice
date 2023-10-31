#pragma once

#include <exception>

class WindowsException : public std::exception
{
public:
	explicit WindowsException(const char* message)
		:
		errorMessage(message)
	{}

	WindowsException(const char* message, const char* codeSection, const char* fileName, int line);

	~WindowsException();

	virtual const char* what() const noexcept override
	{
		return errorMessage;
	}

private:
	const char* errorMessage;
	bool heapAllocated = false;
};

