#include "manager/update/ReleaseLookupText.h"

#include "manager/text/ServiceText.h"

namespace wgrd::manager {
std::string_view ReleaseLookupText::Describe(const ReleaseLookupError failure) {
	switch (failure) {
		case ReleaseLookupError::RepositoryUnset:
			return text::LOOKUP_REPOSITORY_UNSET;
		case ReleaseLookupError::RequestFailed:
			return text::LOOKUP_REQUEST_FAILED;
		case ReleaseLookupError::ResponseMalformed:
			return text::LOOKUP_RESPONSE_MALFORMED;
		case ReleaseLookupError::TagRejected:
			return text::LOOKUP_TAG_REJECTED;
		case ReleaseLookupError::AssetMissing:
			return text::LOOKUP_ASSET_MISSING;
		case ReleaseLookupError::AssetUrlRejected:
			return text::LOOKUP_ASSET_URL_REJECTED;
	}

	return text::LOOKUP_FAILED;
}
}
