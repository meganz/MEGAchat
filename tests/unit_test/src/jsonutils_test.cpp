/**
 * @file jsonutils_test.cpp
 * @brief Tests for JSonUtils message-content parsing
 *
 * (c) 2026 by Mega Limited
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

#include <megachatapi.h>
#include <megachatapi_impl.h>
#include <memory>
#include <string>
#include <vector>

using megachat::JSonUtils;
using megachat::MegaChatAttachedUser;
using megachat::MegaChatContainsMeta;
using megachat::MegaChatMessage;

namespace
{
// A large integer whose 64-bit payload overlaps the internal array
// size/capacity field, so an array accessor used on it without a type check
// would read it as a one-element array.
constexpr char kLargeNumber[] = "4294967296";

// A syntactically valid single-entry node-attachment array.
constexpr char kValidNodeAttach[] =
    R"([{"h":"AAAAAAAAAAA","name":"file.txt","k":[1,2,3,4,5,6,7,8],"s":1024,"t":0,"ts":1600000000}])";

// A syntactically valid single-entry contact-attachment array.
constexpr char kValidContactAttach[] =
    R"([{"email":"alice@example.com","u":"AAAAAAAAAAA","name":"Alice"}])";
} // namespace

// --- parseAttachNodeJSon ---

TEST(ParseAttachNodeJSon, AcceptsValidArray)
{
    std::unique_ptr<mega::MegaNodeList> nodes{JSonUtils::parseAttachNodeJSon(kValidNodeAttach)};
    ASSERT_NE(nodes, nullptr);
    ASSERT_EQ(nodes->size(), 1);
    EXPECT_STREQ(nodes->get(0)->getName(), "file.txt");
}

TEST(ParseAttachNodeJSon, AcceptsKeyMemberFallback)
{
    constexpr char payload[] =
        R"([{"h":"AAAAAAAAAAA","name":"file.txt","key":[1,2,3,4,5,6,7,8],"s":1024,"t":0,"ts":1600000000}])";
    std::unique_ptr<mega::MegaNodeList> nodes{JSonUtils::parseAttachNodeJSon(payload)};
    ASSERT_NE(nodes, nullptr);
    ASSERT_EQ(nodes->size(), 1);
    EXPECT_STREQ(nodes->get(0)->getName(), "file.txt");
}

TEST(ParseAttachNodeJSon, RejectsLargeNumberContainer)
{
    std::unique_ptr<mega::MegaNodeList> nodes{JSonUtils::parseAttachNodeJSon(kLargeNumber)};
    EXPECT_EQ(nodes, nullptr);
}

TEST(ParseAttachNodeJSon, RejectsNonArrayContainers)
{
    for (const char* payload: {"42", "\"hello\"", "true", "null", "{}"})
    {
        std::unique_ptr<mega::MegaNodeList> nodes{JSonUtils::parseAttachNodeJSon(payload)};
        EXPECT_EQ(nodes, nullptr) << "payload: " << payload;
    }
}

TEST(ParseAttachNodeJSon, RejectsScalarArrayElement)
{
    std::unique_ptr<mega::MegaNodeList> nodes{JSonUtils::parseAttachNodeJSon("[123]")};
    EXPECT_EQ(nodes, nullptr);
}

TEST(ParseAttachNodeJSon, RejectsObjectWithoutNodeKeyMember)
{
    // Object carries "h"/"name" but neither "k" nor "key": a missing member must
    // not lead to dereferencing a past-the-end member iterator.
    constexpr char payload[] =
        R"([{"h":"AAAAAAAAAAA","name":"file.txt","s":1024,"t":0,"ts":1600000000}])";
    std::unique_ptr<mega::MegaNodeList> nodes{JSonUtils::parseAttachNodeJSon(payload)};
    EXPECT_EQ(nodes, nullptr);
}

// --- parseAttachContactJSon ---

TEST(ParseAttachContactJSon, AcceptsValidArray)
{
    std::unique_ptr<std::vector<MegaChatAttachedUser>> users{
        JSonUtils::parseAttachContactJSon(kValidContactAttach)};
    ASSERT_NE(users, nullptr);
    ASSERT_EQ(users->size(), 1u);
    EXPECT_STREQ(users->at(0).getName(), "Alice");
}

TEST(ParseAttachContactJSon, RejectsLargeNumberContainer)
{
    std::unique_ptr<std::vector<MegaChatAttachedUser>> users{
        JSonUtils::parseAttachContactJSon(kLargeNumber)};
    EXPECT_EQ(users, nullptr);
}

TEST(ParseAttachContactJSon, RejectsNonArrayContainers)
{
    for (const char* payload: {"42", "\"hello\"", "true", "null", "{}"})
    {
        std::unique_ptr<std::vector<MegaChatAttachedUser>> users{
            JSonUtils::parseAttachContactJSon(payload)};
        EXPECT_EQ(users, nullptr) << "payload: " << payload;
    }
}

TEST(ParseAttachContactJSon, RejectsScalarArrayElement)
{
    std::unique_ptr<std::vector<MegaChatAttachedUser>> users{
        JSonUtils::parseAttachContactJSon("[123]")};
    EXPECT_EQ(users, nullptr);
}

// --- parseContainsMeta ---

TEST(ParseContainsMeta, AcceptsValidTextMessage)
{
    constexpr char payload[] = R"({"textMessage":"hello world"})";
    std::unique_ptr<const MegaChatContainsMeta> meta{
        JSonUtils::parseContainsMeta(payload, MegaChatContainsMeta::CONTAINS_META_INVALID, true)};
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(std::string(meta->getTextMessage()), "hello world");
}

TEST(ParseContainsMeta, RejectsNonObjectTopLevel)
{
    for (const char* payload: {"42", "\"x\"", kLargeNumber, "[1,2,3]", "true"})
    {
        std::unique_ptr<const MegaChatContainsMeta> meta{
            JSonUtils::parseContainsMeta(payload,
                                         MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW,
                                         false)};
        ASSERT_NE(meta, nullptr) << "payload: " << payload;
        EXPECT_STREQ(meta->getTextMessage(), "") << "payload: " << payload;
        EXPECT_EQ(meta->getRichPreview(), nullptr) << "payload: " << payload;
    }
}

TEST(ParseContainsMeta, RejectsNonObjectTopLevelWithOnlyTextMessage)
{
    // Chat-list preview path passes onlyTextMessage=true; top-level guard must
    // still run before FindMember("textMessage").
    for (const char* payload: {"42", "\"x\"", kLargeNumber})
    {
        std::unique_ptr<const MegaChatContainsMeta> meta{
            JSonUtils::parseContainsMeta(payload,
                                         MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW,
                                         true)};
        ASSERT_NE(meta, nullptr) << "payload: " << payload;
        EXPECT_STREQ(meta->getTextMessage(), "") << "payload: " << payload;
    }
}

// --- parseRichPreview ---

TEST(ParseRichPreview, AcceptsValidExtraArray)
{
    constexpr char payload[] =
        R"({"textMessage":"hi","extra":[{"t":"Title","d":"Desc","url":"http://example.com"}]})";
    std::unique_ptr<const MegaChatContainsMeta> meta{
        JSonUtils::parseContainsMeta(payload,
                                     MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW,
                                     false)};
    ASSERT_NE(meta, nullptr);
    ASSERT_NE(meta->getRichPreview(), nullptr);
    EXPECT_STREQ(meta->getRichPreview()->getTitle(), "Title");
}

TEST(ParseRichPreview, RejectsNonArrayExtra)
{
    const std::string payload = std::string(R"({"textMessage":"hi","extra":)") + kLargeNumber + "}";
    std::unique_ptr<const MegaChatContainsMeta> meta{
        JSonUtils::parseContainsMeta(payload.c_str(),
                                     MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW,
                                     false)};
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->getRichPreview(), nullptr);
}

TEST(ParseRichPreview, RejectsObjectExtra)
{
    constexpr char payload[] = R"({"textMessage":"hi","extra":{"t":"Title"}})";
    std::unique_ptr<const MegaChatContainsMeta> meta{
        JSonUtils::parseContainsMeta(payload,
                                     MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW,
                                     false)};
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->getRichPreview(), nullptr);
}

TEST(ParseRichPreview, SurvivesScalarExtraElement)
{
    constexpr char payload[] = R"({"textMessage":"hi","extra":[123]})";
    std::unique_ptr<const MegaChatContainsMeta> meta{
        JSonUtils::parseContainsMeta(payload,
                                     MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW,
                                     false)};
    ASSERT_NE(meta, nullptr);
    // Non-object element: parser returns a preview with empty fields rather than
    // crashing by treating the scalar as an object.
    ASSERT_NE(meta->getRichPreview(), nullptr);
    EXPECT_STREQ(meta->getRichPreview()->getTitle(), "");
}

// --- parseGeolocation ---

TEST(ParseGeolocation, AcceptsValidExtraArray)
{
    constexpr char payload[] = R"({"textMessage":"loc","extra":[{"lng":"1.5","la":"2.5"}]})";
    std::unique_ptr<const MegaChatContainsMeta> meta{
        JSonUtils::parseContainsMeta(payload,
                                     MegaChatContainsMeta::CONTAINS_META_GEOLOCATION,
                                     false)};
    ASSERT_NE(meta, nullptr);
    ASSERT_NE(meta->getGeolocation(), nullptr);
    EXPECT_FLOAT_EQ(meta->getGeolocation()->getLongitude(), 1.5f);
    EXPECT_FLOAT_EQ(meta->getGeolocation()->getLatitude(), 2.5f);
}

TEST(ParseGeolocation, RejectsNonArrayExtra)
{
    const std::string payload =
        std::string(R"({"textMessage":"loc","extra":)") + kLargeNumber + "}";
    std::unique_ptr<const MegaChatContainsMeta> meta{
        JSonUtils::parseContainsMeta(payload.c_str(),
                                     MegaChatContainsMeta::CONTAINS_META_GEOLOCATION,
                                     false)};
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->getGeolocation(), nullptr);
}

TEST(ParseGeolocation, RejectsObjectExtra)
{
    constexpr char payload[] = R"({"textMessage":"loc","extra":{"lng":"1.5","la":"2.5"}})";
    std::unique_ptr<const MegaChatContainsMeta> meta{
        JSonUtils::parseContainsMeta(payload,
                                     MegaChatContainsMeta::CONTAINS_META_GEOLOCATION,
                                     false)};
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->getGeolocation(), nullptr);
}

TEST(ParseGeolocation, RejectsScalarExtraElement)
{
    constexpr char payload[] = R"({"textMessage":"loc","extra":[123]})";
    std::unique_ptr<const MegaChatContainsMeta> meta{
        JSonUtils::parseContainsMeta(payload,
                                     MegaChatContainsMeta::CONTAINS_META_GEOLOCATION,
                                     false)};
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->getGeolocation(), nullptr);
}

// --- parseGiphy ---

TEST(ParseGiphy, RejectsNonObjectTopLevel)
{
    for (const char* payload: {"42", "\"x\"", kLargeNumber})
    {
        std::unique_ptr<const MegaChatContainsMeta> meta{
            JSonUtils::parseContainsMeta(payload,
                                         MegaChatContainsMeta::CONTAINS_META_GIPHY,
                                         false)};
        ASSERT_NE(meta, nullptr) << "payload: " << payload;
        EXPECT_STREQ(meta->getTextMessage(), "") << "payload: " << payload;
        EXPECT_EQ(meta->getGiphy(), nullptr) << "payload: " << payload;
    }
}

// --- getLastMessageContent ---

TEST(GetLastMessageContent, NodeAttachmentPreviewHandlesMalformedJson)
{
    // Two leading bytes (0x00, subtype) precede the JSON payload.
    const std::string content = std::string("\x00\x10", 2) + kLargeNumber;
    const std::string preview =
        JSonUtils::getLastMessageContent(content, MegaChatMessage::TYPE_NODE_ATTACHMENT);
    EXPECT_TRUE(preview.empty());
}

TEST(GetLastMessageContent, ContactAttachmentPreviewHandlesMalformedJson)
{
    const std::string content = std::string("\x00\x10", 2) + "[123]";
    const std::string preview =
        JSonUtils::getLastMessageContent(content, MegaChatMessage::TYPE_CONTACT_ATTACHMENT);
    EXPECT_TRUE(preview.empty());
}

TEST(GetLastMessageContent, NodeAttachmentPreviewReturnsFilename)
{
    const std::string content = std::string("\x00\x10", 2) + kValidNodeAttach;
    const std::string preview =
        JSonUtils::getLastMessageContent(content, MegaChatMessage::TYPE_NODE_ATTACHMENT);
    EXPECT_EQ(preview, "file.txt");
}

TEST(GetLastMessageContent, ContactAttachmentPreviewReturnsName)
{
    const std::string content = std::string("\x00\x10", 2) + kValidContactAttach;
    const std::string preview =
        JSonUtils::getLastMessageContent(content, MegaChatMessage::TYPE_CONTACT_ATTACHMENT);
    EXPECT_EQ(preview, "Alice");
}
