#ifndef __TORRENT_SANITIZE_UTILS_H
#define __TORRENT_SANITIZE_UTILS_H

extern "C" {
#include <sys/types.h>
#include <string.h>
}

#include <string>

namespace torrent {

bool validUTF8(const char *_s, size_t len);
bool validUTF8(const std::string &s);

template<size_t N> bool stringHasPrefix(const std::string &s, const char (&prefix)[N]) {
	return s.length() >= (N-1) && 0 == memcmp(s.c_str(), prefix, N-1);
}

class Writable {
public:
	virtual void write(std::ostream &os) const = 0;

	bool writeAtomicFile(const std::string &filename) const;
};

template<typename T>
class OStreamObject : public Writable {
private:
	const T &obj;
public:
	OStreamObject(const T &obj) : obj(obj) { }
	virtual void write(std::ostream &os) const {
		os << obj;
	}
};

template<typename T> bool writeAtomicFile(const std::string &filename, const T &t) {
	OStreamObject<T> o(t);
	o.writeAtomicFile(filename);
}

}

#endif
