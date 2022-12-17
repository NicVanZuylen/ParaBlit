#include "Application.h"

#ifdef PARABLIT_WINDOWS
#include <crtdbg.h>
#endif

#include "CLib/Reflection.h"

int main(int argc, char** argv) 
{
#ifdef PARABLIT_WINDOWS
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Eng::Application* appInstance = new Eng::Application();
	appInstance->Init(argc, argv);
	appInstance->Run();

	delete appInstance;

	return 0;
}
