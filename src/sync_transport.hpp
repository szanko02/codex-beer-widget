#pragma once
#include "sync_publisher.hpp"
#include <memory>

namespace beer {
// Returns null unless the user has explicitly provisioned sync.dpapi.
std::unique_ptr<SyncPublisher> configured_sync_publisher();
} // namespace beer
