#include "manager/extensions/ExtensionPackageContract.h"
#include "manager/hash/Blake3Hasher.h"
#include "manager/install/ContentInstaller.h"
#include "manager/install/InstalledContentAuditor.h"
#include "manager/io/MappedFile.h"
#include "manager/manifest/ManifestBuilder.h"
#include "manager/manifest/ManifestCodec.h"
#include "manager/payload/PayloadPathPolicy.h"
#include "SingleChunkFixture.h"
#include "UnavailableChunkFixture.h"
#include "../../../../tests/support/TemporaryTree.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

TEST_CASE("Unsigned publication rejected") {
    const wgrd::tests::TemporaryTree tree("extensionpublish");
    tree.WriteFile("blacktearemos_extensions.dll", 64, 1);
    const wgrd::tests::SingleChunkFixture chunker;
    const wgrd::manager::Blake3Hasher hasher;
    const wgrd::manager::PayloadPathPolicy policy;
    const wgrd::manager::ManifestBuilder builder(chunker, hasher, policy);
    const auto publisher = wgrd::domain::PublisherFingerprint::FromHex("0011223344556677");
    REQUIRE(publisher.has_value());
    const auto manifest = builder.Build(tree.Root(), *publisher, "demo", 1);
    REQUIRE_FALSE(manifest.has_value());
    REQUIRE(manifest.error() == wgrd::domain::ManifestBuildError::PathRejected);
}

TEST_CASE("Extension declarations bounded") {
    nlohmann::json document{
        {"publisher", "0011223344556677"}, {"mod", "demo"}, {"version", std::uint64_t{1}},
        {"files", nlohmann::json::array({{
            {"path", "blacktearemos_extensions.dll"}, {"size", std::uint64_t{512}},
            {"chunks", nlohmann::json::array({{
                {"digest", std::string(64, 'a')}, {"length", std::uint64_t{512}}
            }})}
        }})}
    };
    const wgrd::manager::PayloadPathPolicy policy;
    const wgrd::manager::ManifestCodec codec(policy);
    const std::string valid = document.dump();
    REQUIRE(codec.Decode(std::vector<std::uint8_t>(valid.begin(), valid.end())).has_value());

    document["files"][0]["size"] = wgrd::manager::ExtensionPackageContract::MAX_PACKAGE_BYTES + 1;
    const std::string oversized = document.dump();
    const auto rejected = codec.Decode(std::vector<std::uint8_t>(oversized.begin(), oversized.end()));
    REQUIRE_FALSE(rejected.has_value());
    REQUIRE(rejected.error() == wgrd::domain::ManifestDecodeError::PathRejected);

    document["files"][0]["size"] = std::uint64_t{512};
    document["files"].push_back(document["files"][0]);
    document["files"][1]["path"] = "BLACKTEAREMOS_EXTENSIONS.DLL";
    const std::string duplicate = document.dump();
    REQUIRE_FALSE(codec.Decode(std::vector<std::uint8_t>(duplicate.begin(), duplicate.end())).has_value());
}

TEST_CASE("Rejected extension preserves targets") {
    const wgrd::tests::TemporaryTree tree("extensioninstall");
    tree.WriteFile("blacktearemos_extensions.dll", 16, 1);
    tree.WriteFile("pack.dat", 17, 2);
    tree.WriteFile("blacktearemos_extensions.dll.wgrdmm-new", 64, 3);
    tree.WriteFile("pack.dat.wgrdmm-new", 64, 4);
    const wgrd::manager::Blake3Hasher hasher;
    const auto digest = [&]() {
        const auto mapped = wgrd::manager::MappedFile::Open(tree.Root() / "blacktearemos_extensions.dll.wgrdmm-new");
        REQUIRE(mapped.has_value());
        return hasher.Hash(mapped->Data());
    }();
    const wgrd::domain::ChunkPlacement placement{
        digest, 0, 64, wgrd::domain::ChunkSourceKind::Remote, {}, 0
    };
    const wgrd::domain::InstallPlan plan({
        {"pack.dat", 64, {placement}}, {"blacktearemos_extensions.dll", 64, {placement}}
    }, {});
    wgrd::tests::UnavailableChunkFixture source;
    const wgrd::manager::ContentInstaller installer(hasher);
    const auto result = installer.ApplyPlaced(plan, tree.Root(), source);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == wgrd::manager::InstallError::ExtensionRejected);
    REQUIRE(std::filesystem::file_size(tree.Root() / "blacktearemos_extensions.dll") == 16);
    REQUIRE(std::filesystem::file_size(tree.Root() / "pack.dat") == 17);
}

TEST_CASE("Existing unapproved extension rejected") {
    const wgrd::tests::TemporaryTree tree("extensionunchanged");
    tree.WriteFile("blacktearemos_extensions.dll", 64, 1);
    const wgrd::manager::Blake3Hasher hasher;
    const wgrd::manager::ContentInstaller installer(hasher);
    wgrd::tests::UnavailableChunkFixture source;
    const auto result = installer.Apply(wgrd::domain::InstallPlan({}, {}), tree.Root(), source);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == wgrd::manager::InstallError::ExtensionRejected);
}

TEST_CASE("Audit checks extension approval") {
    const wgrd::tests::TemporaryTree tree("extensionaudit");
    tree.WriteFile("blacktearemos_extensions.dll", 64, 1);
    const wgrd::manager::Blake3Hasher hasher;
    const auto mapped = wgrd::manager::MappedFile::Open(tree.Root() / "blacktearemos_extensions.dll");
    REQUIRE(mapped.has_value());
    const auto publisher = wgrd::domain::PublisherFingerprint::FromHex("0011223344556677");
    REQUIRE(publisher.has_value());
    const wgrd::domain::ModManifest manifest(*publisher, "demo", 1, {{
        "blacktearemos_extensions.dll", 64, {{hasher.Hash(mapped->Data()), 0, 64}}
    }});
    const wgrd::manager::InstalledContentAuditor auditor(hasher);
    const auto audit = auditor.Examine(manifest, tree.Root(), {});
    REQUIRE(audit.damagedChunks == 1);
}
