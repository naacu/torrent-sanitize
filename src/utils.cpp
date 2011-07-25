
#include "utils.h"

#include <iostream>
#include <fstream>

extern "C" {
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
}

namespace torrent {

	bool validUTF8(const char *_s, size_t len) {
	const unsigned char *s = (const unsigned char*) _s;
	for (size_t i = 0; i < len; i++) {
		size_t seqlen = 0;
		if (0 == (s[i] & 0x80)) { seqlen = 0; }
		else if (0xC0 == (s[i] & 0xE0)) { seqlen = 1; }
		else if (0xE0 == (s[i] & 0xF0)) { seqlen = 2; }
		else if (0xF0 == (s[i] & 0xF8)) { seqlen = 3; }
		else if (0xF8 == (s[i] & 0xFC)) { seqlen = 4; }
		else if (0xFC == (s[i] & 0xFE)) { seqlen = 5; }
		else return false;
		if (seqlen > len - i) return false;
		for ( ; seqlen > 0; seqlen--, i++) {
			if (0x80 != (s[i+1] && 0xC0)) return false;
		}
	}
	return true;
}

bool validUTF8(const std::string &s) {
	return validUTF8(s.c_str(), s.length());
}

bool Writable::writeAtomicFile(const std::string &filename) const {
	char *tmpfname = new char[filename.length() + 8];
	::memcpy(tmpfname, filename.c_str(), filename.length());
	::memcpy(tmpfname + filename.length(), ".XXXXXX", 8);
	int fd = ::mkstemp(tmpfname);
	if (-1 == fd) {
		int e = errno;
		std::cerr << "Cannot create secure tempfile '" << tmpfname << "': " << ::strerror(e) << std::endl;
		return false;
	}

	{
		std::ofstream os(tmpfname, std::ios_base::binary | std::ios_base::trunc | std::ios_base::out);
		if (os.fail()) {
			int e = errno;
			std::cerr << "Cannot open file '" << tmpfname << "': " << ::strerror(e) << std::endl;
			::close(fd);
			return false;
		}
		write(os);
		os.close();
	}
	::fchmod(fd, 0644);
	::close(fd);
	if (-1 == ::rename(tmpfname, filename.c_str())) {
		int e = errno;
		std::cerr << "Cannot rename tempfile '" << tmpfname << "' to '" << filename << "': " << ::strerror(e) << std::endl;
		return false;
	}

	return true;
}

}
