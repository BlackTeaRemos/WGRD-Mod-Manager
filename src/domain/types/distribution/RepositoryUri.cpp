#include "domain/types/distribution/RepositoryUri.h"

namespace wgrd::domain {
std::string_view RepositoryUri::Slug(std::string_view repository) {
	if (repository.starts_with(SCHEME)) {
		repository = repository.substr(SCHEME.size());
	}

	const std::size_t host = repository.find(HOST);
	if (host != std::string_view::npos) {
		repository = repository.substr(host + HOST.size());
	}

	while (repository.starts_with('/')) {
		repository = repository.substr(1);
	}

	while (repository.ends_with('/')) {
		repository = repository.substr(0, repository.size() - 1);
	}

	return repository;
}

std::string RepositoryUri::Https(const std::string_view repository) {
	const std::string_view slug = Slug(repository);

	if (slug.empty()) {
		return {};
	}

	return std::string(SCHEME) + std::string(HOST) + std::string(slug);
}
}
