#pragma once
#include <iostream>

#define REFLECTRON_LOG(...) std::cout << "[Reflectron] "; printf_s(__VA_ARGS__); std::cout << std::endl;