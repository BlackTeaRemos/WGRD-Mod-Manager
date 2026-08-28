#include "manager/install/InstalledReleaseStore.h"
#include "manager/service/ModRemovalService.h"
#include "manager/service/OrderService.h"

#include "domain/interfaces/trust/IAnnounceCatalogue.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using wgrd::domain::AnnounceSummary;
using wgrd::domain::IAnnounceCatalogue;
using wgrd::domain::InstalledRelease;
using wgrd::domain::PublisherFingerprint;
using wgrd::domain::RemovalError;
using wgrd::manager::InstalledReleaseStore;
using wgrd::manager::ModRemovalService;

namespace {
class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-removal" / label;

		std::error_code failure;
		std::filesystem::remove_all(_root, failure);
		std::filesystem::create_directories(_root, failure);
	}

	~TemporaryTree() {
		std::error_code failure;
		std::filesystem::remove_all(_root, failure);
	}

	[[nodiscard]] const std::filesystem::path& Root() const {
		return _root;
	}

private:
	std::filesystem::path _root;
};

class EmptyCatalogue final : public IAnnounceCatalogue {
public:
	~EmptyCatalogue() override = default;

	[[nodiscard]] std::vector<AnnounceSummary> Summaries() const override {
		return {};
	}

	[[nodiscard]] std::optional<std::vector<std::uint8_t>> Record(
		const PublisherFingerprint&,
		std::string_view
	) const override {
		return std::nullopt;
	}

	[[nodiscard]] bool WouldAccept(
		const PublisherFingerprint&,
		std::string_view,
		std::uint64_t
	) const override {
		return true;
	}
};

InstalledRelease MakeRecord(std::string modName) {
	const std::string identifier = "e65389f24dc7be9a/" + modName;

	return InstalledRelease{identifier, std::move(modName), 3, std::string(64, 'a')};
}

void WriteModFolder(const std::filesystem::path& modsDirectory, const std::string_view modName) {
	std::error_code failure;
	std::filesystem::create_directories(modsDirectory / modName, failure);

	std::ofstream payload(modsDirectory / modName / "pack.dat", std::ios::binary | std::ios::trunc);
	payload << "payload bytes";
}

wgrd::domain::GameInstallation MakeInstallation(const std::filesystem::path& root) {
	const std::filesystem::path mods = root / "Mods";

	std::error_code failure;
	std::filesystem::create_directories(mods, failure);

	return wgrd::domain::GameInstallation{root, mods, mods / "load_order.txt"};
}
}

TEST_CASE("installed releases round trip through the store") {
	const TemporaryTree tree("store");
	const InstalledReleaseStore store(tree.Root() / "installed");

	const InstalledRelease record = MakeRecord("angel_maps");

	REQUIRE_FALSE(store.Find(record.identifier).has_value());
	REQUIRE(store.Save(record));

	const auto found = store.Find(record.identifier);

	REQUIRE(found.has_value());
	REQUIRE(found->version == 3);
	REQUIRE(found->modName == "angel_maps");
	REQUIRE(store.LoadAll().size() == 1);

	REQUIRE(store.Remove(record.identifier));
	REQUIRE_FALSE(store.Find(record.identifier).has_value());
}

TEST_CASE("installed release identifiers cannot escape the folder") {
	const TemporaryTree tree("escape");
	const InstalledReleaseStore store(tree.Root() / "installed");

	REQUIRE_FALSE(store.Find("../../outside").has_value());
	REQUIRE_FALSE(store.Remove("../../outside"));

	const InstalledRelease hostile{"../../outside", "outside", 1, std::string(64, 'a')};
	REQUIRE_FALSE(store.Save(hostile));
}

TEST_CASE("removal deletes the folder and forgets the record") {
	const TemporaryTree tree("remove");
	const auto installation = MakeInstallation(tree.Root());

	WriteModFolder(installation.modsDirectory, "angel_maps");

	const InstalledReleaseStore store(tree.Root() / "installed");
	const InstalledRelease record = MakeRecord("angel_maps");
	REQUIRE(store.Save(record));

	const EmptyCatalogue catalogue;
	wgrd::manager::OrderService order(installation);

	ModRemovalService removal(
		installation.modsDirectory,
		store,
		catalogue,
		&order,
		nullptr,
		nullptr
	);

	REQUIRE(std::filesystem::is_directory(installation.modsDirectory / "angel_maps"));

	REQUIRE(removal.Remove(record.identifier).has_value());

	REQUIRE_FALSE(std::filesystem::exists(installation.modsDirectory / "angel_maps"));
	REQUIRE_FALSE(store.Find(record.identifier).has_value());
}

