#pragma once

#include "domain/interfaces/services/IUpdateService.h"
#include "manager/update/GitHubReleaseSource.h"
#include "manager/update/HttpsClient.h"
#include "manager/update/VersionNumber.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace wgrd::manager {

class UpdateService final : public domain::IUpdateService {
public:
    UpdateService(std::string repository, VersionNumber current);

    UpdateService(const UpdateService&) = delete;

    UpdateService& operator=(const UpdateService&) = delete;

    ~UpdateService() override;

    [[nodiscard]] domain::UpdateStatus Status() const override;

    void Check() override;

    void Download() override;

    [[nodiscard]] bool ApplyAndRestart() override;

private:
    void RunCheck_();

    void RunDownload_();

    void JoinWorker_();

    void Publish_(domain::UpdatePhase phase, std::string message);

    HttpsClient _client;
    GitHubReleaseSource _source;
    std::string _repository;
    VersionNumber _current;

    mutable std::mutex _guard;
    domain::UpdateStatus _status;
    std::optional<ReleaseDescription> _available;

    std::thread _worker;
    std::atomic<bool> _busy;
};

}
