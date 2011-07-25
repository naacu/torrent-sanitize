#ifndef __TORRENT_SANITIZE_SANITIZE_SETTINGS_H
#define __TORRENT_SANITIZE_SANITIZE_SETTINGS_H

#include "buffer.h"
#include "torrent-ostream.h"
#include "torrent-pcre.h"

#include <vector>
#include <map>

#include <pcrecpp.h>

namespace torrent {

typedef std::map<std::string, std::string> TorrentRawParts;

class AnnounceUrl {
public:
	std::string url;
	std::string domain; /* domain + tld, like "example.com" for "udp://tracker.example.com/". full address for ipv4/ipv6 */
};

class TorrentSanitize {
public:
	TorrentSanitize();
	bool validMetaKey(BufferString key) const;

	bool validMetaTextKey(BufferString key) const;
	bool validMetaNumKey(BufferString key) const;
	bool validMetaOtherKey(BufferString key) const;

	bool basicUrlCleaner(const std::string &url, AnnounceUrl &annurl) const;
	std::vector<AnnounceUrl> filterUrl(const std::string &url) const;

	bool loadUrlConfig(const std::string &configpath);

	template<typename Value> void add_new_meta_entry(const std::string &key, const Value &value) {
		std::ostringstream raw;
		TorrentOStream(raw) << key << value;
		new_meta_entries.insert(std::make_pair(key, raw.str()));
	}

	void add_new_raw_meta_entry(const std::string &key, const std::string &value) {
		std::ostringstream raw;
		TorrentOStream(raw) << key;
		raw << value;
		new_meta_entries.insert(std::make_pair(key, raw.str()));
	}

	TorrentRawParts new_meta_entries;

	bool debug;
	bool show_paths; /* build paths for file entries, joined with '/', show them later */
	bool check_info_utf8; /* as we can't modify the info part, optionally disable struct utf-8 checks */

	PCRE filter_meta_text, filter_meta_num, filter_meta_other;

	PCRE filter_url_whitelist, filter_url_blacklist;
	std::vector<PCRE_Replace> filter_url_replace;
	std::vector< std::string > additional_announce_urls;

private:
	std::vector< std::string > m_alloced_strings;
};

}

#endif
