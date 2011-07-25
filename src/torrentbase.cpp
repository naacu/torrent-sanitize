#include "torrentbase.h"

namespace torrent {

TorrentBase::TorrentBase()
: m_check_info_utf8(true) { }

std::string TorrentBase::lasterror() { return m_lasterror; }
std::string TorrentBase::filename() { return m_buffer.m_filename; }

std::string TorrentBase::infohash() { if (m_info_hash.empty() && m_raw_info.length() > 0) m_info_hash = m_raw_info.sha1(); return m_info_hash; }

bool TorrentBase::loadfile(const std::string &filename) {
	if (!m_buffer.load(filename)) return seterror("couldn't load file");
	return true;
}

void TorrentBase::sanitize_announce_urls(const TorrentSanitize &san, const TorrentBase *mergefromother) {
	AnnounceList list(san);
	list.force_merge(san.additional_announce_urls);
	list.merge(*this);

	if (0 != mergefromother) list.merge(*mergefromother);

	t_announce_list.clear();

	if (list.list.empty()) {
		std::string hash = infohash();
		for (size_t i = 0; i < hash.length(); i++) hash[i] = ::toupper(hash[i]);
		t_announce = std::string("dht://") + hash;
	} else {
		t_announce = list.list[0][0];
		t_announce_list = list.list;
	}
}

bool TorrentBase::parse_announce_list() {
	std::string s;
	if (m_buffer.eof()) return seterror("expected announce-list, found eof");
	if (!m_buffer.isNext('l')) {
		if (!read_utf8(s)) return errorcontext("expected announce-list, neither list nor string found");
		std::cerr << "broken Announce-list, found single string" << std::endl;
		t_announce_list.push_back(std::vector<std::string>());
		t_announce_list.back().push_back(s);
	} else {
		bool warned1 = false;
		m_buffer.next();
		while (!m_buffer.eof() && !m_buffer.isNext('e')) {
			if (!m_buffer.isNext('l')) {
				if (!read_utf8(s)) return errorcontext("expected announce-list list, neither list nor string found");
				if (!warned1) {
					std::cerr << "broken announce-list, found string in list, should be in separate list" << std::endl;
					warned1 = true;
				}
				if (t_announce_list.empty()) t_announce_list.push_back(std::vector<std::string>());
				t_announce_list.back().push_back(s);
			} else {
				t_announce_list.push_back(std::vector<std::string>());
				m_buffer.next();
				while (!m_buffer.eof() && !m_buffer.isNext('e')) {
					if (!read_utf8(s)) return errorcontext("expected announce-list list entry");
					t_announce_list.back().push_back(s);
				}
				if (!m_buffer.isNext('e')) return seterror("expected announce-list list, found eof");
				m_buffer.next();
			}
		}
		if (!m_buffer.isNext('e')) return seterror("expected announce-list lists, found eof");
		m_buffer.next();
	}
	return true;
}

bool TorrentBase::errorcontext(const char msg[]) {
	std::ostringstream oss;
	oss << "Error: " << msg << "\n    in " << m_lasterror;
	m_lasterror = oss.str();
	return false;
}

bool TorrentBase::seterror(const char msg[]) {
	std::ostringstream oss;
	std::string context = std::string(m_buffer.m_data + m_buffer.pos(), std::min<size_t>(16, m_buffer.m_len - m_buffer.pos()));
	oss << "Error @[" << m_buffer.pos() << "/" << m_buffer.m_len << " '" << context << "'...]: " << msg;
	m_lasterror = oss.str();
	return false;
}

bool TorrentBase::read_string(BufferString &str) {
	char c;
	int64_t pos = m_buffer.pos(), len = m_buffer.m_len;
	int64_t slen = 0;
	str.m_data = 0; str.m_len = 0;

	if (pos >= len) return seterror("expected string length, found eof");

	c = m_buffer.m_data[pos++];
	if (c < '0' || c > '9') return seterror("expected digit for string length, found eof");
	if (pos >= len) return seterror("expected string length, found eof");

	if (c == '0' && m_buffer.m_data[pos] != ':') return seterror("expected string length, found leading zero of non zero length (no following ':')");

	slen = (c - '0');
	for (;;) {
		c = m_buffer.m_data[pos++];
		if (c == ':') break;
		if (c < '0' || c > '9') return seterror("expected digit or colon for string length, found eof");
		if (pos >= len) return seterror("expected string length, found eof");
		if (slen > std::numeric_limits<int32_t>::max()) return seterror("string length overflow");
		slen = 10*slen + (c - '0');
	}

	if (slen > len || slen > len - pos) return seterror("file not large enough for string length"); /* overflow */

	str.m_data = m_buffer.m_data + pos;
	str.m_len = slen;
	m_buffer.m_pos = pos + slen;

	return true;
}
bool TorrentBase::read_string(std::string &str) {
	BufferString tmp;
	if (!read_string(tmp)) return false;
	str = tmp.toString();
	return true;
}

bool TorrentBase::skip_string() {
	BufferString tmp;
	return read_string(tmp);
}

bool TorrentBase::read_utf8(BufferString &str) {
	size_t pos = m_buffer.pos();
	if (!read_string(str)) return false;
	if (!str.validUTF8()) { m_buffer.m_pos = pos; return seterror("string not valid utf-8"); }
	return true;
}
bool TorrentBase::read_utf8(std::string &str) {
	size_t pos = m_buffer.pos();
	BufferString tmp;
	if (!read_string(tmp)) return false;
	if (!tmp.validUTF8()) { m_buffer.m_pos = pos; return seterror("string not valid utf-8"); }
	str = tmp.toString();
	return true;
}
bool TorrentBase::skip_utf8() {
	BufferString tmp;
	return read_utf8(tmp);
}

bool TorrentBase::read_info_utf8(BufferString &str) {
	return m_check_info_utf8 ? read_utf8(str) : read_string(str);
}
bool TorrentBase::read_info_utf8(std::string &str) {
	return m_check_info_utf8 ? read_utf8(str) : read_string(str);
}
bool TorrentBase::skip_info_utf8() {
	return m_check_info_utf8 ? skip_utf8() : skip_string();
}

bool TorrentBase::read_number(int64_t &number) {
	char c;
	int64_t pos = m_buffer.pos(), len = m_buffer.m_len;
	number = 0;

	if (pos >= len) return seterror("expected number, found eof");

	c = m_buffer.m_data[pos++];
	if (c != 'i') return seterror("expected 'i' for number");
	if (pos >= len) return seterror("expected digit for number, found eof");

	c = m_buffer.m_data[pos++];
	if (pos >= len) return seterror("expected digit, '-' or 'e' for number, found eof");

	if (c == '-') {
		c = m_buffer.m_data[pos++];
		if (pos >= len) return seterror("expected digit or 'e' for number, found eof");
		if (c == '0') return seterror("found leading zero in negative number");
		if (c < '1' || c > '9') return seterror("expected leading digit for negative number");

		number = -(c - '0');
		for (;;) {
			c = m_buffer.m_data[pos++];
			if (c == 'e') break;
			if (pos >= len) return seterror("expected digit or 'e' for number, found eof");
			if (c < '0' || c > '9') return seterror("expected digit or 'e' for number");
			if ((std::numeric_limits<int64_t>::min() + (c - '0')) / 10 > number) return seterror("number too small for int64_t"); /* underflow */
			number = 10*number - (c - '0');
		}
	} else {
		if (c == '0' && m_buffer.m_data[pos] != 'e') return seterror("found leading zero for non zero number");
		if (c < '0' || c > '9') return seterror("expected leading digit for number");

		number = (c - '0');
		for (;;) {
			c = m_buffer.m_data[pos++];
			if (c == 'e') break;
			if (pos >= len) return seterror("expected digit or 'e' for number, found eof");
			if (c < '0' || c > '9') return seterror("expected digit or 'e' for number");
			if ((std::numeric_limits<int64_t>::max() - (c - '0')) / 10 < number) return seterror("number too large for int64_t"); /* overflow */
			number = 10*number + (c - '0');
		}
	}

	m_buffer.m_pos = pos;
	return true;
}

bool TorrentBase::skip_number() {
	char c;
	int64_t pos = m_buffer.pos(), len = m_buffer.m_len;

	if (pos >= len) return seterror("expected number, found eof");

	c = m_buffer.m_data[pos++];
	if (c != 'i') return seterror("expected 'i' for number");
	if (pos >= len) return seterror("expected digit for number, found eof");

	c = m_buffer.m_data[pos++];
	if (pos >= len) return seterror("expected digit, '-' or 'e' for number, found eof");

	if (c == '-') {
		c = m_buffer.m_data[pos++];
		if (pos >= len) return seterror("expected digit or 'e' for number, found eof");
		if (c == '0') return seterror("found leading zero in negative number");
		if (c < '1' || c > '9') return seterror("expected leading digit for negative number");
	} else {
		if (c == '0' && m_buffer.m_data[pos] != 'e') return seterror("found leading zero for non zero number");
		if (c < '0' || c > '9') return seterror("expected leading digit for number");
	}

	for (;;) {
		c = m_buffer.m_data[pos++];
		if (c == 'e') break;
		if (pos >= len) return seterror("expected digit or 'e' for number, found eof");
		if (c < '0' || c > '9') return seterror("expected digit or 'e' for number");
	}

	m_buffer.m_pos = pos;
	return true;
}

bool TorrentBase::skip_list() {
	if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected list, found eof");
	if (m_buffer.m_data[m_buffer.pos()] != 'l') return seterror("expected 'l' for list");

	m_buffer.next();
	if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected list entry or 'e', found eof");

	while (m_buffer.m_data[m_buffer.pos()] != 'e') {
		if (!skip_value()) return errorcontext("parsing list entry failed");;
		if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected list entry or 'e', found eof");
	}
	m_buffer.next();

	return true;
}

bool TorrentBase::skip_dict() {
	if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected dict, found eof");
	if (m_buffer.m_data[m_buffer.pos()] != 'd') return seterror("expected 'd' for dict");

	m_buffer.next();
	if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected dict entry or 'e', found eof");

	BufferString last, cur;

	while (m_buffer.m_data[m_buffer.pos()] != 'e') {
		if (!read_utf8(cur)) return errorcontext("parsing dict key failed");
		if (cur <= last) return seterror("(previous) dict entries in wrong order");
		last = cur;
		if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected dict value, found eof");
		if (!skip_value()) return errorcontext("parsing dict value failed");
		if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected dict entry or 'e', found eof");
	}
	m_buffer.next();

	return true;
}

/* skip entries until found search or dict end. skipped does not include the found key */
bool TorrentBase::try_next_dict_entry(BufferString search, BufferString prev, bool &error, BufferString *skipped) {
	size_t start = m_buffer.pos();
	error = true;
	if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected dict, found eof");

	BufferString cur;

	while (m_buffer.m_data[m_buffer.pos()] != 'e') {
		size_t curpos = m_buffer.pos();;
		if (!read_utf8(cur)) return errorcontext("parsing dict key failed");
		if (cur <= prev) return seterror("(previous) dict entries in wrong order");
		if (cur == search) {
			error = false;
			if (0 != skipped) *skipped = BufferString(m_buffer.data() + start, curpos - start);
			return true;
		}
		if (cur > search) {
			m_buffer.m_pos = curpos;
			error = false;
			if (0 != skipped) *skipped = BufferString(m_buffer.data() + start, m_buffer.pos() - start);
			return false;
		}
		prev = cur;
		if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected dict value, found eof");
		if (!skip_value()) return errorcontext("parsing dict value failed");
		if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected dict entry or 'e', found eof");
	}

	/* dict end found, don't skip the 'e' */
	error = false;
	if (0 != skipped) *skipped = BufferString(m_buffer.data() + start, m_buffer.pos() - start);
	return false;
}

/* skip entries until dict end */
bool TorrentBase::goto_dict_end(BufferString prev, BufferString *skipped) {
	size_t start = m_buffer.pos();
	if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected dict entry or 'e', found eof");

	BufferString cur;

	while (m_buffer.m_data[m_buffer.pos()] != 'e') {
		if (!read_utf8(cur)) return errorcontext("parsing dict key failed");
		if (cur <= prev) {
			std::cerr << "dict entries wrong order: '" << cur << "' <= '" << prev << "'\n";
		}
		if (cur <= prev) return seterror("(previous) dict entries in wrong order");
		prev = cur;
		if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected dict value, found eof");
		if (!skip_value()) return errorcontext("parsing dict value failed");
		if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected dict entry or 'e', found eof");
	}
	m_buffer.next();

	if (0 != skipped) *skipped = BufferString(m_buffer.data() + start, m_buffer.pos() - start);

	return true;
}

bool TorrentBase::skip_value() {
	char c;
	if (m_buffer.pos() >= m_buffer.m_len) return seterror("expected value, found eof");
	c = m_buffer.m_data[m_buffer.pos()];
	if (c >= '0' && c <= '9') return skip_string();
	if (c == 'i') return skip_number();
	if (c == 'l') return skip_list();
	if (c == 'd') return skip_dict();
	return seterror("expected value");
}

}
