
#include "torrent.h"

namespace torrent {

Torrent::Torrent(const TorrentSanitize &san) : m_san(san) {
	m_check_info_utf8 = m_san.check_info_utf8;
}

bool Torrent::load(const std::string &filename) {
	if (!loadfile(filename)) return false;

	if (!m_buffer.tryNext("d8:announce")) return seterror("doesn't look like a valid torrent, expected 'd8:announce'");
	if (!read_utf8(t_announce)) return errorcontext("parsing torrent announce failed");

	BufferString prevkey(m_buffer.m_data+3, 8), curkey;
	while (!m_buffer.eof() && !m_buffer.isNext('e')) {
		size_t curpos = m_buffer.pos();
		if (!read_utf8(curkey)) return errorcontext("parsing dict key in torrent failed");
		if (curkey <= prevkey) return seterror("wrong key order in torrent dict");
		prevkey = curkey;

		if (curkey == BufferString("announce-list")) {
			if (!parse_announce_list()) return errorcontext("parsing torrent announce-list failed");
		} else if (curkey == BufferString("info")) {
			curpos = m_buffer.pos();
			if (!parse_info()) return errorcontext("parsing torrent info failed");
			m_raw_info = BufferString(m_buffer.m_data + curpos, m_buffer.pos() - curpos);
		} else if (curkey == BufferString("encoding")) {
			if (!read_utf8(t_encoding)) return errorcontext("parsing torrent encoding failed");
		} else {
			std::string content;
			int64_t number;

			if (!m_san.validMetaKey(curkey)) {
				if (!skip_value()) return errorcontext("parsing torrent meta entry failed");
				if (m_san.debug) std::cerr << "Skipped entry '" << curkey.toString() << "'\n";
			} else if (m_san.validMetaTextKey(curkey) && read_utf8(content)) {
				if (m_san.debug) std::cerr << "Additional text entry '" << curkey.toString() << "': '" << content << "'\n";
				m_raw_parts.insert(std::make_pair(curkey.toString(), BufferString(m_buffer.m_data + curpos, m_buffer.pos() - curpos).toString()));
			} else if (m_san.validMetaNumKey(curkey) && read_number(number)) {
				if (m_san.debug) std::cerr << "Additional numeric entry '" << curkey.toString() << "': " << number << "\n";
				m_raw_parts.insert(std::make_pair(curkey.toString(), BufferString(m_buffer.m_data + curpos, m_buffer.pos() - curpos).toString()));
			} else if (m_san.validMetaOtherKey(curkey)) {
				if (!skip_value()) return errorcontext("parsing torrent meta entry failed");
				if (m_san.debug) std::cerr << "Additional raw entry '" << curkey.toString() << "'\n";
				m_raw_parts.insert(std::make_pair(curkey.toString(), BufferString(m_buffer.m_data + curpos, m_buffer.pos() - curpos).toString()));
			} else if (skip_value()) {
				if (m_san.debug) std::cerr << "Skipped entry '" << curkey.toString() << "'\n";
			} else {
				return errorcontext("parsing torrent meta entry failed");
			}
		}
	}

	m_raw_parts.insert(m_san.new_meta_entries.begin(), m_san.new_meta_entries.end());

	if (m_buffer.m_len-1 != m_buffer.pos()) {
		if (m_buffer.eof()) return seterror("unexpected end of file while parsing torrent");
		return seterror("file contains garbage after torrent");
	}

	return true;
}

void Torrent::write(std::ostream &os) const {
	TorrentOStream tos(os);

	os << "d8:announce";
	tos << t_announce;

	writerawkeys(os, bs_announce, bs_announce_list);

	if (!t_announce_list.empty()) {
		os << "13:announce-list";
		tos << t_announce_list;
	} else {
		writerawkey(os, bs_announce_list);
	}

	if (!t_encoding.empty()) {
		writerawkeys(os, bs_announce_list, bs_encoding);
		os << "8:encoding";
		tos << bs_encoding;
		writerawkeys(os, bs_encoding, bs_info);
	} else {
		writerawkeys(os, bs_announce_list, bs_info);
	}

	os << "4:info" << m_raw_info;

	writerawkeys(os, bs_info, BufferString());

	os << "e";
}

void Torrent::print_details() {
	std::cout << "Announce-url: " << t_announce << std::endl;
	std::cout << "Announce-list:\n";
	for (size_t i = 0; i < t_announce_list.size(); i++) {
		std::cout << " - [ ";
		for (size_t j = 0; j < t_announce_list[i].size(); j++) {
			if (j > 0) std::cout << ", ";
			std::cout << t_announce_list[i][j];
		}
		std::cout << " ]\n";
	}
	if (!t_encoding.empty()) std::cout << "Encoding: " << t_encoding << std::endl;

	std::cout << "Info name: " << t_info_name << std::endl;
	std::cout << "Info files: " << t_info_files.size() << std::endl;
	if (m_san.show_paths) {
		for (size_t i = 0; i < t_info_files.size(); i++) {
			std::cout << " - " << t_info_files[i].second << " bytes: '" << t_info_files[i].first << "'" << std::endl;
		}
	}
	std::cout << "Info complete length: " << t_info_complete_length << " bytes" << std::endl;
	std::cout << "Info pieces length: " << t_info_piece_length << " bytes" << std::endl;
	std::cout << "Info-Hash: " << infohash() << std::endl;
}

bool Torrent::parse_info() {
	bool err;
	BufferString prev;
	std::string tmps;
	/* ('length':number | 'files':...) ['name': string] 'piece length':number 'pieces':raw[%20] */
	if (m_buffer.eof()) return seterror("expected torrent info, found eof");
	if (!m_buffer.isNext('d')) return seterror("expected 'd' for dict");
	m_buffer.next();

	if (try_next_dict_entry(bs_files, bs_empty, err)) {
		t_info_complete_length = 0;
		if (!parse_info_files()) return errorcontext("couldn't parse files in torrent info");
		prev = bs_files;
	} else if (err) {
		return errorcontext("couldn't find torrent info key");
	} else if (try_next_dict_entry(bs_length, bs_files, err)) {
		if (!read_number(t_info_complete_length)) return errorcontext("couldn't parse length in torrent info");
		prev = bs_length;
	} else if (err) {
		return errorcontext("couldn't find torrent info key");
	} else return seterror("expected files or length in torrent info");

	if (try_next_dict_entry(bs_name, prev, err)) {
		if (!read_info_utf8(t_info_name)) return errorcontext("couldn't parse name in torrent info");
	} else if (err) {
		return errorcontext("couldn't find torrent info key");
	} else {
		std::cerr << "torrent info has no name entry\n";
	}

	if (try_next_dict_entry(bs_piece_length, bs_name, err)) {
		if (!read_number(t_info_piece_length)) return errorcontext("couldn't parse piece length in torrent info");
	} else if (err) {
		return errorcontext("couldn't find torrent info key");
	} else {
		return seterror("expected piece length in torrent info");
	}

	if (try_next_dict_entry(bs_pieces, bs_piece_length, err)) {
		BufferString pieces;
		if (!read_string(pieces)) return errorcontext("couldn't parse pieces in torrent info");
		if (0 != pieces.m_len % 20) return seterror("pieces in torrent info has wrong length (not a multiple of 20)");
	} else if (err) {
		return errorcontext("couldn't find torrent info key");
	} else {
		return seterror("expected piece length in torrent info");
	}

	if (try_next_dict_entry(bs_private, bs_pieces, err)) {
		int64_t private_flag;
		if (!read_number(private_flag)) return errorcontext("couldn't parse torrent info private flag");
		if (0 != private_flag && 1 != private_flag) return seterror("torrent info private flag is neither 0 nor 1");
	} else if (err) {
		return errorcontext("couldn't find torrent info key");
	}

	if (!goto_dict_end(bs_private)) return errorcontext("couldn't find torrent info key");

	return true;
}

bool Torrent::parse_info_files() {
	if (!m_buffer.isNext('l')) return seterror("expected 'l' for list");
	m_buffer.next();

	while (!m_buffer.eof() && !m_buffer.isNext('e')) {
		if (!parse_info_file()) return errorcontext("couldn't parse files entry");
	}
	if (!m_buffer.isNext('e')) return seterror("expected info files entry, found eof");
	m_buffer.next();

	if (0 == t_info_files.size()) return seterror("empty files list");

	return true;
}

bool Torrent::parse_info_file() {
	if (!m_buffer.isNext('d')) return seterror("expected 'd' for dict");
	m_buffer.next();

	bool err;

	int64_t length;
	std::string path;

	if (try_next_dict_entry(bs_length, bs_empty, err)) {
		if (!read_number(length)) return seterror("couldn't parse length");
		if (length < 0) return seterror("negative file length");
		t_info_complete_length += length;
	} else if (err) {
		return errorcontext("couldn't find info file entry key");
	} else {
		return seterror("expected length in file entry");
	}

	if (try_next_dict_entry(bs_path, bs_length, err)) {
		if (!parse_info_file_path(path)) return errorcontext("couldn't parse path in file entry");
	} else if (err) {
		return errorcontext("couldn't find info file entry key");
	} else {
		return seterror("expected path in file entry");
	}

	t_info_files.push_back(File(path, length));

	if (!goto_dict_end(bs_path)) return errorcontext("error while searching end of info files entry");

	return true;
}

bool Torrent::parse_info_file_path(std::string &path) {
	int components = 0;
	std::ostringstream buildpath;
	std::string part;
	if (!m_buffer.isNext('l')) return seterror("expected 'l' for list");
	m_buffer.next();

	while (!m_buffer.eof() && !m_buffer.isNext('e')) {
		if (m_san.show_paths) {
			if (!read_info_utf8(part)) return errorcontext("couldn't parse path component");
			if (components > 0) buildpath << '/';
			buildpath << part;
		} else {
			if (!skip_info_utf8()) return errorcontext("couldn't parse path component");
		}
		components++;
	}
	if (!m_buffer.isNext('e')) return seterror("expected path component, found eof");
	m_buffer.next();

	path = buildpath.str();

	return true;
}

void Torrent::writerawkeys(std::ostream &os, BufferString prev, BufferString next) const {
	std::string ns = next.toString();
	TorrentRawParts::const_iterator it = m_raw_parts.upper_bound(prev.toString());
	while (it != m_raw_parts.end() && (0 == next.length() || it->first < ns)) {
		os << it->second;
		it++;
	}
}

void Torrent::writerawkey(std::ostream &os, BufferString key) const {
	TorrentRawParts::const_iterator it = m_raw_parts.find(key.toString());
	if (it != m_raw_parts.end()) os << it->second;
}

std::ostream& operator<<(std::ostream &os, const Torrent &t) {
	t.write(os);
	return os;
}

TorrentAnnounceInfo::TorrentAnnounceInfo() {
}

bool TorrentAnnounceInfo::load(const std::string &filename) {
	bool err;

	if (!loadfile(filename)) return false;

	if (!m_buffer.tryNext("d8:announce")) return seterror("doesn't look like a valid torrent, expected 'd8:announce'");
	if (!read_utf8(t_announce)) return errorcontext("parsing torrent announce failed");

	if (try_next_dict_entry(bs_announce_list, bs_announce, err, &m_post_announce)) {
		if (!parse_announce_list()) return errorcontext("parsing torrent announce-list failed");
	} else if (err) {
		return errorcontext("parsing dict key in torrent failed");
	}

	if (try_next_dict_entry(bs_info, bs_announce_list, err, &m_post_announce_list)) {
		size_t curpos = m_buffer.pos();
		if (!skip_value()) return errorcontext("parsing torrent info failed");
		m_raw_info = BufferString(m_buffer.data() + curpos, m_buffer.pos() - curpos);
	} else if (err) {
		return errorcontext("parsing dict key in torrent failed");
	} else {
		return seterror("no info key in torrent");
	}

	if (!goto_dict_end(bs_info, &m_post_info)) return seterror("expected end of info files entry");

	return true;
}

void TorrentAnnounceInfo::write(std::ostream &os) const {
	TorrentOStream tos(os);

	os << "d8:announce";
	tos << t_announce;

	os << m_post_announce;

	if (!t_announce_list.empty()) {
		os << "13:announce-list";
		tos << t_announce_list;
	}

	os << m_post_announce_list << "4:info" << m_raw_info << m_post_info;
}

void TorrentAnnounceInfo::print_details() {
	std::cout << "Announce-url: " << t_announce << std::endl;
	std::cout << "Announce-list:\n";
	for (size_t i = 0; i < t_announce_list.size(); i++) {
		if (i > 0) std::cout << " -- \n";
		for (size_t j = 0; j < t_announce_list[i].size(); j++) {
			std::cout << " - " << t_announce_list[i][j] << "\n";
		}
	}

	std::cout << "Info-Hash: " << infohash() << std::endl;
}

std::ostream& operator<<(std::ostream &os, const TorrentAnnounceInfo &t) {
	t.write(os);
	return os;
}

TorrentAnnounce::TorrentAnnounce() {
}

bool TorrentAnnounce::load(const std::string &filename) {
	bool err;

	if (!loadfile(filename)) return false;

	if (!m_buffer.tryNext("d8:announce")) return seterror("doesn't look like a valid torrent, expected 'd8:announce'");
	if (!read_utf8(t_announce)) return errorcontext("parsing torrent announce failed");

	if (try_next_dict_entry(bs_announce_list, bs_announce, err, &m_post_announce)) {
		if (!parse_announce_list()) return errorcontext("parsing torrent announce-list failed");
	} else if (err) {
		return errorcontext("parsing dict key in torrent failed");
	}

	/* just remember which part we skipped - this doesn't actually read anything */
	m_post_announce_list = BufferString(m_buffer.data() + m_buffer.pos(), m_buffer.len() - m_buffer.pos());

	return true;
}

void TorrentAnnounce::write(std::ostream &os) const {
	TorrentOStream tos(os);

	os << "d8:announce";
	tos << t_announce;

	os << m_post_announce;

	if (!t_announce_list.empty()) {
		os << "13:announce-list";
		tos << t_announce_list;
	}

	os << m_post_announce_list;
}

void TorrentAnnounce::print_details() {
	std::cout << "Announce-url: " << t_announce << std::endl;
	std::cout << "Announce-list:\n";
	for (size_t i = 0; i < t_announce_list.size(); i++) {
		if (i > 0) std::cout << " -- \n";
		for (size_t j = 0; j < t_announce_list[i].size(); j++) {
			std::cout << " - " << t_announce_list[i][j] << "\n";
		}
	}
}

std::ostream& operator<<(std::ostream &os, const TorrentAnnounce &t) {
	t.write(os);
	return os;
}

}
