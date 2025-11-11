#pragma once

class Environment {
private:

  	unsigned long inputCP, outputCP;
	unsigned long mode, outputMode;
	bool err;
public:
	Environment();
	~Environment();
	bool check();
};