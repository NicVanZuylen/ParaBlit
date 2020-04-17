#pragma once
#include <iostream>

#define PB_LOG(message) printf("PARABLIT LOG: "); printf(message); printf("\n")
#define PB_LOG_FORMAT(format, values) printf("PARABLIT LOG: "); printf(format, values); printf("\n")

#define PB_NOT_IMPLEMENTED __debugbreak()