#include "Application.h"

#ifdef VKR_WINDOWS
#include <crtdbg.h>
#endif

int main() 
{
#ifdef VKR_WINDOWS
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Application* game = new Application();
	game->Init();

	game->Run();

	delete game;

	return 0;
}
