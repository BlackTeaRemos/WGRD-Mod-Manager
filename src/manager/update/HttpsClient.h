#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::manager {
enum class HttpsError {
	SchemeRejected
	, UrlRejected
	, SessionFailed
	, ConnectFailed
	, RequestFailed
	, SendFailed
	, ResponseFailed
	, StatusRejected
	, TooLarge
	, ReadFailed
	, WriteFailed
};

struct HttpsResponse {
	unsigned int status;
	std::vector<std::uint8_t> body;
};

class HttpsClient {
public:
	static constexpr std::size_t METADATA_LIMIT = 1024 * 1024;
	static constexpr std::size_t PAYLOAD_LIMIT = 128 * 1024 * 1024;

	HttpsClient();

	[[nodiscard]] std::expected<HttpsResponse, HttpsError> Get(
		std::string_view url,
		std::string_view acceptHeader,
		std::size_t limit
	) const;

	using ProgressSink = std::function<void(std::uint64_t)>;

	[[nodiscard]] std::expected<std::uint64_t, HttpsError> Download(
		std::string_view url,
		const std::filesystem::path& target,
		std::size_t limit,
		const ProgressSink& progress = {}
	) const;

private:
	struct Target {
		std::wstring host;
		std::wstring path;
		std::uint16_t port;
	};

	[[nodiscard]] static std::expected<Target, HttpsError> CrackUrl_(std::string_view url);

	[[nodiscard]] std::expected<unsigned int, HttpsError> Fetch_(
		std::string_view url,
		std::string_view acceptHeader,
		std::size_t limit,
		const std::function<bool(const std::uint8_t*, std::size_t)>& sink
	) const;
};
}
