#include "manager/baseline/BackgroundThreadMode.h"

#include <windows.h>

namespace wgrd::manager {
void BackgroundThreadMode::Enter() {
	SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
}

void BackgroundThreadMode::Leave() {
	SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
}
}
