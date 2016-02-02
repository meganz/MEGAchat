#include "contactList.h"
#include <mstrophepp.h>
#include <type_traits>
#include "busConstants.h"
#include "karereCommon.h"
#include <mega/base64.h>
#include <chatClient.h>

namespace karere
{
const char* Presence::sStrings[Presence::kLast+1] =
{"unavailable", "busy", "away", "available", "chatty"};

void XmppContact::onPresence(Presence pres, const std::string& fullJid)
{
    auto resource = strophe::getResourceFromJid(fullJid);
    auto it = mResources.find(resource);
    if (it == mResources.end())
        it = mResources.emplace(resource, pres).first;
    else
        it->second = pres;
    mPresence = calculatePresence();
    if (mPresListener)
        mPresListener->onPresence(mPresence);
}
void XmppContact::onOffline()
{
    mPresence = Presence::kOffline;
    if (mPresListener)
        mPresListener->onPresence(Presence::kOffline);
}

void XmppContactList::notifyOffline()
{
    for (auto& item: *this)
        item.second->onOffline();
}

Presence XmppContact::calculatePresence()
{
    Presence max(Presence::kOffline);
    for (auto& res: mResources)
    {
        if (res.second > max)
            max = res.second;
    }
    return max;
}

void XmppContactList::receivePresences()
{
    if (mHandler)
    {
        mReadyPromise = promise::Promise<void>();
    }
    mHandler = mClient.conn->addHandler([this](strophe::Stanza presence, void*, bool& keep) mutable
    {
        const char* jid = presence.attrOrNull("from");
        if(!jid)
        {
            KR_LOG_WARNING("received presence stanza without 'from' attribute");
            return;
        }
        if (strstr(jid, "@conference."))
            return; //we are not interested in chatroom presences
        if (strcmp(jid, mClient.conn->fullJid()) == 0) //our own presence, this is the end of the presence list
        {
            if (!mReadyPromise.done())
                mReadyPromise.resolve();
            return;
        }
        Presence status(presence);
        auto bareJid = strophe::getBareJidFromJid(jid);
        auto it = find(bareJid);
        if (it == end())
        {
            KR_LOG_DEBUG("Received presence for unknown XMPP contact '%s', creating contact", bareJid.c_str());
            addContact(jid, status);
        }
        else
        {
            it->second->onPresence(status, jid);
        }
    }, nullptr, "presence", nullptr, nullptr);
}

XmppContactList::~XmppContactList()
 { if (mHandler) mClient.conn->removeHandler(mHandler); }

bool XmppContactList::addContact(const std::string& fullJid, Presence presence, std::string bareJid)
{
    if (bareJid.empty())
        bareJid = strophe::getBareJidFromJid(fullJid);
    auto it = find(bareJid);
    if (it != end())
        return false;
    emplace(bareJid, std::make_shared<XmppContact>(presence, fullJid, true));
    return true;
}

std::shared_ptr<XmppContact> XmppContactList::addContact(Contact& contact)
{
    uint64_t uid = contact.userId();
    auto bareJid = useridToJid(uid);
    auto it = find(bareJid);
    if (it != end())
    {
        auto xmppContact = it->second;
        xmppContact->setPresenceListener(&contact);
        auto pres = xmppContact->presence();
        if (pres != Presence::kOffline)
            contact.onPresence(pres);
        return xmppContact;
    }
    else
    {
        auto xmppContact = std::make_shared<XmppContact>(Presence::kOffline, bareJid, false, &contact);
        emplace(bareJid, xmppContact);
        return xmppContact;
    }
}

XmppContact& XmppContactList::getContact(const std::string& bareJid) const
{
    auto it = find(bareJid);
    if (it == end())
        throw std::runtime_error("XMppContactList::getContact: unknown jid "+bareJid);
    return *it->second;
}

promise::Promise<void> XmppContactList::fetch()
{
    strophe::Stanza roster(*mClient.conn);
    roster.setName("iq")
          .setAttr("type", "get")
          .setAttr("from", mClient.conn->fullJid())
          .c("query")
              .setAttr("xmlns", "jabber:iq:roster");
    return mClient.conn->sendIqQuery(roster, "roster")
    .then([this](strophe::Stanza s) mutable
    {
        auto query = s.child("query", true);
        if (query)
        {
            query.forEachChild("item", [this](strophe::Stanza c)
            {
                const char* jid = c.attrOrNull("jid");
                if (!jid)
                    return;

                addContact(jid, Presence::kOffline);
            });
        }
        receivePresences();
        return mReadyPromise;
    })
    .fail([](const promise::Error& err)
    {
        KR_LOG_WARNING("Error receiving contact list");
        return err;
    });
}

}
