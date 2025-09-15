#pragma once
#include <iostream>

#define PB_LOG(message) printf("PARABLIT LOG: "); printf(message); printf("\n")
#define PB_LOG_FORMAT(format, ...) printf("PARABLIT LOG: "); printf(format, __VA_ARGS__); printf("\n")

#define PB_NOT_IMPLEMENTED printf("PARABLIT: NOT IMPLEMENTED"); __debugbreak()