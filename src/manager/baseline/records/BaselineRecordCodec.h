#pragma once

#include "manager/baseline/records/BaselineFileRecord.h"

#include <optional>
#include <string>
#include <string_view>

namespace wgrd::manager {
class BaselineRecordCodec {
public:
	static constexpr std::size_t CHUNK_LIMIT = 1u << 16;
	static constexpr std::size_t DOCUMENT_LIMIT = 16u * 1024u * 1024u;

	[[nodiscard]] static std::string Encode(const BaselineFileRecord& record);

	[[nodiscard]] static std::optional<BaselineFileRecord> Decode(std::string_view document);
};
}
