
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
		if (0 == (s[i] & 0x80)) { continue; }
		else if ((0xC0 == s[i]) || (0xC1 == s[i])) { return false; /* 0xCO / 0xC1 overlong */ }
		else if (0xC0 == (s[i] & 0xE0)) { seqlen = 1; }
		else if (0xE0 == s[i]) {
			if (2 > len - i) return false;
			if (0xA0 != (s[++i] & 0xE0)) return false; /* exclude overlong, require bit 0x20 */
			seqlen = 1; /* checked first, one less */
		}
		else if (0xED == s[i]) {
			if (2 > len - i) return false;
			if (0x80 != (s[++i] & 0xE0)) return false; /* exclude surrogates, forbid bit 0x20 */
			seqlen = 1; /* checked first, one less */
		}
		else if (0xE0 == (s[i] & 0xF0)) { seqlen = 2; }
		else if (0xF0 == s[i]) {
			if (3 > len - i) return false;
			if (0x00 == (s[i+1] & 0x30)) return false; /* exclude overlong: at least one bit from 0x30 */
			seqlen = 3; /* require additional check on first, no skip */
		}
		else if (0xF4 == s[i]) {
			if (3 > len - i) return false;
			if (0x80 != (s[++i] & 0xF0)) return false; /* check range <= U+10FFFF, require 0x8- */
			seqlen = 2; /* checked first, one less */
		}
		else if (0xF4 < s[i]) return false; /* range overflow > U+10FFFF */
		else if (0xF0 == (s[i] & 0xF8)) { seqlen = 3; }
		else return false;
		if (seqlen > len - i) return false;
		for ( ; seqlen > 0; seqlen--) {
			if (0x80 != (s[++i] & 0xC0)) return false;
		}
	}
	return true;
}

bool validUTF8(const std::string &s) {
	return validUTF8(s.c_str(), s.length());
}

/* forbid ctrl characters < 0x20 */
bool validUTF8Text(const char *_s, size_t len) {
	const unsigned char *s = (const unsigned char*) _s;
	for (size_t i = 0; i < len; i++) {
		unsigned int code;
		int seqlen = 0;
		if (0 == (s[i] & 0x80)) { seqlen = 0; code = s[i] & 0x7f; }
		else if (0xC0 == (s[i] & 0xFE)) { return false; /* 0xCO / 0xC1 overlong */ }
		else if (0xC0 == (s[i] & 0xE0)) { seqlen = 1; code = s[i] & 0x1f; }
		else if (0xE0 == (s[i] & 0xF0)) { seqlen = 2; code = s[i] & 0x0f; }
		else if (0xF0 == (s[i] & 0xF8)) { seqlen = 3; code = s[i] & 0x07; }
		else return false;
		if (seqlen > len - i) return false;
		for (int slen = seqlen; slen > 0; slen--) {
			if (0x80 != (s[++i] & 0xC0)) return false;
			code = (code << 6) | (s[i] & 0x3f);
		}

		if ((seqlen == 2) && (code < 0x800)) return false; /* overlong */
		if ((seqlen == 3) && (code < 0x1000)) return false; /* overlong */

		/* allow \n \r \t */
		if (code == 0x0a || code == 0x0d || code == 0x09) continue;
		/* CO, 0x7f and C1: http://en.wikipedia.org/wiki/C0_and_C1_control_codes */
		if (code < 0x20 || (0x7f <= code && code <= 0x9f) ) return false;
		/* exclude surrogates: "high" D800–DBFF and "low" DC00–DFFF */
		if (code >= 0xD800 && code <= 0xDFFF) return false;
		/* "noncharacters": last two code points in plane and U+FDD0..U+FDEF */
		if ((code & 0xFFFE) == 0xFFE || (code >= 0xFDD0 && code <= 0xFDEF)) return false;
		/* planes 15-16: private. above: invalid for now */
		if (code >= 0xf0000) return false;
		/* some special unicode chars */
		switch (code) {
		case 0x200B: // ZERO WIDTH SPACE
		case 0x200C: // ZERO WIDTH NON-JOINER
		case 0x200D: // ZERO WIDTH JOINER
		case 0x200E: // LEFT-TO-RIGHT MARK
		case 0x200F: // RIGHT-TO-LEFT MARK
		case 0x202A: // LEFT-TO-RIGHT EMBEDDING
		case 0x202B: // RIGHT-TO-LEFT EMBEDDING
		case 0x202C: // POP DIRECTIONAL FORMATTING
		case 0x202D: // LEFT-TO-RIGHT OVERRIDE
		case 0x202E: // RIGHT-TO-LEFT OVERRIDE
			return false;
		}
	}
	return true;
}
bool validUTF8Text(const std::string &s) {
	return validUTF8Text(s.c_str(), s.length());
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
