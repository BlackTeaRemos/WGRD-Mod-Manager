#include "manager/profile/ProfileCodec.h"
#include "manager/profile/ProfileNameRule.h"
#include "manager/profile/ProfileStore.h"
#include "manager/service/OrderService.h"
#include "manager/service/ProfileService.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using wgrd::domain::InstallFolder;
using wgrd::domain::LoadOrder;
using wgrd::domain::OrderEntry;
using wgrd::domain::Profile;
using wgrd::domain::ProfileError;
using wgrd::manager::ProfileCodec;
using wgrd::manager::ProfileNameRule;
using wgrd::manager::ProfileService;
using wgrd::manager::ProfileStore;

namespace {
class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-profile" / label;

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

InstallFolder Folder(const std::string_view name) {
	const auto folder = InstallFolder::Parse(name);
	REQUIRE(folder.has_value());
	return *folder;
}

Profile MakeProfile(std::string name) {
	std::vector<OrderEntry> entries;
	entries.push_back(OrderEntry{Folder("alpha_mod"), true});
	entries.push_back(OrderEntry{Folder("beta_mod"), false});
	entries.push_back(OrderEntry{Folder("gamma_mod"), true});

	return Profile(std::move(name), LoadOrder(std::move(entries)));
}

wgrd::domain::GameInstallation MakeInstallation(const std::filesystem::path& root) {
	const std::filesystem::path mods = root / "Mods";

	std::error_code failure;
	std::filesystem::create_directories(mods / "alpha_mod", failure);
	std::filesystem::create_directories(mods / "gamma_mod", failure);

	return wgrd::domain::GameInstallation{root, mods, mods / "load_order.txt"};
}
}

TEST_CASE("profile names accept only safe characters") {
	REQUIRE(ProfileNameRule::Accepts("sunday_co-op.2"));
	REQUIRE_FALSE(ProfileNameRule::Accepts(""));
	REQUIRE_FALSE(ProfileNameRule::Accepts("has space"));
	REQUIRE_FALSE(ProfileNameRule::Accepts("slash/name"));
	REQUIRE_FALSE(ProfileNameRule::Accepts("back\\slash"));
	REQUIRE_FALSE(ProfileNameRule::Accepts(std::string(ProfileNameRule::LENGTH_LIMIT + 1, 'a')));
}

TEST_CASE("profile round trips through the codec") {
	const Profile original = MakeProfile("ladder");

	const auto decoded = ProfileCodec::Decode(ProfileCodec::Encode(original));

	REQUIRE(decoded.has_value());
	REQUIRE(decoded->Name() == "ladder");
	REQUIRE(decoded->Order().Entries().size() == 3);
	REQUIRE(decoded->Order().EnabledCount() == 2);
	REQUIRE(decoded->Order().Entries()[1].folder.Value() == "beta_mod");
	REQUIRE_FALSE(decoded->Order().Entries()[1].enabled);
}

TEST_CASE("malformed profile documents are refused") {
	REQUIRE_FALSE(ProfileCodec::Decode("not json").has_value());
	REQUIRE_FALSE(ProfileCodec::Decode("[]").has_value());
	REQUIRE_FALSE(ProfileCodec::Decode(R"({"entries":[]})").has_value());
	REQUIRE_FALSE(ProfileCodec::Decode(R"({"name":"has space","entries":[]})").has_value());
	REQUIRE_FALSE(ProfileCodec::Decode(R"({"name":"ok","entries":{}})").has_value());
	REQUIRE_FALSE(
		ProfileCodec::Decode(R"({"name":"ok","entries":[{"folder":"a/b","enabled":true}]})").has_value()
	);
	REQUIRE_FALSE(
		ProfileCodec::Decode(R"({"name":"ok","entries":[{"folder":"a"}]})").has_value()
	);
}

TEST_CASE("store saves loads and removes a profile") {
	const TemporaryTree tree("store");
	const ProfileStore store(tree.Root() / "profiles" / std::string(ProfileService::SHARED_FOLDER));

	REQUIRE_FALSE(store.Holds("ladder"));
	REQUIRE(store.Save(MakeProfile("ladder")));
	REQUIRE(store.Holds("ladder"));

	const auto loaded = store.Load("ladder");
	REQUIRE(loaded.has_value());
	REQUIRE(loaded->Order().Entries().size() == 3);

	REQUIRE(store.Save(MakeProfile("sunday")));
	REQUIRE(store.LoadAll().size() == 2);
	REQUIRE(store.LoadAll()[0].Name() == "ladder");

	REQUIRE(store.Remove("ladder"));
	REQUIRE_FALSE(store.Holds("ladder"));
	REQUIRE(store.LoadAll().size() == 1);
}

