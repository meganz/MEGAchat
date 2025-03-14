#include <rtcModule/rtcStats.h>
#include <math.h>
#include <webrtcAdapter.h>

#include <iostream>

namespace  rtcModule
{

void QualityLimitationReport::addIncident(const std::string& reason)
{
    const std::pair<std::string, EReason>* it;
    if (reason == "") // empty reason is treated as EReason::NONE ("none")
    {
        it = std::find_if(mStrReasonMap.begin(),
                          mStrReasonMap.end(),
                          [](const auto& p)
                          {
                              return p.second == EReason::NONE;
                          });
        if (it == mStrReasonMap.end())
        {
            std::cerr << "qualityLimitationReason: EReason::NONE is not in the mStrReasonMap\n";
            assert(false); // Fatal: NONE must be in mStrReasonMap
            return;
        }
    }
    else
    {
        it = std::find_if(mStrReasonMap.begin(),
                          mStrReasonMap.end(),
                          [&reason](const auto& p)
                          {
                              return p.first == reason;
                          });
        if (it == mStrReasonMap.end())
        {
            std::cerr << "qualityLimitationReason: unexpected reason: " << reason << "\n";
            assert(false); // Not contemplated reason
            return;
        }
    }
    size_t index = static_cast<size_t>(it - mStrReasonMap.begin());
    ++mIncidentCounter[index];
}

void QualityLimitationReport::toJson(rapidjson::Value& value, rapidjson::Document& json)
{
    for (uint32_t i = 0; i < NUMBER_OF_REASONS; ++i)
    {
        rapidjson::Value pair(rapidjson::kArrayType);
        pair.PushBack(static_cast<uint32_t>(mStrReasonMap[i].second), json.GetAllocator());
        pair.PushBack(mIncidentCounter[i], json.GetAllocator());
        value.PushBack(pair.Move(), json.GetAllocator());
    }
}

void ConnStatsCallBack::removeStats()
{
    mStats = nullptr;
}

std::string Stats::statsErrToString(const callstats_bs_t& e)
{
    if (e.none())
    {
        return "Stats Ok";
    }

    std::string err;
    if (e[kStatsProtoErr])
    {
        err += "Invalid protocol version | ";
    }
    if (e[kStatsMyPeerErr])
    {
        err += "Invalid PeerId | ";
    }
    if (e[kStatsCidErr])
    {
        err += "Invalid Cid | ";
    }
    if (e[kStatsDurErr])
    {
        err += "Invalid call duration | ";
    }
    if (e[kStatsDeviceErr])
    {
        err += "Invalid device Id | ";
    }
    if (e[kStatsSfuHostErr])
    {
        err += "Invalid SFU host | ";
    }
    if (e[kStatsCallIdErr])
    {
        err += "Invalid CallId | ";
    }

    if (err.size() > 3)
    {
        err.erase(err.length() - 3);
    }
    return err;
}

Stats::callstats_bs_t Stats::validateStatsInfo() const
{
    callstats_bs_t bs;
    if (mSfuProtoVersion != static_cast<uint32_t>(sfu::SfuProtocol::SFU_PROTO_PROD))
    {
        bs[Stats::kStatsProtoErr] = true;
    }

    if (!mPeerId.isValid())
    {
        bs[Stats::kStatsMyPeerErr] = true;
    }

    if (!mCid)
    {
        bs[Stats::kStatsCidErr] = true;
    }

    if (!mDuration)
    {
        bs[Stats::kStatsDurErr] = true;
    }

    if (mDevice.empty())
    {
        bs[Stats::kStatsDeviceErr] = true;
    }

    if (mSfuHost.empty())
    {
        bs[Stats::kStatsSfuHostErr] = true;
    }

    if (!mCallid.isValid())
    {
        bs[Stats::kStatsCallIdErr] = true;
    }

    return bs;
}

std::pair<Stats::callstats_bs_t, std::string> Stats::getJson()
{
    const auto statsValidation = validateStatsInfo();
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value sfuv(rapidjson::kNumberType);
    sfuv.SetUint(mSfuProtoVersion);
    json.AddMember("v", sfuv, json.GetAllocator());
    rapidjson::Value userid(rapidjson::kStringType);
    userid.SetString(mPeerId.toString().c_str(), json.GetAllocator());
    json.AddMember("userid", userid, json.GetAllocator());
    mCid // if we have not still joined SFU, send kUnassignedCid as CID in SFU stats
        ? json.AddMember("cid", mCid, json.GetAllocator())
        : json.AddMember("cid", kUnassignedCid, json.GetAllocator());
    rapidjson::Value callid(rapidjson::kStringType);
    callid.SetString(mCallid.toString().c_str(), json.GetAllocator());
    json.AddMember("callid", callid, json.GetAllocator());
    json.AddMember("toffs", mTimeOffset, json.GetAllocator()); // must be in milliseconds
    json.AddMember("dur", mDuration, json.GetAllocator());
    rapidjson::Value device(rapidjson::kStringType);
    device.SetString(mDevice.c_str(), json.GetAllocator());
    json.AddMember("ua", device, json.GetAllocator());

    if (!mSamples.mT.empty())
    {
        rapidjson::Value samples(rapidjson::kObjectType);

        std::vector<float> periods;
        for (unsigned int i = 1; i < mSamples.mT.size(); i++)
        {
            periods.push_back(static_cast<float>((mSamples.mT[i] - mSamples.mT[i - 1])/1000.0));
        }

        rapidjson::Value t(rapidjson::kArrayType);
        parseSamples(mSamples.mT, t, json, false);
        samples.AddMember("t", t, json.GetAllocator());

        rapidjson::Value q(rapidjson::kArrayType);
        parseSamples(mSamples.mQ, q, json, false);
        samples.AddMember("q", q, json.GetAllocator());

        rapidjson::Value pl(rapidjson::kArrayType);
        parseSamples(mSamples.mPacketLost, pl, json, true, &periods);
        samples.AddMember("pl", pl, json.GetAllocator());

        rapidjson::Value rtt(rapidjson::kArrayType);
        parseSamples(mSamples.mRoundTripTime, rtt, json, false);
        samples.AddMember("rtt", rtt, json.GetAllocator());

        rapidjson::Value txBwe(rapidjson::kArrayType);
        parseSamples(mSamples.mOutGoingBitrate, txBwe, json, false);
        samples.AddMember("txBwe", txBwe, json.GetAllocator());

        rapidjson::Value rx(rapidjson::kArrayType);
        parseSamples(mSamples.mBytesReceived, rx, json, true, &periods);
        samples.AddMember("rx", rx, json.GetAllocator());

        rapidjson::Value tx(rapidjson::kArrayType);
        parseSamples(mSamples.mBytesSend, tx, json, true, &periods);
        samples.AddMember("tx", tx, json.GetAllocator());

        rapidjson::Value av(rapidjson::kArrayType);
        parseSamples(mSamples.mAv, av, json, false);
        samples.AddMember("av", av, json.GetAllocator());

        rapidjson::Value nrxh(rapidjson::kArrayType);
        parseSamples(mSamples.mNrxh, nrxh, json, false);
        samples.AddMember("nrxh", nrxh, json.GetAllocator());

        rapidjson::Value nrxl(rapidjson::kArrayType);
        parseSamples(mSamples.mNrxl, nrxl, json, false);
        samples.AddMember("nrxl", nrxl, json.GetAllocator());

        rapidjson::Value nrxa(rapidjson::kArrayType);
        parseSamples(mSamples.mNrxa, nrxa, json, false);
        samples.AddMember("nrxa", nrxa, json.GetAllocator());

        rapidjson::Value vtxfps(rapidjson::kArrayType);
        parseSamples(mSamples.mVtxHiResfps, vtxfps, json, false);
        samples.AddMember("vtxfps", vtxfps, json.GetAllocator());

        rapidjson::Value vtxw(rapidjson::kArrayType);
        parseSamples(mSamples.mVtxHiResw, vtxw, json, false);
        samples.AddMember("vtxw", vtxw, json.GetAllocator());

        rapidjson::Value vtxh(rapidjson::kArrayType);
        parseSamples(mSamples.mVtxHiResh, vtxh, json, false);
        samples.AddMember("vtxh", vtxh, json.GetAllocator());

        rapidjson::Value audioJitter(rapidjson::kArrayType);
        parseSamples(mSamples.mAudioJitter, audioJitter, json, false);
        samples.AddMember("jtr", audioJitter, json.GetAllocator());

        rapidjson::Value qualityLimitationReason(rapidjson::kArrayType);
        mSamples.mQualityLimitations.toJson(qualityLimitationReason, json);
        samples.AddMember("f", qualityLimitationReason, json.GetAllocator());

        json.AddMember("samples", samples, json.GetAllocator());
    }

    json.AddMember("trsn", mTermCode, json.GetAllocator());
    json.AddMember("grp", static_cast<int>(mIsGroup), json.GetAllocator());
    rapidjson::Value sfuHost(rapidjson::kStringType);
    sfuHost.SetString(mSfuHost.c_str(), json.GetAllocator());
    json.AddMember("sfu", sfuHost, json.GetAllocator());
    json.AddMember("peers", mMaxPeers, json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string jsonStats(buffer.GetString(), buffer.GetSize());
    return std::make_pair(statsValidation, jsonStats);
}

void Stats::clear()
{
    mPeerId = karere::Id::inval();
    mCid = 0;
    mCallid = karere::Id::inval();
    mTimeOffset = 0;
    mDuration = 0;
    mMaxPeers = 0;
    mTermCode = 0;
    mInitialTs = 0;
    mIsGroup = false;
    mDevice.clear();
    mSfuHost.clear();
    mSamples.mT.clear();
    mSamples.mPacketLost.clear();
    mSamples.mRoundTripTime.clear();
    mSamples.mOutGoingBitrate.clear();
    mSamples.mBytesReceived.clear();
    mSamples.mBytesSend.clear();
    mSamples.mPacketSent.clear();
    mSamples.mTotalPacketSendDelay.clear();
    mSamples.mAudioJitter.clear();
    mSamples.mQ.clear();
    mSamples.mAv.clear();
    mSamples.mNrxh.clear();
    mSamples.mNrxl.clear();
    mSamples.mNrxa.clear();
    mSamples.mVtxLowResfps.clear();
    mSamples.mVtxLowResw.clear();
    mSamples.mVtxLowResh.clear();
    mSamples.mVtxHiResfps.clear();
    mSamples.mVtxHiResw.clear();
    mSamples.mVtxHiResh.clear();
    mSamples.mQualityLimitations.clear();
}

bool Stats::isEmptyStats()
{
    return mPeerId == karere::Id::inval();
}

void Stats::parseSamples(const std::vector<int32_t> &samples, rapidjson::Value &value, rapidjson::Document& json, bool diff, const std::vector<float> *periods)
{
    std::vector<int32_t> datas;
    if (diff)
    {
        for (unsigned int i = 1; i < samples.size(); i++)
        {
            datas.push_back(samples[i] - samples[i - 1]);
        }

        datas.insert(datas.begin(), 0);
    }
    else
    {
        datas = samples;
    }

    if (datas.empty())
    {
        // Force to add empty brackets
        value.PushBack(0, json.GetAllocator());
        value.Erase(value.Begin());
        return;
    }

    int32_t lastValue = datas[0];
    unsigned int lastIndex = 0;
    for (unsigned int i = 1; i < datas.size(); i++)
    {
        int32_t data = datas[i];

        if (periods)
        {
            if (i < periods->size())
            {
                data = static_cast<int32_t>(static_cast<float>(data) / periods->at(i));
            }
            else
            {
                data = static_cast<int32_t>(static_cast<float>(data) / periods->back());
            }
        }

        if (lastValue == data && i < (datas.size() - 1))
        {
            continue;
        }

        int numDuplicatedValues = i - lastIndex;
        if (numDuplicatedValues < 2)
        {
            value.PushBack(lastValue, json.GetAllocator());
        }
        else
        {
            rapidjson::Value pair(rapidjson::kArrayType);
            pair.PushBack(lastValue, json.GetAllocator());
            pair.PushBack(numDuplicatedValues, json.GetAllocator());
            value.PushBack(pair, json.GetAllocator());
        }

        lastIndex = i;
        lastValue = datas[i];
    }
}

ConnStatsCallBack::ConnStatsCallBack(Stats *stats, uint32_t hiResId, uint32_t lowResId, void* appCtx)
    : mStats(stats)
    , mHiResId(hiResId)
    , mLowResId(lowResId)
    , mAppCtx(appCtx)
{
}

ConnStatsCallBack::~ConnStatsCallBack()
{

}

void ConnStatsCallBack::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport> &report)
{
    auto wptr = weakHandle();
    karere::marshallCall([wptr, this, report]()
    {
        if (wptr.deleted() || !mStats)
        {
            return;
        }

        mStats->mSamples.mRoundTripTime.push_back(0.0);
        mStats->mSamples.mOutGoingBitrate.push_back(0.0);
        mStats->mSamples.mBytesReceived.push_back(0);
        mStats->mSamples.mBytesSend.push_back(0);
        mStats->mSamples.mPacketLost.push_back(0);
        mStats->mSamples.mVtxHiResh.push_back(0);
        mStats->mSamples.mVtxHiResfps.push_back(0);
        mStats->mSamples.mVtxHiResw.push_back(0);
        mStats->mSamples.mVtxLowResh.push_back(0);
        mStats->mSamples.mVtxLowResfps.push_back(0);
        mStats->mSamples.mVtxLowResw.push_back(0);
        mStats->mSamples.mAudioJitter.push_back(0);
        mStats->mSamples.mPacketSent.push_back(0);
        mStats->mSamples.mTotalPacketSendDelay.push_back(0);

        if (mStats->mInitialTs == 0)
        {
            mStats->mInitialTs = report->timestamp().us();
        }

        mStats->mSamples.mT.push_back(
            static_cast<int32_t>((report->timestamp().us() - mStats->mInitialTs) / 1000));

        for (auto it = report->begin(); it != report->end(); it++)
        {
            if (strcmp(it->type(), "candidate-pair") == 0)
            {
                double rtt = 0.0;
                double txBwe = 0.0;
                int64_t bytesRecv = 0;
                int64_t bytesSend = 0;
                getConnStats(it, rtt, txBwe, bytesRecv, bytesSend);
                mStats->mSamples.mRoundTripTime.back() += static_cast<int32_t>(std::lround(rtt));
                mStats->mSamples.mOutGoingBitrate.back() += static_cast<int32_t>(std::lround(txBwe));
                mStats->mSamples.mBytesReceived.back() += static_cast<int32_t>(bytesRecv);
                mStats->mSamples.mBytesSend.back() += static_cast<int32_t>(bytesSend);
            }
            else if (strcmp(it->type(), "inbound-rtp") == 0)
            {
                std::string kind;
                int32_t audioJitter = 0;
                std::vector<webrtc::Attribute> attributes = it->Attributes();
                for (const webrtc::Attribute& attribute: attributes)
                {
                    if (!attribute.has_value())
                    {
                        continue;
                    }

                    if (strcmp(attribute.name(), "packetsLost") == 0)
                    {
                        int32_t packetLost = attribute.get<int32_t>();
                        mStats->mSamples.mPacketLost.back() = mStats->mSamples.mPacketLost.back() + packetLost;
                    }

                    if (strcmp(attribute.name(), "jitter") == 0)
                    {
                        double value = attribute.get<double>();
                        audioJitter = static_cast<int32_t>(round(value * 1000.0));
                    }

                    if (strcmp(attribute.name(), "kind") == 0)
                    {
                        kind = attribute.get<std::string>();
                    }
                }

                // we only take care of lowest value higher than 0
                if (kind == "audio" && (!mStats->mSamples.mAudioJitter.back() || (mStats->mSamples.mAudioJitter.back() > audioJitter && audioJitter > 0)))
                {
                    mStats->mSamples.mAudioJitter.back() = audioJitter;
                }
            }
            else if (strcmp(it->type(), "outbound-rtp") == 0)
            {
                uint32_t width = 0;
                uint32_t height = 0;
                double fps = 0;
                uint32_t ssrc = 0;
                std::vector<webrtc::Attribute> attributes = it->Attributes();
                for (const webrtc::Attribute& attribute: attributes)
                {
                    if (!attribute.has_value())
                    {
                        continue;
                    }

                    if (strcmp(attribute.name(), "frameWidth") == 0)
                    {
                        width = attribute.get<uint32_t>();
                    }
                    else if (strcmp(attribute.name(), "frameHeight") == 0)
                    {
                        height = attribute.get<uint32_t>();
                    }
                    else if (strcmp(attribute.name(), "framesPerSecond") == 0)
                    {
                        fps = attribute.get<double>();
                    }
                    else if (strcmp(attribute.name(), "ssrc") == 0)
                    {
                        ssrc = attribute.get<uint32_t>();
                    }
                    else if (strcmp(attribute.name(), "packetsSent") == 0)
                    {
                        uint64_t packetSent = attribute.get<uint64_t>();
                        mStats->mSamples.mPacketSent.back() = static_cast<uint32_t>(packetSent);
                    }
                    else if (strcmp(attribute.name(), "totalPacketSendDelay") == 0)
                    {
                        double totalPacketSendDelay = attribute.get<double>();
                        mStats->mSamples.mTotalPacketSendDelay.back() = totalPacketSendDelay;
                    }
                    else if (strcmp(attribute.name(), "qualityLimitationReason") == 0)
                    {
                        std::string limitationReason = attribute.get<std::string>();
                        mStats->mSamples.mQualityLimitations.addIncident(limitationReason);
                    }
                }

                if (ssrc == mHiResId && mHiResId)
                {
                    mStats->mSamples.mVtxHiResh.back() = height;
                    mStats->mSamples.mVtxHiResfps.back() = static_cast<int32_t>(fps);
                    mStats->mSamples.mVtxHiResw.back() = width;
                }
                else if (ssrc == mLowResId && mLowResId)
                {
                    mStats->mSamples.mVtxLowResh.back() = height;
                    mStats->mSamples.mVtxLowResfps.back() = static_cast<int32_t>(fps);
                    mStats->mSamples.mVtxLowResw.back() = width;
                }
            }
        }
    }, mAppCtx);
}

void ConnStatsCallBack::getConnStats(const webrtc::RTCStatsReport::ConstIterator& it, double& rtt, double& txBwe, int64_t& bytesRecv, int64_t& bytesSend)
{
    std::vector<webrtc::Attribute> attributes = it->Attributes();
    for (const webrtc::Attribute& attribute: attributes)
    {
        if (!attribute.has_value())
        {
            continue;
        }
        if (strcmp(attribute.name(), "currentRoundTripTime") == 0)
        {
            rtt = attribute.get<double>() * 1000;
        }
        else if (strcmp(attribute.name(), "availableOutgoingBitrate") == 0)
        {
            txBwe = round(attribute.get<double>() / 1024.0);
        }
        else if (strcmp(attribute.name(), "bytesReceived") == 0)
        {
            bytesRecv = attribute.get<uint64_t>() / 128;
        }
        else if (strcmp(attribute.name(), "bytesSent") == 0)
        {
            bytesSend = attribute.get<uint64_t>() / 128;
        }
    }
}

}
