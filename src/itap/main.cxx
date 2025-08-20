#include "app.h"

int main()
{
	itap::app app("dgate.sock", "/dev/ttyACM1", "KO6JXH", 'C');

	app.setup();
	app.run();
}
