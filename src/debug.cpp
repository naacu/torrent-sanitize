
#include "debug.h"

#include <iostream>
#include <sstream>

namespace torrent {

static bool debugActive = false;

static std::stringstream nullOut;

void setDebugActive(bool active) {
	debugActive = active;
}

bool getDebugActive() {
	return debugActive;
}

std::ostream& debug() {
	if (debugActive) return std::cerr;
	nullOut.str(std::string());
	return nullOut;
}

}
