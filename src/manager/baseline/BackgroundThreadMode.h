#pragma once

namespace wgrd::manager {
class BackgroundThreadMode {
public:
	static void Enter();

	static void Leave();
};
}