TEST_CASE("store refuses names that escape the folder") {
	const TemporaryTree tree("escape");
	const ProfileStore store(tree.Root() / "profiles" / std::string(ProfileService::SHARED_FOLDER));

	REQUIRE_FALSE(store.Holds("../outside"));
	REQUIRE_FALSE(store.Load("../outside").has_value());
	REQUIRE_FALSE(store.Remove("../outside"));
}

TEST_CASE("active marker survives a reload") {
	const TemporaryTree tree("active");
	const ProfileStore store(tree.Root() / "profiles" / std::string(ProfileService::SHARED_FOLDER));

	REQUIRE(store.ReadActive().empty());
	REQUIRE(store.WriteActive("ladder"));
	REQUIRE(store.ReadActive() == "ladder");

	const ProfileStore reopened(tree.Root() / "profiles" / std::string(ProfileService::SHARED_FOLDER));
	REQUIRE(reopened.ReadActive() == "ladder");
}

TEST_CASE("capturing the live order creates an active profile") {
	const TemporaryTree tree("capture");

	wgrd::manager::OrderService order(MakeInstallation(tree.Root()));
	ProfileService profiles(tree.Root() / "profiles", order, {});

	REQUIRE(profiles.Profiles().size() == 1);
	REQUIRE(profiles.Profiles()[0].name == ProfileService::DEFAULT_NAME);
	REQUIRE(profiles.Active() == ProfileService::DEFAULT_NAME);

	order.SetEnabled(Folder("alpha_mod"), true);

	REQUIRE(profiles.CaptureCurrent("ladder").has_value());

	REQUIRE(profiles.Profiles().size() == 2);
	REQUIRE(profiles.Profiles()[1].name == "ladder");
	REQUIRE(profiles.Profiles()[1].active);
	REQUIRE(profiles.Active() == "ladder");
}

TEST_CASE("capture refuses bad and duplicate names") {
	const TemporaryTree tree("capture-refuse");

	wgrd::manager::OrderService order(MakeInstallation(tree.Root()));
	ProfileService profiles(tree.Root() / "profiles", order, {});

	REQUIRE(profiles.CaptureCurrent("has space").error() == ProfileError::NameRejected);
	REQUIRE(profiles.CaptureCurrent("ladder").has_value());
	REQUIRE(profiles.CaptureCurrent("ladder").error() == ProfileError::NameTaken);
}

TEST_CASE("activating a profile rewrites the order file") {
	const TemporaryTree tree("activate");
	const auto installation = MakeInstallation(tree.Root());

	wgrd::manager::OrderService order(installation);
	ProfileService profiles(tree.Root() / "profiles", order, {});

	order.SetEnabled(Folder("alpha_mod"), true);
	order.SetEnabled(Folder("gamma_mod"), true);
	REQUIRE(profiles.CaptureCurrent("both").has_value());

	order.SetEnabled(Folder("gamma_mod"), false);
	REQUIRE(order.Current().enabledCount == 1);

	REQUIRE(profiles.Activate("both").has_value());
	REQUIRE(order.Current().enabledCount == 2);

	std::ifstream written(installation.orderFile, std::ios::binary);
	REQUIRE(written);

	const std::string contents(
		(std::istreambuf_iterator<char>(written)),
		std::istreambuf_iterator<char>()
	);

	REQUIRE(contents.find("alpha_mod") != std::string::npos);
	REQUIRE(contents.find("gamma_mod") != std::string::npos);
}

TEST_CASE("activating keeps installed mods the profile omits") {
	const TemporaryTree tree("activate-keep");

	wgrd::manager::OrderService order(MakeInstallation(tree.Root()));
	ProfileService profiles(tree.Root() / "profiles", order, {});

	std::vector<OrderEntry> entries;
	entries.push_back(OrderEntry{Folder("alpha_mod"), true});

	const ProfileStore store(tree.Root() / "profiles" / std::string(ProfileService::SHARED_FOLDER));
	REQUIRE(store.Save(Profile("narrow", LoadOrder(std::move(entries)))));

	profiles.Refresh();
	REQUIRE(profiles.Activate("narrow").has_value());

	REQUIRE(order.Current().enabledCount == 1);

	const bool gammaStillListed = std::any_of(
		order.Current().entries.begin(),
		order.Current().entries.end(),
		[](const wgrd::domain::OrderEntryView& entry) {
			return entry.folder.Value() == "gamma_mod";
		}
	);

	REQUIRE(gammaStillListed);
}

