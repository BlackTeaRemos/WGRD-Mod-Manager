#pragma once

#include "domain/types/order/InstallFolder.h"

#include <string>

namespace wgrd::domain {
enum class AnnotationCategory {
	FolderAbsent
};

enum class AnnotationSeverity {
	Advisory, Blocking
};

struct Annotation {
	InstallFolder folder;
	AnnotationCategory category;
	AnnotationSeverity severity;
	std::string tag;
	std::string explanation;
};
}
