#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace wgrd::tests {
class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label);
	~TemporaryTree() noexcept;
	TemporaryTree(const TemporaryTree&) = delete;
	TemporaryTree& operator=(const TemporaryTree&) = delete;

	[[nodiscard]] const std::filesystem::path& Root() const;
	void Preserve();
	void WriteFile(std::string_view relativePath, std::size_t length, unsigned char seed) const;

private:
	std::filesystem::path _root;
	bool _preserve = false;
};
}
