#ifndef CONTROL_HPP_
#define CONTROL_HPP_

#include <opencv2/core/core.hpp>

#include <string>
#include <iostream>
#include <unordered_map>

struct show_params_t {
	float contrast = 1.f;
	float brightness = 0.f;
};

struct show_status_t {
	show_params_t params;
	cv::Mat image;
};

struct control_panel_t {
	typedef std::unordered_map<std::string, show_status_t> show_status_by_name_t;

	std::unordered_map<std::string, show_status_by_name_t> show_status;
};

void show(control_panel_t& panel, std::string category, std::string name, cv::Mat image, show_params_t params = show_params_t());

bool will_show(control_panel_t& panel, std::string category, std::string name);

template<typename T>
struct trackbar_params_t {
	T start;
	T end;
	T step;
};

template<typename T>
void trackbar(control_panel_t& panel, std::string category, std::string name, T& variable, trackbar_params_t<T> params);

void dump_time(control_panel_t& panel, std::string category, std::string name);

enum log_level_t {
	DEBUG,
	VERBOSE,
	INFO,
	WARNING,
	ERROR,
	CRITICAL
};

std::ostream& logger(control_panel_t& panel, std::string category, log_level_t level = INFO);

#endif /* CONTROL_HPP_ */