TEST_CASE("removal drops the mod from the load order") {
	const TemporaryTree tree("order");
	const auto installation = MakeInstallation(tree.Root());

	WriteModFolder(installation.modsDirectory, "angel_maps");
	WriteModFolder(installation.modsDirectory, "other_mod");

	const InstalledReleaseStore store(tree.Root() / "installed");
	REQUIRE(store.Save(MakeRecord("angel_maps")));

	const EmptyCatalogue catalogue;
	wgrd::manager::OrderService order(installation);

	const auto angel = wgrd::domain::InstallFolder::Parse("angel_maps");
	const auto other = wgrd::domain::InstallFolder::Parse("other_mod");
	REQUIRE(angel.has_value());
	REQUIRE(other.has_value());

	order.SetEnabled(*angel, true);
	order.SetEnabled(*other, true);

	REQUIRE(order.Current().enabledCount == 2);

	ModRemovalService removal(
		installation.modsDirectory,
		store,
		catalogue,
		&order,
		nullptr,
		nullptr
	);

	REQUIRE(removal.Remove("e65389f24dc7be9a/angel_maps").has_value());

	REQUIRE(order.Current().enabledCount == 1);

	std::ifstream written(installation.orderFile, std::ios::binary);
	REQUIRE(written);

	const std::string contents(
		(std::istreambuf_iterator<char>(written)),
		std::istreambuf_iterator<char>()
	);

	REQUIRE(contents.find("angel_maps") == std::string::npos);
	REQUIRE(contents.find("other_mod") != std::string::npos);
}

TEST_CASE("removal refuses an unknown identifier") {
	const TemporaryTree tree("unknown");
	const auto installation = MakeInstallation(tree.Root());

	const InstalledReleaseStore store(tree.Root() / "installed");
	const EmptyCatalogue catalogue;
	wgrd::manager::OrderService order(installation);

	ModRemovalService removal(
		installation.modsDirectory,
		store,
		catalogue,
		&order,
		nullptr,
		nullptr
	);

	REQUIRE(removal.Remove("e65389f24dc7be9a/missing").error() == RemovalError::UnknownIdentifier);
}

TEST_CASE("removal refuses a mod whose folder is absent") {
	const TemporaryTree tree("absent");
	const auto installation = MakeInstallation(tree.Root());

	const InstalledReleaseStore store(tree.Root() / "installed");
	REQUIRE(store.Save(MakeRecord("angel_maps")));

	const EmptyCatalogue catalogue;
	wgrd::manager::OrderService order(installation);

	ModRemovalService removal(
		installation.modsDirectory,
		store,
		catalogue,
		&order,
		nullptr,
		nullptr
	);

	REQUIRE(
		removal.Remove("e65389f24dc7be9a/angel_maps").error() == RemovalError::NotInstalled
	);
}

TEST_CASE("removal refuses a mod name that escapes the mods folder") {
	const TemporaryTree tree("traversal");
	const auto installation = MakeInstallation(tree.Root());

	std::error_code failure;
	std::filesystem::create_directories(tree.Root() / "outside", failure);

	const InstalledReleaseStore store(tree.Root() / "installed");

	const InstalledRelease hostile{
		"e65389f24dc7be9a/traversal", "../outside", 1, std::string(64, 'a')
	};

	const EmptyCatalogue catalogue;
	wgrd::manager::OrderService order(installation);

	ModRemovalService removal(
		installation.modsDirectory,
		store,
		catalogue,
		&order,
		nullptr,
		nullptr
	);

	const auto refused = removal.Remove(hostile.identifier);

	REQUIRE_FALSE(refused.has_value());
	REQUIRE(std::filesystem::is_directory(tree.Root() / "outside"));
}
