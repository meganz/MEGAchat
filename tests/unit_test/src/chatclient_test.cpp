/**
 * @file chatclient_test.cpp
 * @brief Tests for chatClient.cpp
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

#include <base/promise.h>
#include <base/trackDelete.h>
#include <gtest/gtest.h>

namespace
{

struct FakeContact: public karere::DeleteTrackable
{
    int mChatRoomId = 7;
};

promise::Promise<int> createChatRoomGuarded(FakeContact* contact, promise::Promise<int>& apiReply)
{
    auto wptr = contact->weakHandle();
    return apiReply.then(
        [contact, wptr](int) -> promise::Promise<int>
        {
            if (wptr.deleted())
                return ::promise::Error("Contact was deleted while creating the chatroom");
            return contact->mChatRoomId;
        });
}

} // namespace

TEST(ContactCreateChatRoomUseAfterFree, GuardedContinuationSkipsFreedContact)
{
    auto* contact = new FakeContact;
    promise::Promise<int> apiReply;
    promise::Promise<int> resultPms = createChatRoomGuarded(contact, apiReply);

    // Simulates ContactList::syncWithApi's "delete contact"
    // landing before the delayed createChat API reply.
    delete contact;

    // Simulates the createChat API reply arriving afterwards. Must not
    // touch the freed contact (would abort under AddressSanitizer).
    apiReply.resolve(1);

    ASSERT_TRUE(resultPms.done());
    EXPECT_TRUE(resultPms.failed());
    EXPECT_STREQ(resultPms.error().what(), "Contact was deleted while creating the chatroom");
}
