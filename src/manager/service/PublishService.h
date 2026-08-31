#pragma once

#include "domain/interfaces/content/IChunkSetTorrentBuilder.h"
#include "domain/interfaces/content/IContentChunker.h"
#include "domain/interfaces/content/IContentHasher.h"
#include "domain/interfaces/content/IPayloadPathPolicy.h"
#include "domain/interfaces/services/IPublishService.h"
#include "domain/interfaces/services/ISeedingService.h"
#include "domain/interfaces/trust/IAnnounceCatalogue.h"
#include "domain/interfaces/trust/IAnnounceReceiver.h"
#include "domain/interfaces/trust/IKeyRegistry.h"
#include "manager/manifest/ManifestBuilder.h"
#include "manager/manifest/ManifestCodec.h"
#include "manager/manifest/ManifestStore.h"
#include "manager/publish/AnnounceSigner.h"
#include "manager/publish/ManifestSigner.h"
#include "manager/publish/SigningKeyStore.h"

#include <atomic>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
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
		const domain::IChunkSetTorrentBuilder& torrentBuilder,
		domain::IAnnounceReceiver& receiver,
		const domain::IAnnounceCatalogue& catalogue,
		domain::IKeyRegistry& registry,
		domain::ISeedingService* seeding = nullptr
	);

	~PublishService() override;

	[[nodiscard]] const domain::PublisherState& Publisher() const override;

	[[nodiscard]] const std::filesystem::path& KeyPath() const override;

	[[nodiscard]] std::expected<domain::PublisherState, domain::PublishError> CreateKey(
		std::string_view publisherName,
		const std::filesystem::path& keyPath,
		std::string_view passphrase
	) override;

	[[nodiscard]] std::expected<domain::PublisherState, domain::PublishError> UnlockKey(
		const std::filesystem::path& keyPath,
		std::string_view passphrase
	) override;

	void LockKey() override;

	[[nodiscard]] const std::vector<std::string>& Candidates() const override;

	void RefreshCandidates() override;

	[[nodiscard]] std::expected<domain::PublishedRelease, domain::PublishError> Publish(
		std::string_view folder
	) override;

	void StartPublish(std::string_view folder) override;

	[[nodiscard]] domain::PublishProgress Progress() const override;

	[[nodiscard]] const std::vector<domain::PublishedRelease>& History() const override;

	[[nodiscard]] const std::string& LastMessage() const override;

	[[nodiscard]] const ManifestStore& Store() const;

private:
	[[nodiscard]] std::filesystem::path LocalRegistryFolder_() const;

	[[nodiscard]] bool TrustLocally_(const domain::PublisherIdentity& identity);

	[[nodiscard]] static std::filesystem::path SubmissionFolder_(
		const std::filesystem::path& keyPath,
		std::string_view leaf
	);

	[[nodiscard]] std::filesystem::path WriteRegistration_(
		const domain::PublisherIdentity& identity,
		const std::filesystem::path& keyPath
	);

	[[nodiscard]] std::filesystem::path WriteRevocation_(
		const domain::PublisherIdentity& identity,
		const std::filesystem::path& keyPath
	);

	[[nodiscard]] std::uint64_t NextVersion_(
		const domain::PublisherFingerprint& publisher,
		const std::string& modName
	) const;

	[[nodiscard]] static std::string Today_();

	void PublishProgress_(domain::PublishPhase phase, std::string message);

	void JoinWorker_();

	std::filesystem::path _modsDirectory;
	std::filesystem::path _dataDirectory;
	const domain::IContentHasher* _hasher;
	const domain::IChunkSetTorrentBuilder* _torrentBuilder;
	domain::IAnnounceReceiver* _receiver;
	const domain::IAnnounceCatalogue* _catalogue;
	domain::IKeyRegistry* _registry;
	domain::ISeedingService* _seeding;

	SigningKeyStore _keyStore;
	ManifestBuilder _builder;
	ManifestCodec _codec;
	ManifestSigner _signer;
	AnnounceSigner _announcer;
	ManifestStore _store;

	domain::PublisherState _publisher;
	std::filesystem::path _keyPath;
	std::vector<std::string> _candidates;
	std::vector<domain::PublishedRelease> _history;
	std::map<std::string, std::uint64_t> _versions;
	std::string _message;

	mutable std::mutex _progressGuard;
	domain::PublishProgress _progress;
	std::thread _worker;
	std::atomic<bool> _busy;
};
}
