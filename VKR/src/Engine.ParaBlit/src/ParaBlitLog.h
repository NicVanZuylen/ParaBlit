#pragma once
#include <iostream>

#if PARABLIT_LINUX
#include <csignal>
#endif

#define PB_LOG(message) printf("PARABLIT LOG: "); printf(message); printf("\n")
#define PB_LOG_FORMAT(format, ...) printf("PARABLIT LOG: "); printf(format, __VA_ARGS__); printf("\n")

#if PARABLIT_WINDOWS
#define PB_NOT_IMPLEMENTED printf("PARABLIT: NOT IMPLEMENTED"); __debugbreak()
#elif PARABLIT_LINUX
#define PB_NOT_IMPLEMENTED printf("PARABLIT: NOT IMPLEMENTED"); std::raise(SIGINT);
#endif