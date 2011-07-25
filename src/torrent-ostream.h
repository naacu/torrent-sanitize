#ifndef __TORRENT_SANITIZE_TORRENT_OSTREAM_H
#define __TORRENT_SANITIZE_TORRENT_OSTREAM_H

#include "buffer.h"
#include <iostream>
#include <sstream>

#include <vector>

namespace torrent {

class TorrentOStream {
public:
	explicit TorrentOStream(std::ostream &os) : os(os) { }

	std::ostream &os;

	TorrentOStream& operator<<(const std::string &s) {
		os << s.length() << ":" << s; return *this;
	}
	TorrentOStream& operator<<(const BufferString &s) {
		os << s.length() << ":"; os.write(s.data(), s.length()); return *this;
	}
	template< std::size_t n > TorrentOStream& operator<<(const char (&data)[n]) {
		if (n > 0) { os << (n-1) << ":"; os.write(data, n-1); } else { os << "0:"; }
		return *this;
	}
	TorrentOStream& operator<<(int64_t num) {
		os << "i" << num << "e"; return *this;
	}
	template<typename T> TorrentOStream& operator<<(std::vector< T > list) {
		os << "l";
		for (size_t i = 0; i < list.size(); i++) *this << list[i];
		os << "e";
		return *this;
	}
};

}

#endif