TEST_CASE("cloning copies the order under a new name") {
	const TemporaryTree tree("clone");

	wgrd::manager::OrderService order(MakeInstallation(tree.Root()));
	ProfileService profiles(tree.Root() / "profiles", order, {});

	order.SetEnabled(Folder("alpha_mod"), true);
	REQUIRE(profiles.CaptureCurrent("original").has_value());

	REQUIRE(profiles.Clone("original", "copy").has_value());
	REQUIRE(profiles.Profiles().size() == 3);

	REQUIRE(profiles.Clone("missing", "another").error() == ProfileError::NotFound);
	REQUIRE(profiles.Clone("original", "copy").error() == ProfileError::NameTaken);
}

TEST_CASE("removing the active profile clears the marker") {
	const TemporaryTree tree("remove");

	wgrd::manager::OrderService order(MakeInstallation(tree.Root()));
	ProfileService profiles(tree.Root() / "profiles", order, {});

	REQUIRE(profiles.CaptureCurrent("ladder").has_value());
	REQUIRE(profiles.Active() == "ladder");

	REQUIRE(profiles.Remove("ladder").has_value());
	REQUIRE(profiles.Profiles().size() == 1);
	REQUIRE(profiles.Profiles()[0].name == ProfileService::DEFAULT_NAME);
	REQUIRE(profiles.Active() == ProfileService::DEFAULT_NAME);

	REQUIRE(profiles.Remove("ladder").error() == ProfileError::NotFound);
	REQUIRE(profiles.Remove(ProfileService::DEFAULT_NAME).error() == ProfileError::DefaultProtected);
}

TEST_CASE("each steam account keeps its own store and default") {
	const TemporaryTree tree("peraccount");

	std::vector<wgrd::domain::SteamAccount> accounts;
	accounts.push_back(wgrd::domain::SteamAccount{"111", tree.Root() / "remote111", false});
	accounts.push_back(wgrd::domain::SteamAccount{"222", tree.Root() / "remote222", true});

	wgrd::manager::OrderService order(MakeInstallation(tree.Root()));
	ProfileService profiles(tree.Root() / "profiles", order, accounts);

	REQUIRE(profiles.CurrentAccount() == "222");

	REQUIRE(profiles.Profiles().size() == 1);
	REQUIRE(profiles.Profiles()[0].name == ProfileService::DEFAULT_NAME);
	REQUIRE(profiles.Profiles()[0].account == "222");

	REQUIRE(profiles.CaptureCurrent("ladder").has_value());
	REQUIRE(profiles.Profiles().size() == 2);

	const ProfileStore other(tree.Root() / "profiles" / "111");
	const auto otherProfiles = other.LoadAll();

	REQUIRE(otherProfiles.size() == 1);
	REQUIRE(otherProfiles[0].Name() == ProfileService::DEFAULT_NAME);
	REQUIRE(otherProfiles[0].Account() == "111");
	REQUIRE(other.ReadActive() == ProfileService::DEFAULT_NAME);
}

TEST_CASE("setting default replaces the profile file and keeps the order") {
	const TemporaryTree tree("setdefault");

	const std::filesystem::path remote = tree.Root() / "remote222";

	std::error_code failure;
	std::filesystem::create_directories(remote, failure);

	{
		std::ofstream live(remote / "PROFILE.wargameprofile", std::ios::binary | std::ios::trunc);
		live << "ESAVlive";
	}

	{
		std::ofstream other(remote / "vanilla.wargameprofile", std::ios::binary | std::ios::trunc);
		other << "ESAVvanilla";
	}

	std::vector<wgrd::domain::SteamAccount> accounts;
	accounts.push_back(wgrd::domain::SteamAccount{"222", remote, true});

	wgrd::manager::OrderService order(MakeInstallation(tree.Root()));
	ProfileService profiles(tree.Root() / "profiles", order, accounts);

	order.SetEnabled(Folder("alpha_mod"), true);
	REQUIRE(profiles.CaptureCurrent("ladder").has_value());

	const auto before = profiles.Load(ProfileService::DEFAULT_NAME);
	REQUIRE(before.has_value());

	const auto vanilla = std::ranges::find_if(
		profiles.Discovered(),
		[](const wgrd::domain::GameProfileFile& candidate) {
			return candidate.name == "vanilla";
		}
	);

	REQUIRE(vanilla != profiles.Discovered().end());
	REQUIRE(profiles.SetDefault(*vanilla).has_value());

	const std::filesystem::path stored =
			tree.Root() / "profiles" / "222" / "default.wargameprofile";

	std::ifstream input(stored, std::ios::binary);
	const std::string contents(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>()
	);

	REQUIRE(contents == "ESAVvanilla");

	const auto after = profiles.Load(ProfileService::DEFAULT_NAME);
	REQUIRE(after.has_value());
	REQUIRE(after->Order().Entries().size() == before->Order().Entries().size());
	REQUIRE(profiles.Profiles().size() == 2);
}

