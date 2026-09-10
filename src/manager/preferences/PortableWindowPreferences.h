#pragma once

#include "domain/interfaces/preferences/IWindowPreferences.h"

#include <filesystem>

namespace wgrd::manager {
class PortableWindowPreferences final : public domain::IWindowPreferences {
public:
	explicit PortableWindowPreferences(std::filesystem::path dataDirectory);

	~PortableWindowPreferences() override = default;

	[[nodiscard]] domain::WindowPreferences Load() const override;

	[[nodiscard]] bool Save(const domain::WindowPreferences& preferences) const override;

private:
	std::filesystem::path _file;
};
}
