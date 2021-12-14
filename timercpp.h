#include <iostream>
#include <thread>
#include <chrono>
#include <functional>

#include <cstdint>
#include <cstdio>
#include <vector>


class TimerCustom {
	bool clear = false;

public:
	void setTimeout(std::function<void(void)> function, int delay);
	void setInterval(std::function<void(void)> function, int interval);
	void stop();

};



void TimerCustom::setTimeout(std::function<void(void)> function, int delay) {
	this->clear = false;
	std::thread t([=]() {
		if (this->clear) return;
		std::this_thread::sleep_for(std::chrono::milliseconds(delay));
		if (this->clear) return;
		function();
		});
	t.detach();
}



void TimerCustom::setInterval(std::function<void(void)>  function, int interval) {
	this->clear = false;
	std::thread t([=]() {
		while (true) {
			if (this->clear) return;
			std::this_thread::sleep_for(std::chrono::milliseconds(interval));
			if (this->clear) return;
			function();
		}
		});
	t.detach();
}


void TimerCustom::stop() {
	this->clear = true;
}