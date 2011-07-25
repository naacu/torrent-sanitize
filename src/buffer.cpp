
#include "common.h"

#include <limits>

extern "C" {
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <openssl/sha.h>

#ifdef USE_MMAP
# include <sys/mman.h>
#endif
}

namespace torrent {

Buffer::Buffer() :m_data(0), m_len(0), m_pos(0) {
}

bool Buffer::load(const std::string &filename) {
	clear();
	m_filename = filename;

	int fd;

	if (-1 == (fd = ::open(m_filename.c_str(), O_RDONLY))) {
		int e = errno;
		std::cerr << "Cannot open file '" << m_filename << "': " << ::strerror(e) << std::endl;
		return false;
	}
	if (-1 == ::fstat(fd, &filestat)) {
		int e = errno;
		std::cerr << "Cannot stat file '" << m_filename << "': " << ::strerror(e) << std::endl;
		close(fd);
		return false;
	}
	if (!S_ISREG(filestat.st_mode)) {
		std::cerr << "Not a regular file: '" << m_filename << "'" << std::endl;
		close(fd);
		return false;
	}
	if (filestat.st_size > std::numeric_limits<ssize_t>::max()) {
		std::cerr << "File too big: '" << m_filename << "': " << filestat.st_size << " > " << std::numeric_limits<ssize_t>::max() << std::endl;
		close(fd);
		return false;
	}
	m_len = filestat.st_size;

#ifdef USE_MMAP

#ifndef MAP_POPULATE
# define MAP_POPULATE 0
#endif

	m_data = (char*) mmap(NULL, m_len, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
	if (0 == m_data) {
		int e = errno;
		std::cerr << "Cannot mmap file '" << m_filename << "': " << strerror(e) << std::endl;
		close(fd);
		return false;
	}
#else
	ssize_t r;
	m_data = new char[filestat.st_size];
	if (-1 == (r = ::read(fd, m_data, m_len))) {
		int e = errno;
		std::cerr << "Cannot read file '" << m_filename << "': " << strerror(e) << std::endl;
		close(fd);
		return false;
	}
	close(fd);
#endif

	return true;
}

void Buffer::clear() {
#ifdef USE_MMAP
	if (0 != m_data) munmap(m_data, m_len);
#else
	delete m_data;
#endif
	m_data = 0; m_len = m_pos = 0;
}

Buffer::~Buffer() {
	clear();
}

std::string BufferString::toString() {
	if (0 == m_data || 0 == m_len) return std::string();
	return std::string(m_data, m_len);
}

pcrecpp::StringPiece BufferString::toStringPiece() const {
	return pcrecpp::StringPiece(m_data, m_len);
}

std::string BufferString::sha1() {
	unsigned char raw[20];
	::SHA1((const unsigned char*) m_data, m_len, raw);
	std::string hex(40, '.');
	const char hexchar[] = "0123456789ABCDEF";
	for (int i = 0; i < 20; i++) {
		hex[2*i] = hexchar[raw[i] >> 4];
		hex[2*i+1] = hexchar[raw[i] & 0xf];
	}
	return hex;
}

bool BufferString::validUTF8() {
	return torrent::validUTF8(m_data, m_len);
}

bool operator<(const BufferString &a, const BufferString &b) {
	int i = memcmp(a.m_data, b.m_data, std::min(a.m_len, b.m_len));
	if (i < 0) return true;
	if (i == 0 && a.m_len < b.m_len) return true;
	return false;
}
bool operator>(const BufferString &a, const BufferString &b) {
	return b < a;
}
bool operator<=(const BufferString &a, const BufferString &b) {
	int i = memcmp(a.m_data, b.m_data, std::min(a.m_len, b.m_len));
	if (i < 0) return true;
	if (i == 0 && a.m_len <= b.m_len) return true;
	return false;
}
bool operator>=(const BufferString &a, const BufferString &b) {
	return b <= a;
}
bool operator==(const BufferString &a, const BufferString &b) {
	if (a.m_len != b.m_len) return false;
	return (0 == memcmp(a.m_data, b.m_data, a.m_len));
}
bool operator!=(const BufferString &a, const BufferString &b) {
	return !(a == b);
}

std::ostream& operator<<(std::ostream &os, const BufferString &b) {
	os.write(b.data(), b.length());
	return os;
}

const BufferString bs_empty("");
const BufferString bs_announce("announce");
const BufferString bs_announce_list("announce-list");
const BufferString bs_info("info");
const BufferString bs_encoding("encoding");
const BufferString bs_files("files");
const BufferString bs_length("length");
const BufferString bs_name("name");
const BufferString bs_path("path");
const BufferString bs_piece_length("piece length");
const BufferString bs_pieces("pieces");
const BufferString bs_private("private");

}
