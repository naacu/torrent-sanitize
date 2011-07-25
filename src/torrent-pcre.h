#ifndef __TORRENT_SANITIZE_TORRENT_PCRE_H
#define __TORRENT_SANITIZE_TORRENT_PCRE_H

#include <pcrecpp.h>

#include <string>
#include <vector>

#include "buffer.h"

namespace torrent {

std::string globToRegex(const std::string &glob);

class PCRE_Replace {
public:
	PCRE_Replace();
	PCRE_Replace(const PCRE_Replace& other);
	PCRE_Replace& operator =(const PCRE_Replace& other);
	~PCRE_Replace();

	void clear();

	bool load(const std::string &pattern, const std::vector<std::string> rewrites);
	bool replaceFull(const std::string &text, std::vector<std::string> &result) const;

	const std::string& pattern() const { return m_pattern; }
	const std::vector<std::string>& rewrites() const { return m_rewrites; }

private:
	pcre* m_re;
	std::string m_pattern;
	std::vector<std::string> m_rewrites;
};

class PCRE {
public:
	PCRE();
	PCRE(const PCRE &other);
	PCRE& operator =(const PCRE &other);
	~PCRE();

	void clear();

	bool load(const char *pattern);

	bool matches(BufferString str) const;
	bool matches(const std::string &str) const;

private:
	pcre *m_re;
};

}

#endif
