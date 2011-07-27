#ifndef __TORRENT_SANITIZE_BUFFER_H
#define __TORRENT_SANITIZE_BUFFER_H

#include <string>
#include <iostream>

#include <pcrecpp.h>

extern "C" {
#include <sys/stat.h>
}

namespace torrent {

class Buffer {
private:
	Buffer(const Buffer &b);
	Buffer& operator=(const Buffer &b);

public:
	Buffer();

	bool load(const std::string &filename);

	template< std::size_t n > bool tryNext( const char (&cstr)[n] ) {
		size_t len = sizeof(cstr)/sizeof(char);
		if (0 == len) return false;
		len--;
		if (len > m_len - m_pos) return false;
		if (0 != memcmp(cstr, m_data + m_pos, len)) return false;
		m_pos += len;
		return true;
	}

	bool isNext(char c) { return !eof() && (c == m_data[m_pos]); }

	bool eof() const { return m_pos >= m_len; }

	void clear();

	size_t pos() const { return m_pos; }
	size_t len() const { return m_len; }
	const char* data() const { return m_data; }
	const char* c_str() const { return m_data; }

	void next() { if (m_pos < m_len) m_pos++; }
	char current() const { return !eof() ? m_data[m_pos] : '\0'; }

	~Buffer();

	struct stat filestat;

private:
	friend class Torrent;
	friend class TorrentBase;

	std::string m_filename;
	char *m_data;
	size_t m_len, m_pos;
};

class BufferString {
public:
	BufferString()
	: m_data(0), m_len(0) { }

	template< std::size_t n > explicit BufferString(const char (&data)[n])
	: m_data(data), m_len(sizeof(data)/sizeof(char) > 0 ? sizeof(data)/sizeof(char) - 1 : 0) {
	}
	BufferString(const char *data, size_t len)
	: m_data(data), m_len(len) {
	}
	explicit BufferString(const std::string &s)
	: m_data(s.c_str()), m_len(s.length()) { }

	std::string toString();

	pcrecpp::StringPiece toStringPiece() const;

	std::string sha1();

	bool validUTF8();
	bool validUTF8Text();

	size_t length() const { return m_len; }
	const char* data() const { return m_data; }
	const char* c_str() const { return m_data; }

	char operator[](size_t ndx) const {
		if (ndx >= m_len) return '\0';
		return m_data[ndx];
	}

private:
	friend class TorrentBase;
	friend class Torrent;
	friend class TorrentSanitize;

	friend bool operator<(const BufferString &a, const BufferString &b);
	friend bool operator<=(const BufferString &a, const BufferString &b);
	friend bool operator==(const BufferString &a, const BufferString &b);

	const char *m_data;
	size_t m_len;
};

bool operator<(const BufferString &a, const BufferString &b);
bool operator>(const BufferString &a, const BufferString &b);
bool operator<=(const BufferString &a, const BufferString &b);
bool operator>=(const BufferString &a, const BufferString &b);
bool operator==(const BufferString &a, const BufferString &b);
bool operator!=(const BufferString &a, const BufferString &b);
std::ostream& operator<<(std::ostream &os, const BufferString &b);

extern const BufferString bs_empty;
extern const BufferString bs_announce;
extern const BufferString bs_announce_list;
extern const BufferString bs_info;
extern const BufferString bs_encoding;
extern const BufferString bs_files;
extern const BufferString bs_length;
extern const BufferString bs_name;
extern const BufferString bs_path;
extern const BufferString bs_piece_length;
extern const BufferString bs_pieces;
extern const BufferString bs_private;

}

#endif
