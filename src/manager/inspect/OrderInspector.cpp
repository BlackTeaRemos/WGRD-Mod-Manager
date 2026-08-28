#include "manager/inspect/OrderInspector.h"

namespace wgrd::manager {

std::vector<domain::Annotation> OrderInspector::Inspect(
    const domain::LoadOrder& order,
    std::span<const domain::InstalledMod> installed) const {

    std::vector<domain::Annotation> annotations;

    for (const domain::OrderEntry& entry : order.Entries()) {
        if (!entry.enabled) {
            continue;
        }

        if (!IsInstalled_(installed, entry.folder)) {
            annotations.push_back(domain::Annotation{
                entry.folder,
                domain::AnnotationCategory::FolderAbsent,
                domain::AnnotationSeverity::Blocking,
                "missing",
                "folder not present"
            });
        }
    }

    return annotations;
}

bool OrderInspector::IsInstalled_(
    std::span<const domain::InstalledMod> installed,
    const domain::InstallFolder& folder) noexcept {

    for (const domain::InstalledMod& candidate : installed) {
        if (candidate.folder == folder) {
            return true;
        }
    }

    return false;
}

}
