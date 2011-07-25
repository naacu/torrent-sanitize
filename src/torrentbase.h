#ifndef __TORRENT_SANITIZE_TORRENTBASE_H
#define __TORRENT_SANITIZE_TORRENTBASE_H

#include "sanitize-settings.h"

#include "buffer.h"

#include <string>
#include <vector>
#include <map>
#include <iostream>



#include <sstream>
#include <limits>
#include <algorithm>
#include "utils.h"

namespace torrent {

typedef std::pair<std::string, int64_t> File;


class TorrentBase {
public:
	TorrentBase();

	std::string lasterror();
	std::string filename();

	std::string infohash();

	std::string t_announce;
	std::vector< std::vector< std::string > > t_announce_list;

	void sanitize_announce_urls(const TorrentSanitize &san, const TorrentBase *mergefromother = 0);

protected:
	bool m_check_info_utf8;

	Buffer m_buffer;
	bool loadfile(const std::string &filename);

	BufferString m_raw_info;
	std::string m_info_hash;

	std::string m_lasterror;

	bool parse_announce_list();

	bool errorcontext(const char msg[]);

	bool seterror(const char msg[]);

	bool read_string(BufferString &str);
	bool read_string(std::string &str);
	bool skip_string();

	bool read_utf8(BufferString &str);
	bool read_utf8(std::string &str);
	bool skip_utf8();

	bool read_info_utf8(BufferString &str);
	bool read_info_utf8(std::string &str);
	bool skip_info_utf8();

	bool read_number(int64_t &number);
	bool skip_number();

	bool skip_list();

	bool skip_dict();

	/* skip entries until found search or dict end. skipped does not include the found key */
	bool try_next_dict_entry(BufferString search, BufferString prev, bool &error, BufferString *skipped = 0);

	/* skip entries until dict end */
	bool goto_dict_end(BufferString prev, BufferString *skipped = 0);

	bool skip_value();
};

class AnnounceList {
public:
	AnnounceList(const TorrentSanitize &san) : m_san(san) { }

	void merge(std::string url) {
		std::vector<AnnounceUrl> annurls;
		annurls = m_san.filterUrl(url);

		for (size_t i = 0; i < annurls.size(); i++)
			add(annurls[i].domain, annurls[i].url);
	}

	template<typename T> void merge(std::vector< T > urllist) {
		for (size_t i = 0; i < urllist.size(); i++) merge(urllist[i]);
	}

	void force_merge(const std::string &url) {
		AnnounceUrl annurl;
		if (!m_san.basicUrlCleaner(url, annurl)) return;

		add(annurl.domain, annurl.url);
	}

	template<typename T> void force_merge(std::vector< T > urllist) {
		for (size_t i = 0; i < urllist.size(); i++) merge(urllist[i]);
	}

	void merge(const TorrentBase &t) {
		merge(t.t_announce);
		merge(t.t_announce_list);
	}

	std::vector< std::vector< std::string > > list;

private:
	const TorrentSanitize &m_san;

	typedef std::map< std::string, size_t > GroupIndex;
	GroupIndex m_index;

	void add(const std::string &domain, const std::string &url) {
		GroupIndex::iterator it;
		it = m_index.find(domain);
		if (m_index.end() == it || it->second >= list.size()) {
			m_index.insert(std::make_pair( domain, list.size() ));
			std::vector<std::string> l;
			l.push_back(url);
			list.push_back(l);
		} else {
			std::vector<std::string> &l = list[it->second];
			if (l.end() == std::find(l.begin(), l.end(), url)) l.push_back(url);
		}
	}
};

}

#endif
