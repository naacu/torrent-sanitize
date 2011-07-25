#include "common.h"

#include <iostream>
#include <fstream>

int main(int argc, char **argv) {
	static const int html_output = 1;

// 	torrent::setDebugActive(true);

	if (argc < 3) {
		std::cerr << "Syntax: " << argv[0] << " filter.txt urls.txt\n";
		return 1;
	}

	torrent::TorrentSanitize san;

	if (!san.loadUrlConfig(argv[1])) return 2;

	std::ifstream urlfile(argv[2]);
	std::string l;

	if (html_output) {
		std::cout << "<html><head><title>Filter results</title><style type=\"text/css\" media=\"all\">* {font-family: verdana, arial, helvetica, sans-serif;font-size: 10px;} .red{color:red}.green{color:green}.blue{color:blue}</style></head><body><table><tr><th>Status</th><th>Original</th><th>Result</th></tr>\n";
	}

	while (urlfile.good()) {
		std::getline(urlfile, l);
		if (l.empty()) continue;

		std::vector<torrent::AnnounceUrl> annurls;
		annurls = san.filterUrl(l);
		if (annurls.size() == 0) {
			if (html_output) {
				std::cout << "<tr><td class=\"red\">REMOVED</td><td>" << l << "</td></tr>\n";
			} else {
				std::cout << "REMOVED   " << l << "\n";
			}
		} else if (annurls.size() == 1 && annurls[0].url == l) {
			if (html_output) {
				std::cout << "<tr><td class=\"green\">PASSED</td><td>" << l << "</td><td>" << l << "</td></tr>\n";
			} else {
				std::cout << "PASSED    " << l << "\n";
			}
		} else {
			if (html_output) {
				std::cout << "<tr><td class=\"blue\">REPLACED</td><td>" << l << "</td><td class=\"blue\">";
			} else {
				std::cout << "REPLACED  " << l << "   ";
			}
			for (int i = 0; i < annurls.size(); i++) {
				std::cout << " " << annurls[i].url;
			}
			if (html_output) {
				std::cout << "</td></tr>\n";
			} else {
				std::cout << "\n";
			}
		}
	}

	if (html_output) std::cout << "</table></body></html>\n";
}
