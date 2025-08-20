#include "app.h"

int main() {
	dlink::app app("dgate.sock", "KO6JXH", "hosts", {'C'});

	app.setup();
	app.run();
}
