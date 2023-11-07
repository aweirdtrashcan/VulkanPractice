#include "EngineException.h"

#include <memory>

EngineException::EngineException(const char* file, int line, const char* reason)
{
	memset(whatStr, 0, sizeof(whatStr));
	snprintf(whatStr, sizeof(whatStr), "Failure at file: %s:%d\nReason: %s", file, line, reason);
}
