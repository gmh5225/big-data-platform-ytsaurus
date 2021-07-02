#include "remote.h"

#include <yt/yt/core/misc/protobuf_helpers.h>

namespace NYT::NProfiling {

////////////////////////////////////////////////////////////////////////////////

void ToProto(NProto::TSummaryDouble* proto, const TSummarySnapshot<double>& summary)
{
    proto->set_sum(summary.Sum());
    proto->set_min(summary.Min());
    proto->set_max(summary.Max());
    proto->set_last(summary.Last());
    proto->set_count(summary.Count());
}

void FromProto(TSummarySnapshot<double>* summary, const NProto::TSummaryDouble& proto)
{
    *summary = TSummarySnapshot<double>(
        proto.sum(),
        proto.min(),
        proto.max(),
        proto.last(),
        proto.count()
    );
}

void ToProto(NProto::TSummaryDuration* proto, const TSummarySnapshot<TDuration>& summary)
{
    proto->set_sum(summary.Sum().GetValue());
    proto->set_min(summary.Min().GetValue());
    proto->set_max(summary.Max().GetValue());
    proto->set_last(summary.Last().GetValue());
    proto->set_count(summary.Count());
}

void FromProto(TSummarySnapshot<TDuration>* summary, const NProto::TSummaryDuration& proto)
{
    *summary = TSummarySnapshot<TDuration>(
        TDuration::FromValue(proto.sum()),
        TDuration::FromValue(proto.min()),
        TDuration::FromValue(proto.max()),
        TDuration::FromValue(proto.last()),
        proto.count()
    );
}

void ToProto(NProto::THistogramSnapshot* proto, const THistogramSnapshot& histogram)
{
    for (auto time : histogram.Times) {
        proto->add_times(time.GetValue());        
    }
    for (auto value : histogram.Values) {
        proto->add_values(value);
    }
}

void FromProto(THistogramSnapshot* histogram, const NProto::THistogramSnapshot& proto)
{
    histogram->Values.clear();
    histogram->Times.clear();

    for (auto time : proto.times()) {
        histogram->Times.push_back(TDuration::FromValue(time));
    }

    for (auto value : proto.values()) {
        histogram->Values.push_back(value);
    }
}

////////////////////////////////////////////////////////////////////////////////

TRemoteRegistry::TRemoteRegistry(TSolomonRegistry* registry)
    : Registry_(registry)
{
    TagRename_.emplace_back();
}

void TRemoteRegistry::Transfer(const NProto::TSensorDump& dump)
{
    for (const auto& cube : dump.cubes()) {
        for (const auto& projection : cube.projections()) {
            for (const auto& tagId : projection.tag_ids()) {
                if (tagId <= 0 || tagId > dump.tags().size()) {
                    THROW_ERROR_EXCEPTION("Incorrect tag")
                        << TErrorAttribute("tag_id", tagId);
                }
            }
        }
    }

    for (TTagId tagId = TagRename_.size(); tagId < dump.tags().size(); tagId++) {
        const auto& remoteTag = dump.tags()[tagId];
        TagRename_.push_back(Registry_->Tags_.Encode(TTag{remoteTag.key(), remoteTag.value()}));
    }

    auto oldSensors = std::move(Sensors_);
    Sensors_ = {};

    for (const auto& cube : dump.cubes()) {
        TSensorOptions options;
        options.Sparse = cube.sparse();
        options.Global = cube.global();
        options.DisableSensorsRename = cube.disable_sensors_rename();
        options.DisableDefault = cube.disable_default();

        auto sensorName = cube.name();
        auto sensorSet = Registry_->FindSet(cube.name(), options);
        auto& usedTags = Sensors_[cube.name()];

        for (const auto& projection : cube.projections()) {
            TTagIdList tagIds;
            for (const auto& tagId : projection.tag_ids()) {
                tagIds.push_back(tagId);
            }
            tagIds = RenameTags(tagIds);

            auto transferValue = [&] (auto cube, ESensorType type, auto value) {
                sensorSet->InitializeType(type);

                bool inserted = usedTags.UsedTags.emplace(type, tagIds).second;
                if (inserted) {
                    cube->Add(tagIds);
                }

                if (projection.has_value()) {
                    cube->Update(tagIds, value);
                }
            };

            if (projection.has_counter()) {
                transferValue(&sensorSet->CountersCube_, ESensorType::Counter, projection.counter());
            } else if (projection.has_duration()) {
                transferValue(&sensorSet->TimeCountersCube_, ESensorType::TimeCounter, TDuration::FromValue(projection.duration()));
            } else if (projection.has_gauge()) {
                transferValue(&sensorSet->GaugesCube_, ESensorType::Gauge, projection.gauge());
            } else if (projection.has_summary()) {
                transferValue(&sensorSet->SummariesCube_, ESensorType::Summary, NYT::FromProto<TSummarySnapshot<double>>(projection.summary()));
            } else if (projection.has_timer()) {
                transferValue(&sensorSet->TimersCube_, ESensorType::Timer, NYT::FromProto<TSummarySnapshot<TDuration>>(projection.timer()));
            } else if (projection.has_histogram()) {
                transferValue(&sensorSet->HistogramsCube_, ESensorType::Histogram, NYT::FromProto<THistogramSnapshot>(projection.histogram()));
            } else {
                // Ignore unknown types.
            }      
        }
    }

    DoDetach(oldSensors);
}

void TRemoteRegistry::Detach()
{
    DoDetach(Sensors_);
}

void TRemoteRegistry::DoDetach(const THashMap<TString, TRemoteSensorSet>& sensors)
{
    for (const auto& [name, usedTags] : sensors) {
        auto& sensorSet = Registry_->Sensors_.find(name)->second;

        for (const auto& [type, tags] : usedTags.UsedTags) {
            switch (type) {
            case ESensorType::Counter:
                sensorSet.CountersCube_.Remove(tags);
                break;
            case ESensorType::TimeCounter:
                sensorSet.TimeCountersCube_.Remove(tags);
                break;
            case ESensorType::Gauge:
                sensorSet.GaugesCube_.Remove(tags);
                break;
            case ESensorType::Summary:
                sensorSet.SummariesCube_.Remove(tags);
                break;
            case ESensorType::Timer:
                sensorSet.TimersCube_.Remove(tags);
                break;
            case ESensorType::Histogram:
                sensorSet.HistogramsCube_.Remove(tags);
                break;
            default:
                YT_VERIFY(false);
            }
        }
    }
}

TTagIdList TRemoteRegistry::RenameTags(const TTagIdList& tags)
{
    TTagIdList renamed;
    for (auto tag : tags) {
        renamed.push_back(TagRename_[tag]);
    }
    return renamed;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NProfiling