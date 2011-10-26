
PROJECT(torrent-sanitize)

cmake_minimum_required(VERSION 2.8)
if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

ADD_LIBRARY(Base STATIC
	src/buffer.cpp
	src/debug.cpp
	src/utils.cpp
	src/torrentbase.cpp
	src/sanitize-settings.cpp
	src/torrent.cpp
	src/torrent-pcre.cpp
)

ADD_EXECUTABLE(torrent-merge
	src/torrent-merge.cpp
)

ADD_EXECUTABLE(torrent-sanitize
	src/torrent-sanitize.cpp
)

ADD_EXECUTABLE(torrent-test-filter
	src/torrent-test-filter.cpp
)

TARGET_LINK_LIBRARIES(torrent-merge Base pcrecpp ssl crypto)
TARGET_LINK_LIBRARIES(torrent-sanitize Base pcrecpp ssl crypto)
TARGET_LINK_LIBRARIES(torrent-test-filter Base pcrecpp ssl crypto)
