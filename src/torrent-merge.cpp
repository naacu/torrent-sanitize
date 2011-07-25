#include "common.h"

#include <iostream>
#include <fstream>

extern "C" {
#include <unistd.h>
}

void syntax() {
	std::cerr << "Syntax: torrent-merge [-d] [-f url-filter ] destination.torrent [source.torrents...]\n"
		"\n"
		"\t\t-d: debug\n";
	exit(100);
}

int main(int argc, char **argv) {
	int opt;
	torrent::TorrentSanitize san;

// 	torrent::setDebugActive(true);

	while (-1 != (opt = getopt(argc, argv, "df:"))) {
		switch (opt) {
		case 'd':
			san.debug = true;
			break;
		case 'f':
			if (!san.loadUrlConfig(optarg)) return 2;
			break;
		default:
			syntax();
		}
	}

	if (argc - optind < 1) syntax();

	torrent::TorrentAnnounceInfo dest;

	if (!dest.load(std::string(argv[optind]))) {
		std::cerr << dest.filename() << ": " << dest.lasterror() << std::endl;
		return 1;
	}

	std::string hash = dest.infohash();
	std::cout << "Merging announce urls for " << hash << "\n";

	torrent::AnnounceList list(san);
	list.force_merge(san.additional_announce_urls);
	list.merge(dest);

	for (int i = optind + 1; i < argc; i++) {
		torrent::TorrentAnnounce source;
		if (!source.load(std::string(argv[i]))) {
			std::cerr << source.filename() << ": " << source.lasterror() << std::endl;
			return 1;
		}

		list.merge(source);
	}

	if (list.list.empty()) {
		for (size_t i = 0; i < hash.length(); i++) hash[i] = ::toupper(hash[i]);
		dest.t_announce = std::string("dht://") + hash;
		dest.t_announce_list.clear();
	} else {
		dest.t_announce = list.list[0][0];
		dest.t_announce_list = list.list;
	}

	if (san.debug) {
		std::cout << "Announce-url: " << dest.t_announce << std::endl;
		std::cout << "Announce-list:\n";
		for (size_t i = 0; i < dest.t_announce_list.size(); i++) {
			std::cout << " - [ ";
			for (size_t j = 0; j < dest.t_announce_list[i].size(); j++) {
				if (j > 0) std::cout << ", ";
				std::cout << dest.t_announce_list[i][j];
			}
			std::cout << " ]\n";
		}
	}

	writeAtomicFile(std::string(argv[optind]), dest);

	return 0;
}
