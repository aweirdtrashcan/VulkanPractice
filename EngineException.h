#pragma once

#include <exception>

class EngineException : public std::exception
{
public:
	EngineException(const char* file, int line, const char* reason);
	~EngineException() = default;

	virtual const char* what() const noexcept { return whatStr; }
	constexpr const char* GetType() const noexcept { return "Engine Exception"; }

private:
	char whatStr[3000];
};

