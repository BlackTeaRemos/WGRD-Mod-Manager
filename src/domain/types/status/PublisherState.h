#pragma once

#include <string>

namespace wgrd::domain {

struct PublisherState {
    bool present;
    std::string fingerprint;
    std::string name;

    bool operator==(const PublisherState& other) const = default;
};

}
