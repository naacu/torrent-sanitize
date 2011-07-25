#ifndef __TORRENT_SANITIZE_DEBUG_H
#define __TORRENT_SANITIZE_DEBUG_H

#include <ostream>

namespace torrent {

void setDebugActive(bool active);
bool getDebugActive();

std::ostream& debug();

}

#endif
