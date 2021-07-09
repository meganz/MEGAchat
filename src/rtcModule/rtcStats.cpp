#include <rtcModule/rtcStats.h>
#include <math.h>

#include <iostream>

namespace  rtcModule
{

RtcStatCallback::RtcStatCallback(Stats *stats)
    : mStats(stats)
{

}

void RtcStatCallback::removeStats()
{
    mStats = nullptr;
}

void RtcStatCallback::AddRef() const
{
    mRefCount.IncRef();
}

rtc::RefCountReleaseStatus RtcStatCallback::Release() const
{
    const auto status = mRefCount.DecRef();
    if (status == rtc::RefCountReleaseStatus::kDroppedLastRef)
    {
        delete this;
    }

    return status;
}

LocalVideoStatsCallBack::LocalVideoStatsCallBack(Stats* stats, bool hiRes)
    : RtcStatCallback(stats)
    , mHiRes(hiRes)
{
}

void LocalVideoStatsCallBack::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport> &report)
{
    if (!mStats)
    {
        return;
    }

    int64_t ts = 0;
    for (auto it = report->begin(); it != report->end(); it++)
    {
        if (strcmp(it->type(), "outbound-rtp") == 0)
        {
            std::vector<const webrtc::RTCStatsMemberInterface*>members = it->Members();
            ts = it->timestamp_us();
            for (const webrtc::RTCStatsMemberInterface* member : members)
            {
                if (strcmp(member->name(), "frameWidth") == 0)
                {
                    uint32_t width = *member->cast_to<const webrtc::RTCStatsMember<uint32_t>>();
                    if (mStats->mSamples.mT.size() > mStats->mSamples.mVtxLowResw.size())
                    {
                        if (mHiRes)
                        {
                            mStats->mSamples.mVtxHiResw.push_back(width);
                        }
                        else
                        {
                            mStats->mSamples.mVtxLowResw.push_back(width);
                        }
                    }
                }
                else if (strcmp(member->name(), "frameHeight") == 0)
                {
                    uint32_t height = *member->cast_to<const webrtc::RTCStatsMember<uint32_t>>();
                    if (mStats->mSamples.mT.size() > mStats->mSamples.mVtxLowResh.size())
                    {
                        if (mHiRes)
                        {
                            mStats->mSamples.mVtxHiResh.push_back(height);
                        }
                        else
                        {
                            mStats->mSamples.mVtxLowResh.push_back(height);
                        }
                    }
                }
                else if (strcmp(member->name(), "framesPerSecond") == 0)
                {
                    double fps = *member->cast_to<const webrtc::RTCStatsMember<double>>();
                    if (mStats->mSamples.mT.size() > mStats->mSamples.mVtxLowResfps.size())
                    {
                        if (mHiRes)
                        {
                            mStats->mSamples.mVtxHiResfps.push_back(static_cast<int32_t>(fps));
                        }
                        else
                        {
                            mStats->mSamples.mVtxLowResfps.push_back(static_cast<int32_t>(fps));
                        }
                    }
                }
            }
        }
    }
}

RemoteVideoStatsCallBack::RemoteVideoStatsCallBack(Stats *stats)
    : RtcStatCallback(stats)
{

}

RemoteVideoStatsCallBack::~RemoteVideoStatsCallBack()
{

}

void RemoteVideoStatsCallBack::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport> &report)
{
    if (!mStats)
    {
        return;
    }

    std::string trackId;
    int64_t ts = 0;
    uint32_t packetLost = 0;
    uint32_t nackCount = 0;
    uint32_t bytesReceived = 0;
    uint32_t keyFramesDecoded = 0;
    for (auto it = report->begin(); it != report->end(); it++)
    {
        if (strcmp(it->type(), "track") == 0)
        {
            trackId = it->id();
        }
        else if (strcmp(it->type(), "inbound-rtp") == 0)
        {
            std::vector<const webrtc::RTCStatsMemberInterface*>members = it->Members();
            ts = it->timestamp_us();
            for (const webrtc::RTCStatsMemberInterface* member : members)
            {
                if (strcmp(member->name(), "packetsLost") == 0)
                {
                    packetLost = *member->cast_to<const webrtc::RTCStatsMember<int32_t>>();
                    mStats->mSamples.mPacketLost.back() = mStats->mSamples.mPacketLost.back() + packetLost;
                }
                else if (strcmp(member->name(), "nackCount") == 0)
                {
                    nackCount = *member->cast_to<const webrtc::RTCStatsMember<uint32_t>>();
                }
                else if (strcmp(member->name(), "bytesReceived") == 0)
                {
                    bytesReceived = *member->cast_to<const webrtc::RTCStatsMember<uint64_t>>();
                }
                else if (strcmp(member->name(), "keyFramesDecoded") == 0)
                {
                    keyFramesDecoded = *member->cast_to<const webrtc::RTCStatsMember<uint32_t>>();
                }
            }
        }
    }
}