TEST_CASE("construction reconciles a stale active profile") {
	const TemporaryTree tree("reconcile");

	wgrd::manager::OrderService order(MakeInstallation(tree.Root()));

	{
		ProfileService profiles(tree.Root() / "profiles", order, {});
		REQUIRE(profiles.CaptureCurrent("ladder").has_value());
	}

	std::vector<OrderEntry> entries;
	entries.push_back(OrderEntry{Folder("alpha_mod"), true});

	const ProfileStore store(tree.Root() / "profiles" / std::string(ProfileService::SHARED_FOLDER));
	REQUIRE(store.Save(Profile("ladder", LoadOrder(std::move(entries)))));

	const auto stale = store.Load("ladder");
	REQUIRE(stale.has_value());
	REQUIRE(stale->Order().EnabledCount() == 1);

	const ProfileService reopened(tree.Root() / "profiles", order, {});

	const auto reconciled = reopened.Load("ladder");
	REQUIRE(reconciled.has_value());
	REQUIRE(reconciled->Order().EnabledCount() == 0);
}

TEST_CASE("active profile mirrors live order changes") {
	const TemporaryTree tree("mirror");

	wgrd::manager::OrderService order(MakeInstallation(tree.Root()));
	ProfileService profiles(tree.Root() / "profiles", order, {});

	order.SetChangeHandler([&profiles]() {
			profiles.SyncActive();
		}
	);

	REQUIRE(profiles.CaptureCurrent("ladder").has_value());
	REQUIRE(profiles.Active() == "ladder");

	const auto before = profiles.Load("ladder");
	REQUIRE(before.has_value());
	REQUIRE(before->Order().EnabledCount() == 0);

	order.SetEnabled(Folder("alpha_mod"), true);

	const auto after = profiles.Load("ladder");
	REQUIRE(after.has_value());
	REQUIRE(after->Order().EnabledCount() == 1);

	const auto untouched = profiles.Load(ProfileService::DEFAULT_NAME);
	REQUIRE(untouched.has_value());
	REQUIRE(untouched->Order().EnabledCount() == 0);
}

TEST_CASE("sync leaves other profiles alone when nothing is active") {
	const TemporaryTree tree("mirrorinactive");

	wgrd::manager::OrderService order(MakeInstallation(tree.Root()));
	ProfileService profiles(tree.Root() / "profiles", order, {});

	REQUIRE(profiles.CaptureCurrent("ladder").has_value());
	REQUIRE(profiles.Remove("ladder").has_value());

	order.SetChangeHandler([&profiles]() {
			profiles.SyncActive();
		}
	);

	order.SetEnabled(Folder("alpha_mod"), true);

	const auto fallback = profiles.Load(ProfileService::DEFAULT_NAME);
	REQUIRE(fallback.has_value());
	REQUIRE(fallback->Order().EnabledCount() == 1);
}

TEST_CASE("missing folders are counted against a profile") {
	const TemporaryTree tree("missing");

	wgrd::manager::OrderService order(MakeInstallation(tree.Root()));
	ProfileService profiles(tree.Root() / "profiles", order, {});

	std::vector<OrderEntry> entries;
	entries.push_back(OrderEntry{Folder("alpha_mod"), true});
	entries.push_back(OrderEntry{Folder("absent_mod"), true});

	const ProfileStore store(tree.Root() / "profiles" / std::string(ProfileService::SHARED_FOLDER));
	REQUIRE(store.Save(Profile("partial", LoadOrder(std::move(entries)))));

	profiles.Refresh();

	REQUIRE(profiles.Profiles().size() == 2);
	REQUIRE(profiles.Profiles()[1].name == "partial");
	REQUIRE(profiles.Profiles()[1].enabledCount == 2);
	REQUIRE(profiles.Profiles()[1].missingCount == 1);
}
