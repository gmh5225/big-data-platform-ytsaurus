#pragma once

#include "public.h"

#include <yt/yt/library/profiling/sensor.h>

#include <yt/yt/library/syncmap/map.h>

#include <yt/yt/core/misc/atomic_object.h>

#include <yt/yt/core/ytree/yson_struct.h>

namespace NYT::NTracing {

////////////////////////////////////////////////////////////////////////////////

class TSamplerConfig
    : public NYTree::TYsonStruct
{
public:
    //! Request is sampled with probability P.
    double GlobalSampleRate;

    //! Additionally, request is sampled with probability P(user).
    THashMap<TString, double> UserSampleRate;

    //! Spans are sent to specified endpoint.
    THashMap<TString, TString> UserEndpoint;

    //! Additionally, sample first N requests for each user in the window.
    ui64 MinPerUserSamples;
    TDuration MinPerUserSamplesPeriod;

    //! Clear sampled from from incoming user request.
    THashMap<TString, bool> ClearSampledFlag;

    REGISTER_YSON_STRUCT(TSamplerConfig);

    static void Register(TRegistrar registrar);
};

DEFINE_REFCOUNTED_TYPE(TSamplerConfig)

////////////////////////////////////////////////////////////////////////////////

class TSampler
    : public TRefCounted
{
public:
    TSampler();

    explicit TSampler(
        const TSamplerConfigPtr& config);

    void SampleTraceContext(const TString& user, const TTraceContextPtr& traceContext);

    void UpdateConfig(const TSamplerConfigPtr& config);

private:
    TAtomicObject<TSamplerConfigPtr> Config_;

    struct TUserState final
    {
        std::atomic<ui64> Sampled = {0};
        std::atomic<NProfiling::TCpuInstant> LastReset = {0};

        bool TrySampleByMinCount(ui64 minCount, NProfiling::TCpuDuration period);

        NProfiling::TCounter TracesSampledByUser;
        NProfiling::TCounter TracesSampledByProbability;
    };

    NConcurrency::TSyncMap<TString, TIntrusivePtr<TUserState>> Users_;
    NProfiling::TCounter TracesSampled_;
};

DEFINE_REFCOUNTED_TYPE(TSampler)

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NTracing