std::string Stats::getJson()
{
    rapidjson::Document json(rapidjson::kObjectType);

    rapidjson::Value userid(rapidjson::kStringType);
    userid.SetString(mPeerId.toString().c_str(), json.GetAllocator());
    json.AddMember("userid", userid, json.GetAllocator());
    json.AddMember("cid", mCid, json.GetAllocator());
    rapidjson::Value callid(rapidjson::kStringType);
    callid.SetString(mCallid.toString().c_str(), json.GetAllocator());
    json.AddMember("callid", callid, json.GetAllocator());
    json.AddMember("toffs", mTimeOffset, json.GetAllocator());
    json.AddMember("dur", mDuration, json.GetAllocator());
    rapidjson::Value device(rapidjson::kStringType);
    device.SetString(mDevice.c_str(), json.GetAllocator());
    json.AddMember("ua", device, json.GetAllocator());

    rapidjson::Value samples(rapidjson::kObjectType);

    std::vector<int32_t> periods;
    for (unsigned int i = 1; i < mSamples.mT.size(); i++)
    {
        periods.push_back(mSamples.mT[i] - mSamples.mT[i - 1]);
    }

    rapidjson::Value t(rapidjson::kArrayType);
    parseSamples(mSamples.mT, t, json, false);
    samples.AddMember("t", t, json.GetAllocator());

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
    parseSamples(mSamples.mVtxLowResfps, vtxfps, json, false);
    samples.AddMember("vtxfps", vtxfps, json.GetAllocator());

    rapidjson::Value vtxw(rapidjson::kArrayType);
    parseSamples(mSamples.mVtxLowResw, vtxw, json, false);
    samples.AddMember("vtxw", vtxw, json.GetAllocator());

    rapidjson::Value vtxh(rapidjson::kArrayType);
    parseSamples(mSamples.mVtxLowResh, vtxh, json, false);
    samples.AddMember("vtxh", vtxh, json.GetAllocator());

    json.AddMember("samples", samples, json.GetAllocator());
    json.AddMember("trsn", mTerCode, json.GetAllocator());
    json.AddMember("grp", static_cast<int>(mIsGroup), json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string jsonStats(buffer.GetString(), buffer.GetSize());
    return jsonStats;
}

void Stats::clear()
{
    mPeerId = karere::Id::inval();
    mCid = 0;
    mCallid = karere::Id::inval();
    mTimeOffset = 0;
    mDuration = 0;
    mSamples.mT.clear();
    mSamples.mPacketLost.clear();
    mSamples.mRoundTripTime.clear();
    mSamples.mOutGoingBitrate.clear();
    mSamples.mBytesReceived.clear();
    mSamples.mBytesSend.clear();
    mSamples.mQ.clear();
    mSamples.mAv.clear();
    mSamples.mNrxh.clear();
    mSamples.mNrxl.clear();
    mSamples.mNrxa.clear();
    mSamples.mVtxLowResfps.clear();
    mSamples.mVtxLowResw.clear();
    mSamples.mVtxLowResh.clear();
    mTerCode = 0;
    mIsGroup = false;
    mInitialTs = 0;
    mDevice.clear();
}

void Stats::parseSamples(const std::vector<int32_t> &samples, rapidjson::Value &value, rapidjson::Document& json, bool diff, const std::vector<int32_t> *periods)
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
                data = data / periods->at(i);
            }
            else
            {
                data = data / periods->back();
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

ConnStatsCallBack::ConnStatsCallBack(Stats *stats)
    : RtcStatCallback(stats)
{

}

ConnStatsCallBack::~ConnStatsCallBack()
{

}

void ConnStatsCallBack::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport> &report)
{
    if (!mStats)
    {
        return;
    }

    mStats->mSamples.mRoundTripTime.push_back(0.0);
    mStats->mSamples.mOutGoingBitrate.push_back(0.0);
    mStats->mSamples.mBytesReceived.push_back(0);
    mStats->mSamples.mBytesSend.push_back(0);

    if (mStats->mInitialTs == 0)
    {
        mStats->mInitialTs = report->timestamp_us();
    }

    mStats->mSamples.mT.push_back((report->timestamp_us() - mStats->mInitialTs)/ 1000);

    for (auto it = report->begin(); it != report->end(); it++)
    {
        if (strcmp(it->type(), "candidate-pair") == 0)
        {
            double rtt = 0.0;
            double txBwe = 0.0;
            int64_t bytesRecv = 0;
            int64_t bytesSend = 0;
            getConnStats(it, rtt, txBwe, bytesRecv, bytesSend);
            mStats->mSamples.mRoundTripTime.back() += rtt;
            mStats->mSamples.mOutGoingBitrate.back() += txBwe;
            mStats->mSamples.mBytesReceived.back() += bytesRecv;
            mStats->mSamples.mBytesSend.back() += bytesSend;
        }
    }
}

void ConnStatsCallBack::getConnStats(const webrtc::RTCStatsReport::ConstIterator& it, double& rtt, double txBwe, int64_t& bytesRecv, int64_t& bytesSend)
{
    std::vector<const webrtc::RTCStatsMemberInterface*>members = it->Members();
    for (const webrtc::RTCStatsMemberInterface* member : members)
    {
        if (strcmp(member->name(), "currentRoundTripTime") == 0)
        {
            rtt = *member->cast_to<const webrtc::RTCStatsMember<double>>() * 1000;
        }
        else if (strcmp(member->name(), "availableOutgoingBitrate") == 0)
        {
            txBwe = round(static_cast<double>(*member->cast_to<const webrtc::RTCStatsMember<double>>()) / 128.0);
        }
        else if (strcmp(member->name(), "bytesReceived") == 0)
        {
           bytesRecv = *member->cast_to<const webrtc::RTCStatsMember<uint64_t>>() / 128;
        }
        else if (strcmp(member->name(), "bytesSent") == 0)
        {
            bytesSend = *member->cast_to<const webrtc::RTCStatsMember<uint64_t>>() / 128;
        }
    }
}

}
