#pragma once
#include <iostream>
#include <thread>
#include <chrono>
#include <functional>

#include <cstdint>
#include <cstdio>
#include <vector>

class BonusTimer
{
	bool clear = false;

	public:
		void setTimeout(std::function<void(void)> function, int delay);
		void setInterval(std::function<void(void)> function, int interval);
		void stop();
};
