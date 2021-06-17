#pragma once
#include <string>

class MLProgressObserverBase
{
public:
	MLProgressObserverBase() {}
	virtual ~MLProgressObserverBase() {}

	virtual void beginStage(const std::string& name) = 0;
	virtual void setValue(float value) = 0;
	virtual bool wasCanceled() { return false; };
};