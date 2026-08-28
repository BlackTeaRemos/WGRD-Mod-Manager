#include "manager/inspect/OrderInspector.h"

#include <catch2/catch_test_macros.hpp>

using wgrd::domain::Annotation;
using wgrd::domain::AnnotationCategory;
using wgrd::domain::AnnotationSeverity;
using wgrd::domain::GameBuild;
using wgrd::domain::InstalledMod;
using wgrd::domain::InstallFolder;
using wgrd::domain::LoadOrder;
using wgrd::domain::OrderEntry;
using wgrd::manager::OrderInspector;

namespace {
InstallFolder MakeFolder(const std::string_view name) {
	const auto folder = InstallFolder::Parse(name);
	REQUIRE(folder.has_value());
	return *folder;
}

InstalledMod MakeMod(const std::string_view name, std::vector<std::uint32_t> builds) {
	std::vector<GameBuild> parsed;
	for (const std::uint32_t build : builds) {
		parsed.emplace_back(build);
	}
	return InstalledMod{MakeFolder(name), std::move(parsed)};
}
}

TEST_CASE("matching mod is silent") {
	const LoadOrder order(std::vector{OrderEntry{MakeFolder("alpha"), true}});
	const std::vector<InstalledMod> installed{MakeMod("alpha", {131635})};

	const OrderInspector inspector;
	REQUIRE(inspector.Inspect(order, installed).empty());
}

TEST_CASE("absent folder blocks") {
	const LoadOrder order(std::vector{OrderEntry{MakeFolder("ghost"), true}});
	const std::vector<InstalledMod> installed;

	const OrderInspector inspector;
	const std::vector<Annotation> annotations = inspector.Inspect(order, installed);

	REQUIRE(annotations.size() == 1);
	REQUIRE(annotations.front().category == AnnotationCategory::FolderAbsent);
	REQUIRE(annotations.front().severity == AnnotationSeverity::Blocking);
	REQUIRE(annotations.front().folder.Value() == "ghost");
}

TEST_CASE("disabled entries are skipped") {
	const LoadOrder order(std::vector{OrderEntry{MakeFolder("ghost"), false}});
	const std::vector<InstalledMod> installed;

	const OrderInspector inspector;
	REQUIRE(inspector.Inspect(order, installed).empty());
}

TEST_CASE("reports every absent entry") {
	const LoadOrder order(std::vector{
			OrderEntry{MakeFolder("good"), true}, OrderEntry{MakeFolder("ghost"), true}, OrderEntry{MakeFolder("vanished"), true}
		}
	);
	const std::vector<InstalledMod> installed{MakeMod("good", {131635})};

	const OrderInspector inspector;
	REQUIRE(inspector.Inspect(order, installed).size() == 2);
}

TEST_CASE("emits no other category") {
	const LoadOrder order(std::vector{
			OrderEntry{MakeFolder("good"), true}, OrderEntry{MakeFolder("ghost"), true}, OrderEntry{MakeFolder("hidden"), false}
		}
	);
	const std::vector<InstalledMod> installed{MakeMod("good", {131635})};

	const OrderInspector inspector;
	for (const Annotation& annotation : inspector.Inspect(order, installed)) {
		REQUIRE(annotation.category == AnnotationCategory::FolderAbsent);
	}
}
