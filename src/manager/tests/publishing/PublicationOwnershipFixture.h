#pragma once

#include "manager/hash/Blake3Hasher.h"
#include "manager/install/InstalledReleaseStore.h"
#include "manager/payload/PayloadPathPolicy.h"
#include "manager/service/PublishService.h"
#include "manager/announce/AnnounceReceiver.h"
#include "manager/trust/DirectoryKeyRegistry.h"
#include "../InstallServiceTestSupport.h"
#include "../../../../tests/support/TemporaryTree.h"

namespace wgrd::manager::tests {
class PublicationOwnershipFixture {
public:
    explicit PublicationOwnershipFixture(const bool useCustomStore = false)
        : tree("publicationownership")
        , modsDirectory(tree.Root() / "Mods")
        , dataDirectory(tree.Root() / "data")
        , defaultStore(dataDirectory / "installed")
        , customStore(dataDirectory / "custom")
        , registry(dataDirectory / "registry")
        , receiver(registry, nullptr)
        , publisher(modsDirectory, dataDirectory, chunker, hasher, policy, torrentBuilder,
              receiver, receiver, registry, nullptr, useCustomStore ? &customStore : nullptr) {
        tree.WriteFile("Mods/demo/pack.dat", 64, 1);
        REQUIRE(publisher.CreateKey("tester", tree.Root() / "publisher.wgrdkey",
            "correct horse battery").has_value());
    }

    wgrd::tests::TemporaryTree tree;
    std::filesystem::path modsDirectory;
    std::filesystem::path dataDirectory;
    InstalledReleaseStore defaultStore;
    InstalledReleaseStore customStore;
    FixedSizeChunker chunker;
    Blake3Hasher hasher;
    PayloadPathPolicy policy;
    StubTorrentBuilder torrentBuilder;
    DirectoryKeyRegistry registry;
    AnnounceReceiver receiver;
    PublishService publisher;
};
}
