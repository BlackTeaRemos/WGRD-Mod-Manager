#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace wgrd::domain {
enum class RemovalError {
	UnknownIdentifier
	, NotInstalled
	, FolderRejected
	, FolderLocked
	, Busy
};

class IModRemovalService {
public:
	virtual ~IModRemovalService() = 0;

	[[nodiscard]] virtual std::expected<void, RemovalError> Remove(std::string_view identifier) = 0;

	[[nodiscard]] virtual const std::string& LastMessage() const = 0;
};

inline IModRemovalService::~IModRemovalService() = default;
}
