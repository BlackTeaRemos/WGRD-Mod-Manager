#pragma once

#include "domain/interfaces/trust/IAnnounceReceiver.h"
#include "domain/interfaces/content/IChunkSetTorrentBuilder.h"
#include "domain/interfaces/content/IContentChunker.h"
#include "domain/interfaces/content/IContentHasher.h"
#include "domain/interfaces/trust/IDataProtection.h"
#include "domain/interfaces/content/IPayloadPathPolicy.h"
#include "domain/interfaces/services/IPublishService.h"
#include "manager/manifest/ManifestBuilder.h"
#include "manager/manifest/ManifestCodec.h"
#include "manager/manifest/ManifestStore.h"
#include "manager/publish/AnnounceSigner.h"
#include "manager/publish/ManifestSigner.h"
#include "manager/publish/PublisherKeyExporter.h"
#include "manager/publish/SigningKeyStore.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::manager {

class PublishService final : public domain::IPublishService {
public:
    PublishService(
        std::filesystem::path modsDirectory,
        std::filesystem::path dataDirectory,
        const domain::IContentChunker& chunker,
        const domain::IContentHasher& hasher,
        const domain::IPayloadPathPolicy& pathPolicy,
        const domain::IDataProtection& protection,
        const domain::IChunkSetTorrentBuilder& torrentBuilder,
        domain::IAnnounceReceiver& receiver);

    ~PublishService() override;

    [[nodiscard]] const domain::PublisherState& Publisher() const override;

    [[nodiscard]] std::expected<domain::PublisherState, domain::PublishError> CreateKey(
        std::string_view publisherName) override;

    [[nodiscard]] const std::vector<std::string>& Candidates() const override;

    void RefreshCandidates() override;

    [[nodiscard]] std::expected<domain::PublishedRelease, domain::PublishError> Publish(
        std::string_view folder) override;

    [[nodiscard]] const std::vector<domain::PublishedRelease>& History() const override;

    [[nodiscard]] const std::string& LastMessage() const override;

    [[nodiscard]] const ManifestStore& Store() const;

private:
    void RefreshPublisher_();

    [[nodiscard]] std::uint64_t NextVersion_(const std::string& identifier) const;

    [[nodiscard]] static std::string Today_();

    std::filesystem::path _modsDirectory;
    std::filesystem::path _dataDirectory;
    const domain::IContentHasher* _hasher;
    const domain::IChunkSetTorrentBuilder* _torrentBuilder;
    domain::IAnnounceReceiver* _receiver;

    SigningKeyStore _keyStore;
    PublisherKeyExporter _exporter;
    ManifestBuilder _builder;
    ManifestCodec _codec;
    ManifestSigner _signer;
    AnnounceSigner _announcer;
    ManifestStore _store;

    domain::PublisherState _publisher;
    std::vector<std::string> _candidates;
    std::vector<domain::PublishedRelease> _history;
    std::map<std::string, std::uint64_t> _versions;
    std::string _message;
};

}
