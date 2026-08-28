#pragma once

#include "domain/interfaces/order/IOrderInspector.h"

namespace wgrd::manager {
class OrderInspector final : public domain::IOrderInspector {
public:
	OrderInspector() = default;
	~OrderInspector() override = default;

	[[nodiscard]] std::vector<domain::Annotation> Inspect(
		const domain::LoadOrder& order,
		std::span<const domain::InstalledMod> installed
	) const override;

private:
	static bool IsInstalled_(
		std::span<const domain::InstalledMod> installed,
		const domain::InstallFolder& folder
	) noexcept;
};
}
