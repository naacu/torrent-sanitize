
Helper programs for torrent hosting

## Upload handling ##

First extract the info-hash:

	torrent-sanitize -h $tmpfile

If it returns errors (status code != 0) you probably don't want the torrent.

It might be a good idea to sync modifications for an info hash,
so lock with a file like /tmp/torrent-update-$infohash.lock

The binaries in this source will try to do atomic updates, i.e. first write a
temporary file in the same directory, then rename it to the real file.

## Initial upload ##

Run more checks, change some meta data:

	torrent-sanitize -s -v --url-filter url-filter.example --meta-filter-text 'comment|created by' --meta-filter-number 'creation date' --meta-add-string 'comment=uploaded on http://my-torrent-hosting.example.com/' $tmpfile $tmpfile

## Upload for already known info hash ##

Update announce urls:

	torrent-merge -f url-filter.example $destfile $tmpfile

## Run new url filter on old torrents ##

	torrent-merge -f url-filter.example $oldtorrent
