/**
 * @file sfu_test.cpp
 * @brief Classes for test sfu.cpp
 *
 * (c) 2013-2026 by Mega Limited
 *
 * This file is part of the MEGA SDK
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#include <gtest/gtest.h>

#ifndef KARERE_DISABLE_WEBRTC

#include <rapidjson/document.h>

#include <optional>
#include <sfu.h>
#include <string>
#include <vector>

using sfu::Command;

namespace
{
// A canonical 8-byte IV encoded as 16 lowercase hex chars.
constexpr char kValidIv[] = "0123456789abcdef";
} // namespace

// --- Command::hexToBinary ---------------------------------------------------
TEST(HexToBinary, RoundTripsWithBinaryToHex)
{
    for (const uint64_t value:
         {uint64_t{0}, uint64_t{1}, uint64_t{0x1122334455667788ull}, ~uint64_t{0}})
    {
        const std::optional<uint64_t> decoded = Command::hexToBinary(Command::binaryToHex(value));
        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(*decoded, value);
    }
}

TEST(HexToBinary, DecodesUppercaseHex)
{
    const std::optional<uint64_t> decoded = Command::hexToBinary("ABCDEF0011223344");
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, Command::hexToBinary("abcdef0011223344"));
}

TEST(HexToBinary, EmptyStringIsValidAndDecodesToZero)
{
    const std::optional<uint64_t> decoded = Command::hexToBinary("");
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, 0u);
}

TEST(HexToBinary, RejectsOddLength)
{
    EXPECT_DEBUG_DEATH(
        { EXPECT_FALSE(Command::hexToBinary("abc").has_value()); },
        "invalid hex input length");
}

TEST(HexToBinary, RejectsInputLongerThanEightBytes)
{
    // 18 hex chars == 9 bytes, one past the uint64_t the result is packed into.
    EXPECT_DEBUG_DEATH(
        { EXPECT_FALSE(Command::hexToBinary("0123456789abcdef00").has_value()); },
        "invalid hex input length");
}

// --- Command::hexToByteArray ------------------------------------------------
TEST(HexToByteArray, DecodesValidHex)
{
    const std::vector<mega::byte> bytes = Command::hexToByteArray("00ff10");
    ASSERT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0x00);
    EXPECT_EQ(bytes[1], 0xff);
    EXPECT_EQ(bytes[2], 0x10);
}

TEST(HexToByteArray, EmptyStringDecodesToEmpty)
{
    EXPECT_TRUE(Command::hexToByteArray("").empty());
}

TEST(HexToByteArray, RejectsOddLength)
{
    EXPECT_DEBUG_DEATH(
        { EXPECT_TRUE(Command::hexToByteArray("abc").empty()); },
        "invalid odd hex input length");
}

// --- PeerJoinCommand IV validation ------------------------------------------
namespace
{
// Minimal SfuInterface so a PeerJoinCommand can be constructed; PeerJoinCommand
// reports its result through the completion callback, not the interface, so every
// method here is an unused stub.
class StubSfuInterface: public sfu::SfuInterface
{
public:
    bool handleAvCommand(Cid_t, unsigned, uint32_t) override
    {
        return true;
    }

    bool handleAnswerCommand(Cid_t,
                             std::shared_ptr<sfu::Sdp>,
                             uint64_t,
                             std::vector<sfu::Peer>&,
                             const std::map<Cid_t, std::string>&,
                             const std::map<Cid_t, sfu::TrackDescriptor>&,
                             const std::set<karere::Id>&,
                             const std::vector<karere::Id>&,
                             const std::vector<karere::Id>&,
                             const std::map<Cid_t, uint32_t>&) override
    {
        return true;
    }

    bool handleKeyCommand(const Keyid_t&, const Cid_t&, const std::string&) override
    {
        return true;
    }

    bool handleVThumbsCommand(const std::map<Cid_t, sfu::TrackDescriptor>&) override
    {
        return true;
    }

    bool handleVThumbsStartCommand() override
    {
        return true;
    }

    bool handleVThumbsStopCommand() override
    {
        return true;
    }

    bool handleHiResCommand(const std::map<Cid_t, sfu::TrackDescriptor>&) override
    {
        return true;
    }

    bool handleHiResStartCommand() override
    {
        return true;
    }

    bool handleHiResStopCommand() override
    {
        return true;
    }

    bool handleSpeakerAddDelCommand(const uint64_t, const bool) override
    {
        return true;
    }

    bool handleSpeakReqAddDelCommand(const uint64_t, const bool) override
    {
        return true;
    }

    bool handleRaiseHandAddCommand(const uint64_t) override
    {
        return true;
    }

    bool handleRaiseHandDelCommand(const uint64_t) override
    {
        return true;
    }

    bool handleModAdd(uint64_t) override
    {
        return true;
    }

    bool handleModDel(uint64_t) override
    {
        return true;
    }

    bool handleHello(const Cid_t,
                     const unsigned int,
                     const std::set<karere::Id>&,
                     const bool,
                     const bool,
                     bool,
                     const sfu::WrUserList&,
                     const CallLimits&) override
    {
        return true;
    }

    bool handleWrDump(const sfu::WrUserList&) override
    {
        return true;
    }

    bool handleWrEnter(const sfu::WrUserList&) override
    {
        return true;
    }

    bool handleWrLeave(const karere::Id&) override
    {
        return true;
    }

    bool handleWrAllow(const Cid_t&) override
    {
        return true;
    }

    bool handleWrDeny() override
    {
        return true;
    }

    bool handleWrUsersAllow(const std::set<karere::Id>&) override
    {
        return true;
    }

    bool handleWrUsersDeny(const std::set<karere::Id>&) override
    {
        return true;
    }

    bool handleMutedCommand(const unsigned, const Cid_t) override
    {
        return true;
    }

    bool handleWillEndCommand(const unsigned int) override
    {
        return true;
    }

    bool handleClimitsCommand(const sfu::SfuInterface::CallLimits&) override
    {
        return true;
    }

    bool handlePeerJoin(Cid_t,
                        uint64_t,
                        sfu::SfuProtocol,
                        int,
                        std::string&,
                        std::vector<std::string>&) override
    {
        return true;
    }

    bool handlePeerLeft(Cid_t, unsigned) override
    {
        return true;
    }

    bool handleBye(const unsigned, const bool, const std::string&) override
    {
        return true;
    }

    void onSfuDisconnected() override {}

    void onByeCommandSent() override {}

    bool error(unsigned int, const std::string&) override
    {
        return true;
    }

    bool processDeny(const std::string&, const std::string&) override
    {
        return true;
    }

    void logError(const char*) override {}
};

// Builds a well-formed PEERJOIN document carrying the supplied ivs and runs it
// through PeerJoinCommand::processCommand. Returns the ivs the command forwarded
// to its completion callback after validation.
std::vector<std::string> runPeerJoinWithIvs(const rapidjson::Value& ivsArray)
{
    rapidjson::Document command;
    command.SetObject();
    auto& alloc = command.GetAllocator();
    command.AddMember("cid", 1u, alloc);
    command.AddMember("userId", "AAAAAAAAAAA", alloc);
    command.AddMember("av", 0u, alloc);
    command.AddMember("v", 1u, alloc);
    rapidjson::Value ivsCopy;
    ivsCopy.CopyFrom(ivsArray, alloc);
    command.AddMember("ivs", ivsCopy, alloc);

    std::vector<std::string> forwardedIvs;
    StubSfuInterface stub;
    sfu::PeerJoinCommand cmd(
        [&forwardedIvs](Cid_t,
                        uint64_t,
                        sfu::SfuProtocol,
                        int,
                        std::string&,
                        std::vector<std::string>& ivs)
        {
            forwardedIvs = ivs;
            return true;
        },
        stub);
    cmd.processCommand(command);
    return forwardedIvs;
}

rapidjson::Value makeStringArray(const std::vector<std::string>& items,
                                 rapidjson::Document::AllocatorType& alloc)
{
    rapidjson::Value arr(rapidjson::kArrayType);
    for (const std::string& item: items)
    {
        rapidjson::Value v;
        v.SetString(item.c_str(), static_cast<rapidjson::SizeType>(item.size()), alloc);
        arr.PushBack(v, alloc);
    }
    return arr;
}
} // namespace

TEST(PeerJoinIvValidation, AcceptsAllValidIvs)
{
    rapidjson::Document doc;
    rapidjson::Value ivs = makeStringArray({kValidIv, kValidIv, kValidIv}, doc.GetAllocator());
    const std::vector<std::string> forwarded = runPeerJoinWithIvs(ivs);
    EXPECT_EQ(forwarded, (std::vector<std::string>{kValidIv, kValidIv, kValidIv}));
}

TEST(PeerJoinIvValidation, RejectsIvWithWrongLength)
{
    rapidjson::Document doc;
    rapidjson::Value ivs =
        makeStringArray({kValidIv, kValidIv, "0123456789abcde"}, doc.GetAllocator());
    EXPECT_TRUE(runPeerJoinWithIvs(ivs).empty());
}

TEST(PeerJoinIvValidation, RejectsIvWithNonHexCharacter)
{
    rapidjson::Document doc;
    rapidjson::Value ivs = makeStringArray({"0123456789abcdeg"}, doc.GetAllocator());
    EXPECT_TRUE(runPeerJoinWithIvs(ivs).empty());
}

TEST(PeerJoinIvValidation, RejectsNonStringIvElement)
{
    rapidjson::Document doc;
    auto& alloc = doc.GetAllocator();
    rapidjson::Value ivs(rapidjson::kArrayType);
    ivs.PushBack(rapidjson::Value().SetString(kValidIv, alloc), alloc);
    ivs.PushBack(rapidjson::Value(42), alloc); // not a string -> whole set rejected
    EXPECT_TRUE(runPeerJoinWithIvs(ivs).empty());
}

TEST(SfuParseData, RejectsNonObjectTopLevelFrame)
{
    constexpr const char* invalidTopLevelFrames[] = {
        "42",
        "\"x\"",
        "[]",
        "true",
    };

    for (auto* frame: invalidTopLevelFrames)
    {
        rapidjson::Document document;
        sfu::SfuConnection::SfuData outdata;
        EXPECT_FALSE(sfu::SfuConnection::parseSfuData(frame, document, outdata))
            << "[FAILED rejecting non-object SFU frame]: " << frame;
        EXPECT_EQ(outdata.msg, "top-level not an object")
            << "[FAILED rejecting non-object SFU frame]: " << frame;
    }
}

TEST(SdpJsonTracks, ParsesSingleValidTrack)
{
    rapidjson::Document doc;
    doc.Parse(R"({"tracks":[{"t":"v","mid":3,"dir":"sendrecv"}]})");
    ASSERT_FALSE(doc.HasParseError());
    sfu::Sdp sdp(doc);
    ASSERT_EQ(sdp.tracks().size(), 1u);
    EXPECT_EQ(sdp.tracks()[0].mType, "v");
    EXPECT_EQ(sdp.tracks()[0].mMid, 3u);
    EXPECT_EQ(sdp.tracks()[0].mDir, "sendrecv");
}

TEST(SdpJsonTracks, DefaultsMidWhenMissing)
{
    rapidjson::Document doc;
    doc.Parse(R"({"tracks":[{"t":"v","dir":"sendrecv"}]})");
    ASSERT_FALSE(doc.HasParseError());
    sfu::Sdp sdp(doc);
    ASSERT_EQ(sdp.tracks().size(), 1u);
    EXPECT_EQ(sdp.tracks()[0].mType, "v");
    EXPECT_EQ(sdp.tracks()[0].mMid, 0u);
    EXPECT_EQ(sdp.tracks()[0].mDir, "sendrecv");
}

TEST(SdpJsonTracks, SkipsTrailingNonObjectTrackElement)
{
    rapidjson::Document doc;
    doc.Parse(R"({"tracks":[{"t":"v","mid":3,"dir":"sendrecv"},42]})");
    ASSERT_FALSE(doc.HasParseError());
    sfu::Sdp sdp(doc);
    ASSERT_EQ(sdp.tracks().size(), 1u);
    EXPECT_EQ(sdp.tracks()[0].mType, "v");
    EXPECT_EQ(sdp.tracks()[0].mMid, 3u);
    EXPECT_EQ(sdp.tracks()[0].mDir, "sendrecv");
}

TEST(SdpJsonTracks, ParsesMultipleValidTracks)
{
    rapidjson::Document doc;
    doc.Parse(
        R"({"tracks":[{"t":"a","mid":1,"dir":"sendrecv"},{"t":"v","mid":2,"dir":"sendonly"}]})");
    ASSERT_FALSE(doc.HasParseError());
    sfu::Sdp sdp(doc);
    ASSERT_EQ(sdp.tracks().size(), 2u);
    EXPECT_EQ(sdp.tracks()[0].mType, "a");
    EXPECT_EQ(sdp.tracks()[1].mType, "v");
}

TEST(SdpJsonTracks, SkipsOnlyNonObjectTrackElements)
{
    rapidjson::Document doc;
    doc.Parse(R"({"tracks":[42,"x",[]]})");
    ASSERT_FALSE(doc.HasParseError());
    sfu::Sdp sdp(doc);
    EXPECT_TRUE(sdp.tracks().empty());
}

TEST(SdpJsonTracks, SkipsNonObjectTrackBetweenValidTracks)
{
    rapidjson::Document doc;
    doc.Parse(
        R"({"tracks":[{"t":"a","mid":1,"dir":"sendrecv"},"x",{"t":"v","mid":2,"dir":"recvonly"}]})");
    ASSERT_FALSE(doc.HasParseError());
    sfu::Sdp sdp(doc);
    ASSERT_EQ(sdp.tracks().size(), 2u);
    EXPECT_EQ(sdp.tracks()[0].mType, "a");
    EXPECT_EQ(sdp.tracks()[0].mMid, 1u);
    EXPECT_EQ(sdp.tracks()[1].mType, "v");
    EXPECT_EQ(sdp.tracks()[1].mMid, 2u);
    EXPECT_EQ(sdp.tracks()[1].mDir, "recvonly");
}

#endif // KARERE_DISABLE_WEBRTC
