#ifndef CONTACTLIST_H
#define CONTACTLIST_H
#include <mstrophepp.h>
#include <type_traits>
#include "karereCommon.h"
#include <mega/base64.h>

namespace karere
{
class Contact;

class Presence
{
public:
    enum Status//sorted by 'chatty-ness'
    {
        kOffline = 0,
        kBusy = 1,
        kAway = 2,
        kOnline = 3,
        kChatty = 4,
        kLast = kChatty
    };
    Presence(Status pres=kOffline): mPres(pres){}
    inline Presence(const char*str): mPres(fromString(str)){}
    Presence(strophe::Stanza stanza): mPres(fromStanza(stanza)){}
    operator Status() const { return mPres; }
    Status val() const { return mPres; }
    inline const char* toString();
    static inline Status fromString(const char*);
    static inline Status fromStanza(strophe::Stanza);
protected:
    Status mPres;
    static const char* sStrings[Presence::kLast+1];
};

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
    promise::Promise<void> receivePresences();
public:
    XmppContactList(Client& client): mClient(client){}
    ~XmppContactList();

    /** @brief initialize the contactlist to handle the presence messages from contacts. */
    promise::Promise<void> init();
    bool addContact(const std::string& fullJid, Presence pres, std::string bareJid="");
    std::shared_ptr<XmppContact> addContact(Contact& contact);
    XmppContact& getContact(const std::string& bareJid) const;
    using Base::operator[];
    using Base::size;
    static Presence presenceFromStanza(strophe::Stanza pres);
};
inline const char* Presence::toString()
{
    if (mPres == kOffline)
        return "unavailable";
    else if (mPres == kOnline)
        return "available";
    else if (mPres == kAway)
        return "away";
    else if (mPres == kBusy)
        return "dnd";
    else
        throw std::runtime_error("Presence::toString: Unknown presence "+std::to_string(mPres));
}

inline Presence::Status Presence::fromString(const char* text)
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
    else
        throw std::runtime_error("Presence: Unknown presence "+std::string(text));
}

inline Presence::Status Presence::fromStanza(strophe::Stanza pres)
{
    assert(!strcmp(pres.name(), "presence"));
    auto rawShow = pres.rawChild("show");
    if (rawShow)
    {
        strophe::Stanza show(rawShow);
        return fromString(show.text().c_str());
    }

    auto type = pres.attrOrNull("type");
    if (type)
    {
        return fromString(type);
    }
    else
    {
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
