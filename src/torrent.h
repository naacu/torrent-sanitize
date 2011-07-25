#ifndef __TORRENT_SANITIZE_TORRENT_H
#define __TORRENT_SANITIZE_TORRENT_H

#include "torrentbase.h"

namespace torrent {

/* all three classes parse torrent files, and all three can write it back again */
/* they differ in which parts they actually try to understand or just verify */
/* they all parse the announce and announce-list urls */

/* parses the complete torrent and makes sure it is valid bencoding;
   makes sure the important fields are valid
   additionaly may perform other checks; utf-8, additional meta-fields, additional info-fields, ... */
class Torrent : public TorrentBase {
public:
	Torrent(const TorrentSanitize &san);

	bool load(const std::string &filename);

	void write(std::ostream &os) const;
	void print_details();

private:
	bool parse_info();

	bool parse_info_files();
	bool parse_info_file();
	bool parse_info_file_path(std::string &path);

	void writerawkeys(std::ostream &os, BufferString prev, BufferString next) const;
	void writerawkey(std::ostream &os, BufferString key) const;

	const TorrentSanitize &m_san;

	std::string t_encoding;

	std::string t_info_name;
	int64_t t_info_piece_length;
	int64_t t_info_complete_length;
	std::vector<File> t_info_files;

	TorrentRawParts m_raw_parts;
};

std::ostream& operator<<(std::ostream &os, const Torrent &t);

/* only load announce urls and info hash; validates the bencoding of the complete torrent */
/* does *not* check whether the info content is good */
class TorrentAnnounceInfo : public TorrentBase {
public:
	TorrentAnnounceInfo();

	bool load(const std::string &filename);

	void write(std::ostream &os) const;

	void print_details();

private:
	BufferString m_post_announce, m_post_announce_list, m_post_info;
};

std::ostream& operator<<(std::ostream &os, const TorrentAnnounceInfo &t);

/* only read announce urls, don't verify any data after announce-list */
/* very fast as the load method doesn't read the whole torrent from disk */
/* the write method will of course need to read the remaining parts from disk */
class TorrentAnnounce : public TorrentBase {
public:
	TorrentAnnounce();

	bool load(const std::string &filename);

	void write(std::ostream &os) const;

	void print_details();

private:
	BufferString m_post_announce, m_post_announce_list;
};

std::ostream& operator<<(std::ostream &os, const TorrentAnnounce &t);

}

#endif
