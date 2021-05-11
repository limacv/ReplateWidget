#pragma once

class MLStepDataBase
{
public:
	MLStepDataBase()
		: expired(false)
	{}
	virtual ~MLStepDataBase() {}

private:
	bool expired;
};

