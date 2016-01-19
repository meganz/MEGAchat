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
{"unavailable", "dnd", "away", "available", "chatty"};

void XmppContact::onPresence(Presence pres, const std::string& fullJid)
{
    auto resource = strophe::getResourceFromJid(fullJid);
    auto it = mResources.find(resource);
    if (it == mResources.end())
        it = mResources.emplace(resource, pres).first;
    else
        it->second = pres;
    calculatePresence();
}

void XmppContact::calculatePresence()
{
    Presence max(Presence::kOffline);
    for (auto& res: mResources)
    {
        if (res.second > max)
            max = res.second;
    }
    if (max == mPresence)
        return;
    mPresence = max;
    if (mPresListener)
        mPresListener->onPresence(mPresence);
}

promise::Promise<void> XmppContactList::receivePresences()
{
    promise::Promise<void> pms;
    mHandler = mClient.conn->addHandler([this, pms](strophe::Stanza presence, void*, bool& keep) mutable
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
            if (!pms.done())
                pms.resolve();
            return;
        }
        Presence status(presence);
        auto bareJid = strophe::getBareJidFromJid(jid);
        auto it = find(bareJid);
        if (it == end())
        {
            addContact(jid, status);
        }
        else
        {
            it->second->onPresence(status, jid);
        }
    }, nullptr, "presence", nullptr, nullptr);
    return pms;
}
XmppContactList::~XmppContactList()
 { if (mHandler) mClient.conn->removeHandler(mHandler); }

bool XmppContactList::addContact(const std::string& fullJid, Presence presence, std::string bareJid)
{
    if (bareJid.empty())
        bareJid = strophe::getBareJidFromJid(fullJid);

    auto res = emplace(bareJid, std::make_shared<XmppContact>(presence, fullJid));
    return res.second;
}

std::shared_ptr<XmppContact> XmppContactList::addContact(Contact& contact)
{
    char buf[32];
    uint64_t uid = contact.userId();
    ::mega::Base32::btoa((byte*)&uid, sizeof(uid), buf);
    std::string bareJid = std::string(buf)+"@"+KARERE_XMPP_DOMAIN;
    auto it = find(bareJid);
    if (it != end())
        return it->second;
    auto xmppContact = emplace(bareJid, std::make_shared<XmppContact>(Presence::kOffline)).first->second;
    xmppContact->mPresListener = &contact;
    return xmppContact;
}

XmppContact& XmppContactList::getContact(const std::string& bareJid) const
{
    auto it = find(bareJid);
    if (it == end())
        throw std::runtime_error("XMppContactList::getContact: unknown jid "+bareJid);
    return *it->second;
}

promise::Promise<void> XmppContactList::init()
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
        return receivePresences();
    })
    .fail([](const promise::Error& err)
    {
        KR_LOG_WARNING("Error receiving contact list");
        return err;
    });
}

}
