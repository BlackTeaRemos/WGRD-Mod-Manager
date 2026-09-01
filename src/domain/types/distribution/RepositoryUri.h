#pragma once

#include <string>
#include <string_view>

namespace wgrd::domain {
class RepositoryUri {
public:
	static constexpr std::string_view SCHEME = "https://";
	static constexpr std::string_view HOST = "github.com/";

	[[nodiscard]] static std::string_view Slug(std::string_view repository);

	[[nodiscard]] static std::string Https(std::string_view repository);
};
}
