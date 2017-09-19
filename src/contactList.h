#ifndef CONTACTLIST_H
#define CONTACTLIST_H
#include <mstrophepp.h>
#include <type_traits>
#include "karereCommon.h"
#include <mega/base64.h>

namespace karere
{
class Contact;

class XmppResource: public std::string
{
protected:
    Presence mPresence;
    friend class XmppContact;
public:
    const std::string& resource() const { return *this; }
    Presence presence() const { return mPresence; }
    XmppResource(const std::string& aResource, Presence aPresence)
        :std::string(aResource), mPresence(aPresence){}
};
class IPresenceListener
{
public:
    virtual void onPresence(Presence pres) = 0;
    virtual ~IPresenceListener(){}
};

class XmppContact
{
protected:
    /*contact's bare JID*/
    std::string mBareJid;
    /*constact's presence state*/
    Presence mPresence;
    typedef std::map<std::string, Presence> ResourceMap;
    ResourceMap mResources;
    IPresenceListener* mPresListener;
    Presence calculatePresence();
    void onOffline();
    friend class XmppContactList;
public:
    XmppContact(Presence pre, const std::string& jid, bool isFullJid, IPresenceListener* listener=nullptr)
    :mPresence(pre), mPresListener(listener)
    {
        if (isFullJid)
        {
            mBareJid = strophe::getBareJidFromJid(jid);
            auto resource = strophe::getResourceFromJid(jid);
            mResources.emplace(resource, pre);
        }
        else
        {
            mBareJid = jid;
        }
    }
    const std::string& bareJid() const { return mBareJid; }
    Presence presence() const { return mPresence; }
    const ResourceMap& resources() const { return mResources; }
    void onPresence(Presence pres, const std::string& fullJid);
    void setPresenceListener(IPresenceListener* listener) { mPresListener = listener; }
};

class XmppContactList: protected std::map<std::string, std::shared_ptr<XmppContact>>
{
protected:
    typedef std::map<std::string, std::shared_ptr<XmppContact>> Base;
    Client& mClient;
    xmpp_uid mHandler = 0;
    void receivePresences();
    promise::Promise<void> mReadyPromise;
public:
    promise::Promise<void>& ready() { return mReadyPromise; }
    XmppContactList(Client& client): mClient(client){}
    ~XmppContactList();

    /** @brief initialize the contactlist to handle the presence messages from contacts. */
    promise::Promise<void> fetch();
    void notifyOffline();
    bool addContact(const std::string& fullJid, Presence pres, std::string bareJid="");
    std::shared_ptr<XmppContact> addContact(Contact& contact);
    XmppContact& getContact(const std::string& bareJid) const;
    using Base::operator[];
    using Base::size;
    static Presence presenceFromStanza(strophe::Stanza pres);
};
inline const char* Presence::toString()
{
    unsigned char tmpStatus = status();
    if (tmpStatus == kOffline)
        return "unavailable";
    else if (tmpStatus == kOnline)
        return "available";
    else if (tmpStatus == kAway)
        return "away";
    else if (tmpStatus == kBusy)
        return "dnd";
    else if (tmpStatus == kInvalid)
        return "invalid";
    else
        throw std::runtime_error("Presence::toString: Unknown presence "+std::to_string(mPres));
}

inline unsigned char Presence::fromString(const char* text)
{
    if (!text)
        throw std::runtime_error("Presence(const char*): Null text provided");
    if (!strcmp(text, "unavailable"))
        return kOffline;
    else if (!strcmp(text, "chat") || !strcmp(text, "available"))
        return kOnline;
    else if (!strcmp(text, "away"))
        return kAway;
    else if (!strcmp(text, "dnd"))
        return kBusy;
    else if (!strcmp(text, "invalid"))
        return kInvalid;
    else
        throw std::runtime_error("Presence: Unknown presence "+std::string(text));
}

inline unsigned char Presence::fromStanza(strophe::Stanza pres)
{
    assert(!strcmp(pres.name(), "presence"));
    auto rawShow = pres.rawChild("show");
    if (rawShow)
    {
        strophe::Stanza show(rawShow);
        return fromString(show.text().c_str());
    }
    else
    {
        auto type = pres.attr("type");
        if (type)
            return fromString(type);
        else
            return Presence::kOnline;
    }
}
static inline std::string useridToJid(uint64_t userid)
{
    char buf[32];
    ::mega::Base32::btoa((byte*)&userid, sizeof(userid), buf);
    return std::string(buf)+"@"+KARERE_XMPP_DOMAIN;
}

}
#endif // CONTACTLIST_H
