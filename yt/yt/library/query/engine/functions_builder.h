#pragma once

#include <yt/yt/library/query/base/functions_builtin_types.h>

#include <yt/yt/library/query/engine/functions_builtin_profilers.h>

namespace NYT::NQueryClient {

////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<IFunctionRegistryBuilder> CreateFunctionRegistryBuilder(
    const TTypeInferrerMapPtr& typeInferrers,
    const TFunctionProfilerMapPtr& functionProfilers,
    const TAggregateProfilerMapPtr& aggregateProfilers);

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NQueryClient
