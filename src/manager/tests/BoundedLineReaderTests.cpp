#include "manager/io/BoundedLineReader.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

using wgrd::manager::BoundedLineReader;

namespace {
class TemporaryFolder {
public:
	explicit TemporaryFolder(const std::string_view label) {
		_path = std::filesystem::temp_directory_path() / "wgrd-tests" / label;

		std::error_code failure;
		std::filesystem::remove_all(_path, failure);
		std::filesystem::create_directories(_path, failure);
	}

	~TemporaryFolder() {
		std::error_code failure;
		std::filesystem::remove_all(_path, failure);
	}

	[[nodiscard]] std::filesystem::path Write(const std::string_view name, const std::string_view contents) const {
		const std::filesystem::path target = _path / std::string(name);

		std::ofstream output(target, std::ios::binary | std::ios::trunc);
		output << contents;

		return target;
	}

	[[nodiscard]] const std::filesystem::path& Value() const {
		return _path;
	}

private:
	std::filesystem::path _path;
};
}

TEST_CASE("bounded reader returns the first line without its terminator") {
	const TemporaryFolder folder("boundedline");

	REQUIRE(BoundedLineReader::Read(folder.Write("plain", "ladder\nsecond\n"), 32) == "ladder");
	REQUIRE(BoundedLineReader::Read(folder.Write("crlf", "ladder\r\n"), 32) == "ladder");
	REQUIRE(BoundedLineReader::Read(folder.Write("bare", "ladder"), 32) == "ladder");
	REQUIRE(BoundedLineReader::Read(folder.Write("empty", ""), 32) == "");
}

TEST_CASE("bounded reader refuses a line longer than the limit") {
	const TemporaryFolder folder("boundedlimit");

	REQUIRE(BoundedLineReader::Read(folder.Write("exact", std::string(8, 'a')), 8).has_value());
	REQUIRE_FALSE(BoundedLineReader::Read(folder.Write("over", std::string(9, 'a')), 8).has_value());
	REQUIRE_FALSE(BoundedLineReader::Read(folder.Write("huge", std::string(4096, 'a')), 8).has_value());
}

TEST_CASE("bounded reader reports a missing file") {
	const TemporaryFolder folder("boundedmissing");

	REQUIRE_FALSE(BoundedLineReader::Read(folder.Value() / "absent", 8).has_value());
}
