find_package(LibtorrentRasterbar CONFIG REQUIRED)

wgrd_add_module(downloader
    SOURCES
        chunk/GearTable.h
        chunk/FastCdcChunker.h
        chunk/FastCdcChunker.cpp
        torrent/chunkset/ChunkFolderSource.h
        torrent/chunkset/ChunkFolderSource.cpp
        storage/ChunkLocator.h
        storage/ChunkLocator.cpp
        storage/InstalledFolderStorage.h
        storage/InstalledFolderStorage.cpp
        torrent/build/ChunkMerkleHasher.h
        torrent/build/ChunkMerkleHasher.cpp
        torrent/build/VirtualChunkSetTorrent.h
        torrent/build/VirtualChunkSetTorrent.cpp
        torrent/chunkset/ChunkSetLayout.h
        torrent/chunkset/ChunkSetLayout.cpp
        torrent/chunkset/ChunkSetMaterialiser.h
        torrent/chunkset/ChunkSetMaterialiser.cpp
        torrent/build/ChunkSetTorrentBuilder.h
        torrent/build/ChunkSetTorrentBuilder.cpp
        torrent/build/ChunkSetTorrent.h
        torrent/build/ChunkSetTorrent.cpp
        transfer/SwarmNode.h
        transfer/SwarmNode.cpp
        transfer/TorrentSession.h
        transfer/TorrentSession.cpp
    DEPS
        domain
)

target_link_libraries(wgrd_downloader PUBLIC LibtorrentRasterbar::torrent-rasterbar)

if(WGRD_BUILD_TESTS)
    add_subdirectory(tests)
endif()
