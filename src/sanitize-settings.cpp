
#include "sanitize-settings.h"
#include "utils.h"
#include "debug.h"

#include <fstream>
#include <algorithm>

namespace torrent {

TorrentSanitize::TorrentSanitize() : debug(false), show_paths(false), check_info_utf8(false) {
}

bool TorrentSanitize::validMetaKey(BufferString key) const {
	if (0 == key.length()) return false;
	for (size_t i = 0; i < key.length(); i++) {
		if (iscntrl(key[i]) || !isascii(key[i])) return false;
	}
	if (new_meta_entries.end() != new_meta_entries.find(key.toString())) return false;
	return true;
}

bool TorrentSanitize::validMetaTextKey(BufferString key) const {
	return filter_meta_text.matches(key);
}
bool TorrentSanitize::validMetaNumKey(BufferString key) const {
	return filter_meta_num.matches(key);
}
bool TorrentSanitize::validMetaOtherKey(BufferString key) const {
	return filter_meta_other.matches(key);
}

template<class InputIterator, class T>
InputIterator find_last ( InputIterator first, InputIterator last, const T& value ) {
	for (InputIterator it = last; it-- != first; ) if ( *it==value ) return it;
	return last;
}

template<class InputIterator, class Predicate>
bool verify_all ( InputIterator first, InputIterator last, Predicate pred ) {
	for ( ; first!=last ; first++ ) if ( !pred(*first) ) return false;
	return true;
}

/* [ protocol, domain, "" | ":"+port, path ] or [] if no protocol was found */
bool splitUrl(const std::string &url, std::string &protocol, std::string &domain, std::string &port, std::string &path) {
	/* http://stackoverflow.com/questions/2616011/easy-way-to-parse-a-url-in-c-cross-platform */
	const std::string prot_end("://");

	std::string::const_iterator prot_i = std::search(url.begin(), url.end(), prot_end.begin(), prot_end.end());

	if (url.end() == prot_i) return false;

	/* protocol */
	protocol.clear();
	protocol.reserve(distance(url.begin(), prot_i));
	std::transform(url.begin(), prot_i, std::back_inserter(protocol), tolower);

	std::advance(prot_i, prot_end.length());

	string::const_iterator path_i = std::find(prot_i, url.end(), '/');
	string::const_iterator port_i = find_last(prot_i, path_i, ':');

	if (path_i == port_i) {
		/* domain */
		domain.clear();
		domain.reserve(distance(prot_i, path_i));
		std::transform(prot_i, path_i, std::back_inserter(domain), tolower);
		/* empty port */
		port.clear();
	} else {
		/* domain */
		domain.clear();
		domain.reserve(distance(prot_i, port_i));
		std::transform(prot_i, port_i, std::back_inserter(domain), tolower);

		/* port */
		port.clear();
		port.reserve(distance(port_i, path_i));
		std::transform(port_i, path_i, std::back_inserter(port), tolower);
	}

	/* path */
	path.assign(path_i, url.end());

	return true;
}

/* check lowercase domains without trailing dot */
bool validDomain(const std::string &domain, std::string &domtld) {
	const size_t dlen = domain.length();
	const char *dom = domain.c_str();

	if (0 == dlen) return false;
	if ('.' == dom[dlen-1]) return false;

	if ('[' == dom[0]) {
		/* IPv6 address */
		if (dlen < 4) return false; // minimal v6 addr: [::]
		if (']' != dom[dlen-1]) return false;
		for (size_t i = dlen - 2; i > 1; i--) if (':' != dom[i] && !isxdigit(dom[i])) return false;

		domtld = domain;
		return true;
	}

	size_t dot;
	dot = domain.find_last_of('.');
	if (std::string::npos != dot && isdigit(dom[dot+1])) {
		/* IPv4 address */
		long octet;
		char *domnext = NULL;

		for (size_t i = dlen; i-- > 0; ) if ('.' != dom[i] && !isdigit(dom[i])) return false;

		for (size_t i = 0; i < 3; i++) {
			octet = strtol(dom, &domnext, 10);
			if ('.' != *domnext || domnext == dom || domnext - dom > 3 || octet < 0 || octet > 255) return false;
			dom = domnext + 1;
		}
		octet = strtol(dom, &domnext, 10);
		if ('\0' != *domnext || domnext == dom || domnext - dom > 3 || octet < 0 || octet > 255) return false;

		domtld = domain;
		return true;
	}

	/* standard domain name */

	size_t i = 0;
	while (i < dlen) {
		if (!isalnum(dom[i]) && '_' != dom[i]) return false; /* labels must start with letter/digit, accept _ too */
		i++;
		while (i < dlen && dom[i] != '.') {
			if (!isalnum(dom[i]) && '-' != dom[i] && '_' != dom[i]) return false;
			i++;
		}
		i++;
	}

	if (std::string::npos != dot) dot = domain.find_last_of('.', dot-1);
	if (std::string::npos == dot) { dot = 0; } else { dot++; }

	domtld = domain.substr(dot);

	return true;
}

bool TorrentSanitize::basicUrlCleaner(const std::string &url, AnnounceUrl &annurl) const {
	std::string protocol, domain, port, path;
	if (stringHasPrefix(url, "dht://")) return false; /* drop dht urls - fallback if no other url is found */
	if (!splitUrl(url, protocol, domain, port, path)) return false; /* not an url */

	if (!verify_all(protocol.begin(), protocol.end(), isalnum)) return false; /* protocol only a-z0-9 */

	/* remove trailing dot */
	if ('.' == domain[domain.length()-1]) domain.resize(domain.length() - 1);

	long portnum = 0;

	if (port == ":") port.clear();
	if (port.length() > 6) return false; /* max length: ":65536" */
	if (!port.empty()) {
		if (!verify_all(port.begin()+1, port.end(), isdigit)) return false; /* only digits */

		portnum = strtol(port.c_str() + 1, NULL, 10); /* too short for overflow, only digits -> can't fail */
		if (portnum >= 65536 || portnum < 0) portnum = 0; /* drop invalid port numbers */
		port.clear();
	}

	if (protocol == "udp") {
		path.clear(); /* ignore path */
		if (0 == portnum) portnum = 80;  /* enforce port info, default to port 80 */
	} else if (protocol == "http") {
		if (80 == portnum) portnum = 0; /* remove default port */
		if (path.empty()) path = "/";
	} else if (protocol == "https") {
		if (443 == portnum) portnum = 0; /* remove default port */
		if (path.empty()) path = "/";
	}

	/* validate path */
	while (path.length() > 1 && '/' == path[1]) path = path.substr(1); /* remove doubled leading '/' */
	for (size_t i = path.length(); i-- > 0; ) {
		if (!isalnum(path[i])) {
			switch (path[i]) {
			case '%':
			case '&':
			case '+':
			case '-':
			case '.':
			case '/':
			case ':':
			case '=':
			case '?':
			case '_':
				break;
			default:
				return false;
			}
		}
	}

	if (!validDomain(domain, annurl.domain)) return false;

	std::stringstream outurl;
	outurl << protocol << "://" << domain;
	if (portnum != 0) outurl << ":" << portnum;
	outurl << path;

	annurl.url = outurl.str();
// 	torrent::debug() << "domain for '" << url << "' (-> '" << annurl.url << "') is '" << annurl.domain << "'\n";

	return true;
}

std::vector<AnnounceUrl> TorrentSanitize::filterUrl(const std::string &url) const {
	std::vector<AnnounceUrl> urls, queue;
	AnnounceUrl annurl;

	if (!basicUrlCleaner(url, annurl)) return urls;

	if (filter_url_whitelist.matches(annurl.url)) {
		urls.push_back(annurl);
		torrent::debug() << "whitelisted entry: " << annurl.url << "\n";
		return urls;
	} else if (!filter_url_blacklist.matches(annurl.url)) {
		torrent::debug() << "processing entry: " << annurl.url << "\n";
		queue.push_back(annurl);
	} else {
		torrent::debug() << "blaclisted entry: " << annurl.url << "\n";
		return urls;
	}

	std::vector<std::string> rewrites;

	for (std::vector<PCRE_Replace>::const_iterator rfi = filter_url_replace.begin(), rfe = filter_url_replace.end(); !queue.empty() && rfi != rfe; rfi++) {
		for (int qit = 0, qlen = queue.size(); qit < qlen; qit++) {
// 			torrent::debug() << "trying to match '" << queue[qit].url << "' with '" << rfi->pattern() << "'\n";
			if (rfi->replaceFull(queue[qit].url, rewrites)) {
				/* remove qit */
				queue.erase(queue.begin() + qit);
				qit--; qlen--;

				for (int k = 0; k < rewrites.size(); k++) {
					if (!basicUrlCleaner(rewrites[k], annurl)) continue;

					if (filter_url_whitelist.matches(annurl.url)) {
						torrent::debug() << "whitelisted entry: " << annurl.url << "\n";
						urls.push_back(annurl);
					} else if (!filter_url_blacklist.matches(annurl.url)) {
						torrent::debug() << "processing entry: " << annurl.url << "\n";
						queue.push_back(annurl);
					} else {
						torrent::debug() << "whitelisted entry: " << annurl.url << "\n";
						continue;
					}
				}
			}
		}
	}

	for (int k = 0; k < queue.size(); k++) {
		torrent::debug() << "passed entry: " << queue[k].url << "\n";
	}
	urls.insert(urls.end(), queue.begin(), queue.end());

	return urls;
}

static std::vector<std::string> splitLine(const std::string &line) {
	std::vector<std::string> cols;
	int pos = 0;
	while (pos < line.length() && isspace(line[pos])) pos++;

	if (pos >= line.length()) return cols;

	switch (line[pos]) {
	case '+':
	case '*':
		cols.push_back(line.substr(pos, 1));
		pos += 1;
		break;
	case '-':
		if ('-' == line[pos+1]) {
			cols.push_back(line.substr(pos, 2));
			pos += 2;
		} else {
			cols.push_back(line.substr(pos, 1));
			pos += 1;
		}
		break;
	}

	while (pos < line.length()) {
		while (pos < line.length() && isspace(line[pos])) pos++;
		if (pos >= line.length()) return cols;

		if (line[pos] == '#') return cols; /* comment: skip remaining part */

		int pos2 = pos + 1;
		while (pos2 < line.length() && !isspace(line[pos2])) pos2++;

		cols.push_back(line.substr(pos, pos2 - pos));

		pos = pos2 + 1;
	}

	return cols;
}

static std::string patternToRegex(const std::string pattern) {
	/* maybe add glob support in the future */
	return pattern;
}

bool TorrentSanitize::loadUrlConfig(const std::string &urlconfig) {
	std::ifstream urlfile(urlconfig.c_str());
	std::string l;
	std::vector<std::string> cols;

	bool regex_blacklist_domains_empty = true;
	std::stringstream regex_whitelist, regex_blacklist, regex_blacklist_domains;
	regex_whitelist << "^(?:";
	regex_blacklist << "^(?:";

	while (urlfile.good()) {
		std::getline(urlfile, l);
		if (l.empty()) continue;

		cols = splitLine(l);
		if (cols.empty()) continue;

		if (cols[0] == "+") {
			for (int i = 1; i < cols.size(); i++) {
				additional_announce_urls.push_back(cols[i]);
			}
		} else if (cols[0] == "-") {
			for (int i = 1; i < cols.size(); i++) {
				regex_blacklist << "|" << patternToRegex(cols[i]);
			}
		} else if (cols[0] == "--") {
			for (int i = 1; i < cols.size(); i++) {
				if (regex_blacklist_domains_empty) {
					regex_blacklist_domains_empty = false;
				} else {
					regex_blacklist_domains << "|";
				}
				regex_blacklist_domains << patternToRegex(cols[i]);
			}
		} else if (cols[0] == "*") {
			for (int i = 1; i < cols.size(); i++) {
				regex_whitelist << "|" << patternToRegex(cols[i]);
			}
		} else {
			PCRE_Replace rep;
			std::string pattern = cols.front();
			cols.erase(cols.begin());
			if (!rep.load(pattern, cols)) return false;
			filter_url_replace.push_back(rep);
		}
	}

	if (!regex_blacklist_domains_empty) {
		regex_blacklist << "|[^:]+://(?:[0-9a-z_\\-.]*\\.)?(?:" << regex_blacklist_domains.str() << ")(?:[:/].*)?";
	}

	regex_whitelist << ")\\z";
	regex_blacklist << ")\\z";
// 	torrent::debug() << "blacklist regex: " << regex_blacklist.str() << "\n";

	if (!filter_url_whitelist.load(regex_whitelist.str().c_str())) return false;
	if (!filter_url_blacklist.load(regex_blacklist.str().c_str())) return false;

	return true;
}

}
