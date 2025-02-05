#pragma once

#include "block_cache.h"

#include <yt/yt/ytlib/misc/public.h>

namespace NYT::NChunkClient {

////////////////////////////////////////////////////////////////////////////////

struct TBlockCacheEntry
{
    TBlockId BlockId;
    EBlockType BlockType;
    TCachedBlock Block;
};

////////////////////////////////////////////////////////////////////////////////

struct IClientBlockCache
    : public IBlockCache
{
    virtual void Reconfigure(const TBlockCacheDynamicConfigPtr& config) = 0;
};

DEFINE_REFCOUNTED_TYPE(IClientBlockCache)

////////////////////////////////////////////////////////////////////////////////

//! Creates a simple client-side block cache.
IClientBlockCachePtr CreateClientBlockCache(
    TBlockCacheConfigPtr config,
    EBlockType supportedBlockTypes,
    IMemoryUsageTrackerPtr memoryTracker = nullptr,
    INodeMemoryReferenceTrackerPtr memoryReferenceTracker = nullptr,
    const NProfiling::TProfiler& profiler = {});

//! Returns an always-empty block cache.
IBlockCachePtr GetNullBlockCache();

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NChunkClient
