
/*
   goal: fix torrents (if possible): cleanup announce urls, set custom comments, filter meta fields ...

   compile:
     LDFLAGS='-lssl -lpcrecpp' CPPFLAGS='-O2 -Wall' make torrent-sanitize
       needs openssl-dev for sha1 and libpcre++-dev

 */

#include "common.h"

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include <iostream>
#include <sstream>
#include <fstream>

#include <limits>

#include <pcrecpp.h>

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#ifdef USE_MMAP
# include <sys/mman.h>
#endif

#include <getopt.h>
}


using namespace torrent;

void syntax() {
	std::cerr << "Syntax: \n"
		"\tprint info (default command):\n"
		"\t\ttorrent-sanitize -i  [-f] [-d] [-v] file.torrent\n"
		"\n"
		"\t\t  Hint: regular matches always do full matching like ^...$\n"
		"\t\t--meta-filter-text regex      allow matching keys for text entries (example: 'comment|created by', default: '.*')\n"
		"\t\t--meta-filter-number regex    allow matching keys for numeric entries (example: 'creation date', default: '.*')\n"
		"\t\t--meta-filter-any regex       allow matching keys for any entries (default: '', none)\n"
		"\t\t--meta-add-string key=value   add meta string entry\n"
		"\t\t--meta-add-raw key=value      add meta raw (bencoded) entry\n"
		"\n"
		"\t\t--url-filter configfile       use configfile for announce url filtering\n"
		"\n"
		"\tsanitize:\n"
		"\t\ttorrent-sanitize -s [-h] [-d] [-v] infile.torrent [outfile.torrent]\n"
		"\n"
		"\tcalculate info hash / show announce urls:\n"
		"\t\ttorrent-sanitize [-h] [-u] file.torrent\n"
		"\n"
		"\t\t -h: show info hash\n"
		"\t\t -f: show files\n"
		"\t\t -d: debug mode\n"
		"\t\t -v: verify strict: utf-8 checks (more may come)\n";
	exit(100);
}

void keyvaluesplit(const char *arg, std::string &key, std::string &value) {
	const char *delim = strchr(arg, '=');
	if (NULL == delim) {
		std::cerr << "Couldn't parse key-value argument: '" << arg << "'\n\n";
		syntax();
	}
	key = std::string(arg, delim - arg);
	value = std::string(delim + 1);
}

int main(int argc, char **argv) {
	int opt_sanitize = 0, opt_info_hash = 0, opt_show_urls = 0, opt_show_info = -1, opt_show_files = 0, opt_verify = 0;

	const struct option longopts[] = {
		{ "meta-filter-text", 1, 0, 0 },
		{ "meta-filter-number", 1, 0, 1 },
		{ "meta-filter-any", 1, 0, 2 },
		{ "meta-add-string", 1, 0, 3 },
		{ "meta-add-raw", 1, 0, 4 },
		{ "url-filter", 1, 0, 5}
	};

	/* only used for sanitize/info */
	TorrentSanitize san;
	san.filter_meta_text.load(".*");
	san.filter_meta_num.load(".*");
	san.filter_meta_other.load("");

	std::string key, value;

	int c;
	while (-1 != (c = getopt_long(argc, argv, "ifdvshu", longopts, NULL))) {
		switch (c) {
		case 0:
			if (!san.filter_meta_text.load(optarg)) return 2;
			break;
		case 1:
			if (!san.filter_meta_num.load(optarg)) return 2;
			break;
		case 2:
			if (!san.filter_meta_other.load(optarg)) return 2;
			break;
		case 3:
			keyvaluesplit(optarg, key, value);
			san.add_new_meta_entry(key, value);
			break;
		case 4:
			keyvaluesplit(optarg, key, value);
			san.add_new_raw_meta_entry(key, value);
			break;
		case 5:
			if (!san.loadUrlConfig(std::string(optarg))) return 2;
			break;
		case 'i':
			opt_show_info = 1;
			break;
		case 'f':
			opt_show_files = 1;
			break;
		case 'd':
			san.debug = true;
			break;
		case 'v':
			san.check_info_utf8 = true;
			break;
		case 's':
			opt_sanitize = 1;
			break;
		case 'h':
			opt_info_hash = 1;
			if (-1 == opt_show_info) opt_show_info = 0;
			break;
		case 'u':
			opt_show_urls = 1;
			if (-1 == opt_show_info) opt_show_info = 0;
			break;
		case '?':
		default:
			syntax();
			break;
		}
	}

	int filenames = argc - optind;

	if (opt_sanitize) {
		if (1 != filenames && 2 != filenames) syntax();

		san.show_paths = false;

		Torrent t(san);
		if (!t.load(std::string(argv[optind]))) {
			std::cerr << t.filename() << ": " << t.lasterror() << std::endl;
			return 1;
		}
		if (opt_info_hash) std::cout << t.infohash() << std::endl;
		t.sanitize_announce_urls(san);
		if (2 == filenames) writeAtomicFile(std::string(argv[optind+1]), t);
		if (opt_show_info > 0) t.print_details();
	} else if (opt_show_info) {
		/* show info */;
		if (1 != filenames) syntax();

		san.show_paths = opt_show_files;

		Torrent t(san);
		if (!t.load(std::string(argv[optind]))) {
			std::cerr << t.filename() << ": " << t.lasterror() << std::endl;
			return 1;
		}
		t.print_details();
	} else if (opt_info_hash) {
		/* show info hash, optionally urls */;
		if (1 != filenames) syntax();

		TorrentAnnounceInfo t;
		if (!t.load(std::string(argv[optind]))) {
			std::cerr << t.filename() << ": " << t.lasterror() << std::endl;
			return 1;
		}
		std::cout << t.infohash() << std::endl;
		if (opt_show_urls) {
			std::cout << t.t_announce << "\n";
			for (size_t i = 0; i < t.t_announce_list.size(); i++) {
				for (size_t j = 0; j < t.t_announce_list[i].size(); j++) {
					std::cout << t.t_announce_list[i][j] << "\n";
				}
			}
		}
	} else {
		/* show urls */
		if (1 != filenames) syntax();

		TorrentAnnounce t;
		if (!t.load(std::string(argv[optind]))) {
			std::cerr << t.filename() << ": " << t.lasterror() << std::endl;
			return 1;
		}
		std::cout << t.t_announce << "\n";
		for (size_t i = 0; i < t.t_announce_list.size(); i++) {
			for (size_t j = 0; j < t.t_announce_list[i].size(); j++) {
				std::cout << t.t_announce_list[i][j] << "\n";
			}
		}
	}
	return 0;
}
