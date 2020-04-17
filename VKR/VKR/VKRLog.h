#pragma once
#include <iostream>

#define VKR_LOG(message) printf("VKR LOG: "); printf(message); printf("\n")
#define VKR_LOG_FORMAT(format, values) printf("VKR LOG: "); printf(format, values); printf("\n")

#define VKR_NOT_IMPLEMENTED __debugbreak()