#include "manager/update/HttpsClient.h"

#include <windows.h>
#include <winhttp.h>

#include <array>
#include <fstream>

namespace wgrd::manager {

namespace {

constexpr wchar_t USER_AGENT[] = L"wgrd-mod-manager";
constexpr std::size_t READ_BLOCK = 64 * 1024;

class SessionHandle {
public:
    explicit SessionHandle(HINTERNET handle) noexcept
        : _handle(handle) {
    }

    SessionHandle(const SessionHandle&) = delete;

    SessionHandle& operator=(const SessionHandle&) = delete;

    ~SessionHandle() {
        if (_handle != nullptr) {
            WinHttpCloseHandle(_handle);
        }
    }

    [[nodiscard]] HINTERNET Value() const noexcept {
        return _handle;
    }

    [[nodiscard]] bool Valid() const noexcept {
        return _handle != nullptr;
    }

private:
    HINTERNET _handle;
};

std::wstring Widen(std::string_view text) {
    if (text.empty()) {
        return {};
    }

    const int needed = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);

    if (needed <= 0) {
        return {};
    }

    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        wide.data(),
        needed);

    return wide;
}

}

HttpsClient::HttpsClient() = default;

std::expected<HttpsClient::Target, HttpsError> HttpsClient::CrackUrl_(std::string_view url) {
    if (!url.starts_with("https://")) {
        return std::unexpected(HttpsError::SchemeRejected);
    }

    const std::wstring wide = Widen(url);
    if (wide.empty()) {
        return std::unexpected(HttpsError::UrlRejected);
    }

    std::array<wchar_t, 256> hostBuffer{};
    std::array<wchar_t, 2048> pathBuffer{};

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.lpszHostName = hostBuffer.data();
    components.dwHostNameLength = static_cast<DWORD>(hostBuffer.size());
    components.lpszUrlPath = pathBuffer.data();
    components.dwUrlPathLength = static_cast<DWORD>(pathBuffer.size());

    if (!WinHttpCrackUrl(wide.c_str(), static_cast<DWORD>(wide.size()), 0, &components)) {
        return std::unexpected(HttpsError::UrlRejected);
    }

    if (components.nScheme != INTERNET_SCHEME_HTTPS) {
        return std::unexpected(HttpsError::SchemeRejected);
    }

    return Target{
        std::wstring(hostBuffer.data()),
        std::wstring(pathBuffer.data()),
        static_cast<std::uint16_t>(components.nPort)
    };
}

std::expected<unsigned int, HttpsError> HttpsClient::Fetch_(
    std::string_view url,
    std::string_view acceptHeader,
    std::size_t limit,
    const std::function<bool(const std::uint8_t*, std::size_t)>& sink) const {

    const auto target = CrackUrl_(url);
    if (!target.has_value()) {
        return std::unexpected(target.error());
    }

    const SessionHandle session(WinHttpOpen(
        USER_AGENT,
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0));

    if (!session.Valid()) {
        return std::unexpected(HttpsError::SessionFailed);
    }

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(
        session.Value(),
        WINHTTP_OPTION_REDIRECT_POLICY,
        &redirectPolicy,
        sizeof(redirectPolicy));

    const SessionHandle connection(WinHttpConnect(
        session.Value(),
        target->host.c_str(),
        target->port,
        0));

    if (!connection.Valid()) {
        return std::unexpected(HttpsError::ConnectFailed);
    }

    const SessionHandle request(WinHttpOpenRequest(
        connection.Value(),
        L"GET",
        target->path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE));

    if (!request.Valid()) {
        return std::unexpected(HttpsError::RequestFailed);
    }

    const std::wstring headers = acceptHeader.empty()
        ? std::wstring()
        : L"Accept: " + Widen(acceptHeader);

    const BOOL sent = WinHttpSendRequest(
        request.Value(),
        headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
        headers.empty() ? 0 : static_cast<DWORD>(headers.size()),
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0);

    if (!sent) {
        return std::unexpected(HttpsError::SendFailed);
    }

    if (!WinHttpReceiveResponse(request.Value(), nullptr)) {
        return std::unexpected(HttpsError::ResponseFailed);
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);

    if (!WinHttpQueryHeaders(
            request.Value(),
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX)) {
        return std::unexpected(HttpsError::ResponseFailed);
    }

    if (status != 200) {
        return std::unexpected(HttpsError::StatusRejected);
    }

    std::vector<std::uint8_t> block(READ_BLOCK);
    std::size_t received = 0;

    while (true) {
        DWORD read = 0;
        if (!WinHttpReadData(
                request.Value(),
                block.data(),
                static_cast<DWORD>(block.size()),
                &read)) {
            return std::unexpected(HttpsError::ReadFailed);
        }

        if (read == 0) {
            break;
        }

        received += read;
        if (received > limit) {
            return std::unexpected(HttpsError::TooLarge);
        }

        if (!sink(block.data(), read)) {
            return std::unexpected(HttpsError::WriteFailed);
        }
    }

    return status;
}

std::expected<HttpsResponse, HttpsError> HttpsClient::Get(
    std::string_view url,
    std::string_view acceptHeader,
    std::size_t limit) const {

    std::vector<std::uint8_t> body;

    const auto status = Fetch_(
        url,
        acceptHeader,
        limit,
        [&body](const std::uint8_t* data, std::size_t length) {
            body.insert(body.end(), data, data + length);
            return true;
        });

    if (!status.has_value()) {
        return std::unexpected(status.error());
    }

    return HttpsResponse{*status, std::move(body)};
}

std::expected<std::uint64_t, HttpsError> HttpsClient::Download(
    std::string_view url,
    const std::filesystem::path& target,
    std::size_t limit,
    const ProgressSink& progress) const {

    std::error_code failure;
    std::filesystem::create_directories(target.parent_path(), failure);

    std::ofstream output(target, std::ios::binary | std::ios::trunc);
    if (!output) {
        return std::unexpected(HttpsError::WriteFailed);
    }

    std::uint64_t written = 0;

    const auto status = Fetch_(
        url,
        {},
        limit,
        [&output, &written, &progress](const std::uint8_t* data, std::size_t length) {
            output.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(length));
            if (!output) {
                return false;
            }

            written += length;

            if (progress) {
                progress(written);
            }

            return true;
        });

    output.close();

    if (!status.has_value()) {
        std::filesystem::remove(target, failure);
        return std::unexpected(status.error());
    }

    return written;
}

}
