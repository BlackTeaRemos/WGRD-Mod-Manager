#include "manager/preferences/PortableWindowPreferences.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <system_error>
#include <utility>

namespace wgrd::manager {
PortableWindowPreferences::PortableWindowPreferences(std::filesystem::path dataDirectory)
	: _file(std::move(dataDirectory) / "window.json") {}

domain::WindowPreferences PortableWindowPreferences::Load() const {
	std::error_code failure;
	const std::uintmax_t size = std::filesystem::file_size(_file, failure);
	if (failure || size > 4096) {
		return {};
	}

	std::ifstream input(_file, std::ios::binary);
	std::string bytes(static_cast<std::size_t>(size), '\0');
	if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
		return {};
	}

	const nlohmann::json document = nlohmann::json::parse(bytes, nullptr, false);
	if (!document.is_object()) {
		return {};
	}

	domain::WindowPreferences preferences;
	for (const char* field : {"left", "top", "width", "height", "dpi"}) {
		if (!document.contains(field) || !document[field].is_number_integer()) {
			return {};
		}
		if (document[field].is_number_unsigned() && document[field].get<std::uint64_t>() > 100000) {
			return {};
		}
		const auto value = document[field].get<std::int64_t>();
		if (value < -100000 || value > 100000) {
			return {};
		}
	}

	for (const char* field : {"positioned", "maximized", "backgroundOnClose"}) {
		if (!document.contains(field) || !document[field].is_boolean()) {
			return {};
		}
	}

	preferences.left = document["left"].get<std::int32_t>();
	preferences.top = document["top"].get<std::int32_t>();
	preferences.width = document["width"].get<std::int32_t>();
	preferences.height = document["height"].get<std::int32_t>();
	preferences.dpi = document["dpi"].get<std::uint32_t>();
	preferences.positioned = document["positioned"].get<bool>();
	preferences.maximized = document["maximized"].get<bool>();
	preferences.backgroundOnClose = document["backgroundOnClose"].get<bool>();

	if (preferences.width < 200 || preferences.height < 200 || preferences.dpi < 48 || preferences.dpi > 768) {
		return {};
	}

	return preferences;
}

bool PortableWindowPreferences::Save(const domain::WindowPreferences& preferences) const {
	std::error_code failure;
	std::filesystem::create_directories(_file.parent_path(), failure);
	if (failure) {
		return false;
	}

	const nlohmann::json document = {
		{"left", preferences.left},
		{"top", preferences.top},
		{"width", preferences.width},
		{"height", preferences.height},
		{"dpi", preferences.dpi},
		{"positioned", preferences.positioned},
		{"maximized", preferences.maximized},
		{"backgroundOnClose", preferences.backgroundOnClose}
	};

	std::ofstream output(_file, std::ios::binary | std::ios::trunc);
	output << document.dump();
	output.close();
	return static_cast<bool>(output);
}
}
