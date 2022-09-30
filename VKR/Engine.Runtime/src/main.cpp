#include "Application.h"

#ifdef PARABLIT_WINDOWS
#include <crtdbg.h>
#endif

int main(int argc, char** argv) 
{
#ifdef PARABLIT_WINDOWS
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Application* game = new Application();
	game->Init(argc, argv);

	game->Run();

	delete game;

	return 0;
}
