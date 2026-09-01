#pragma once

#include "domain/interfaces/services/ICatalogService.h"
#include "domain/interfaces/services/IInstallService.h"
#include "domain/interfaces/services/IMirrorService.h"
#include "manager/mirror/MirrorSwitch.h"

#include <chrono>
#include <filesystem>
#include <string>

namespace wgrd::manager {
class MirrorService final : public domain::IMirrorService {
public:
	static constexpr std::chrono::seconds ATTEMPT_INTERVAL{5};

	MirrorService(
		std::filesystem::path dataDirectory,
		const domain::ICatalogService& catalog,
		domain::IInstallService& install
	);

	MirrorService(const MirrorService&) = delete;

	MirrorService& operator=(const MirrorService&) = delete;

	~MirrorService() override;

	[[nodiscard]] domain::MirrorStatus Status() const override;

	void SetEnabled(bool enabled) override;

	void Poll() override;

private:
	[[nodiscard]] static bool Wanted_(const domain::CatalogRow& row);

	[[nodiscard]] std::string NextCandidate_() const;

	MirrorSwitch _preference;
	const domain::ICatalogService* _catalog;
	domain::IInstallService* _install;
	std::chrono::steady_clock::time_point _lastAttempt;
	std::size_t _claimed;
	bool _enabled;
};
}
