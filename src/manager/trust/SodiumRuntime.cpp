#include "manager/trust/SodiumRuntime.h"

#include <sodium.h>

#include <mutex>

namespace wgrd::manager {

namespace {

std::once_flag INITIALISED;
bool AVAILABLE = false;

}

bool SodiumRuntime::Ready() {
    std::call_once(INITIALISED, []() {
        AVAILABLE = sodium_init() >= 0;
    });

    return AVAILABLE;
}

}
