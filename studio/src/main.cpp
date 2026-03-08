#include "app/Application.h"
#include <iostream>

int main() {
	dalia::studio::Application app(1280, 720, "DALIA Studio");
	app.Run();

	return 0;
}
