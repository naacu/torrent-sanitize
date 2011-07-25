
#include "torrent-pcre.h"

#include <fstream>
#include <sstream>
#include <cctype>

namespace torrent {

std::string globToRegex(const std::string &glob) {
	static const char hex[] = "0123456789abcdef";
	std::stringstream out;
	for (int i = 0, l = glob.length(); i < l; i++) {
		char c = glob[i];
		switch (c) {
		case '*':
			out << ".*";
			break;
		case '?':
			out << '.';
			break;
		case '(':
		case ')':
		case '-':
		case ':':
		case '/':
			out << c;
			break;
		default:
			if (isalnum(c)) {
				out << c;
			} else {
				char buf[5] = "\\x00";
				buf[2] = hex[(c >> 4) & 0xf];
				buf[3] = hex[c & 0xf];
				out << buf;
			}
			break;
		}
	}
	return out.str();
}

PCRE_Replace::PCRE_Replace() : m_re(0) { }
PCRE_Replace::~PCRE_Replace() {
	clear();
}
PCRE_Replace::PCRE_Replace(const PCRE_Replace& other) : m_re(0), m_pattern(other.m_pattern), m_rewrites(other.m_rewrites) {
	if (0 != other.m_re) {
		pcre_refcount(other.m_re, 1);
		m_re = other.m_re;
	}
}
PCRE_Replace& PCRE_Replace::operator =(const PCRE_Replace& other) {
	if (this == &other) return *this;
	clear();

	if (0 != other.m_re) {
		pcre_refcount(other.m_re, 1);
		m_re = other.m_re;
	}
	m_pattern = other.m_pattern;
	m_rewrites = other.m_rewrites;
	return *this;
}

void PCRE_Replace::clear() {
	if (0 != m_re && 0 == pcre_refcount(m_re, -1)) {
		pcre_free(m_re);
	}
	m_re = 0;
	m_pattern.clear();
	m_rewrites.clear();
}

bool PCRE_Replace::load(const std::string &pattern, const std::vector<std::string> rewrites) {
	const char* compile_error;
	int eoffset;

	clear();

	std::string fullpattern = "(?:" + pattern + ")\\z";
	m_re = pcre_compile(fullpattern.c_str(), PCRE_ANCHORED, &compile_error, &eoffset, NULL);
	if (0 == m_re) {
		std::cerr << "Error in pcre pattern: " << compile_error << " @ position " << (eoffset - 3) << " in '" << pattern << "'\n";
		return false;
	}
	pcre_refcount(m_re, 1);
	m_pattern = pattern;

	m_rewrites = rewrites;
	return true;
}

static std::string replace(const std::string &text, const std::string &rewrite, int *ovector, int n) {
	std::string result;
	for (const char *s = rewrite.c_str(), *e = s + rewrite.length(); s < e; s++) {
		if ('\\' == *s) {
			s++;
			if (s >= e) return result;
			if (isdigit(*s)) {
				int c = *s - '0';
				if (c <= n && ovector[2*c] >= 0) {
					result.append(text.c_str() + ovector[2*c], ovector[2*c+1] - ovector[2*c]);
				}
			} else {
				result += *s;
			}
		} else {
			result += *s;
		}
	}
	return result;
}

bool PCRE_Replace::replaceFull(const std::string &text, std::vector<std::string> &result) const {
	if (0 == m_re) return false;
	int ovector[30];
	int rc = pcre_exec(m_re, NULL, text.c_str(), text.length(), 0, 0, ovector, 30);
	if (rc < 0) {
		if (PCRE_ERROR_NOMATCH == rc) return false;
		std::cerr << "Error while executing pcre pattern, code: " << rc << "\n";
		return false;
	}
	if (0 == rc) rc = 10;
	result.clear();
	for (size_t i = 0; i < m_rewrites.size(); i++) {
		result.push_back(replace(text, m_rewrites[i], ovector, rc));
	}
	return true;
}

PCRE::PCRE() : m_re(0) { }
PCRE::~PCRE() { clear(); }
PCRE::PCRE(const PCRE &other) : m_re(0) {
	if (0 != other.m_re) {
		pcre_refcount(other.m_re, 1);
		m_re = other.m_re;
	}
}
PCRE& PCRE::operator =(const PCRE &other) {
	if (this == &other) return *this;
	clear();

	if (0 != other.m_re) {
		pcre_refcount(other.m_re, 1);
		m_re = other.m_re;
	}
	return *this;
}

void PCRE::clear() {
	if (0 != m_re && 0 == pcre_refcount(m_re, -1)) {
		pcre_free(m_re);
	}
	m_re = 0;
}

bool PCRE::load(const std::string &pattern) {
	const char* compile_error;
	int eoffset, errorcodeptr;

	clear();

	m_re = pcre_compile2(("^(?:" + pattern + ")\\z").c_str(), 0, &errorcodeptr, &compile_error, &eoffset, NULL);
	if (0 == m_re) {
		std::cerr << "Couldn't parse pcre pattern: " << compile_error << "\n";
		return false;
	}
	pcre_refcount(m_re, 1);
	return true;
}

static bool pcreMatches(pcre *re, const char *str, int len) {
	int ovector[30];
	int rc = pcre_exec(re, NULL, str, len, 0, 0, ovector, 30);
	if (rc < 0) {
		if (PCRE_ERROR_NOMATCH == rc) return false;
		std::cerr << "Error while executing pcre pattern, code: " << rc << "\n";
		return false;
	}
	return true;
}

bool PCRE::matches(BufferString str) const {
	if (0 == m_re) return false;
	return pcreMatches(m_re, str.c_str(), str.length());
}

bool PCRE::matches(const std::string &str) const {
	if (0 == m_re) return false;
	return pcreMatches(m_re, str.c_str(), str.length());
}

}
