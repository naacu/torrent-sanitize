#ifndef __TORRENT_SANITIZE_COMMON_H
#define __TORRENT_SANITIZE_COMMON_H

namespace torrent {
class Buffer;
class BufferString;
class TorrentOStream;
class PCRE;
class TorrentSanitize;
class TorrentBase;
class AnnounceList;
class Torrent;
class TorrentAnnounceInfo;
class TorrentAnnounce;
}

#include "config.h"

#include "utils.h"
#include "debug.h"
#include "buffer.h"
#include "torrent-ostream.h"
#include "torrent-pcre.h"
#include "sanitize-settings.h"
#include "torrentbase.h"
#include "torrent.h"

#endif