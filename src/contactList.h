#ifndef CONTACTLIST_H
#define CONTACTLIST_H
#include <mstrophepp.h>
#include <type_traits>
#include "karereCommon.h"

namespace karere
{

class DummyMember;
class DummyGroupMember;

typedef enum ePresence//sorted by 'chatty-ness'
{
    kPresenceOffline = 0,
    kPresenceOther = 1,
    kPresenceBusy = 2,
    kPresenceAway = 3,
    kPresenceOnline = 4,
    kPresenceChatty = 5,
} Presence;

class Contact
{
protected:
    /*contact's bare JID*/
    std::string bareJid;
    /*constact's presence state*/
    Presence presence;
public:
    Contact(const std::string& BareJid, const Presence pre = Presence::kPresenceOffline)
    : bareJid(BareJid)
    , presence(pre)
    {}

    /**
    * Setter for property `bareJid`
    *
    * @param val {string} contact's bare JID
    * @returns void
    */
    inline void setBaseJid(const std::string& BareJid)
    {
        bareJid = BareJid;
    }

    /**
     * Getter for property `bareJid`
     *
     * @returns {(string)} contact's bare JID
     */
    inline std::string getBareJid() const
    {
        return bareJid;
    }

    /**
    * Setter for property `presence`
    *
    * @param val {Presence} contact's presence state
    * @returns void
    */
    inline void setPresence(const Presence pre)
    {
        presence = pre;
    }

    /**
     * Getter for property `presence`
     *
     * @returns {(Presence)} contact's presence state.
     */
    inline Presence getPresence() const
    {
        return presence;
    }
};

class ContactList
{
public:
    typedef std::map<std::string, std::shared_ptr<Contact>> PresentContactMap;
    typedef std::map<std::string, Presence> PresentContactIdentityMap;

    ContactList(std::shared_ptr<strophe::Connection> connection);

    ~ContactList();
    /*
     * @brief convert the presence state to a text.
     */
    static inline Presence textToPresence(const char* text);

    /*
     * @brief convert the text to a presence state.
     */
    static inline std::string presenceToText(Presence presence);

    /*
     * @brief initialize the contactlist can register handler to handle the presence messages from contacts.
     */
    promise::Promise<int> init();


    /**
    * @brief Get a list of full Jids based on the give bare Jid.
    * @param jid {string} bare JID
    * @returns {(vector<std::string>)} A list of full Jids.
    */
    std::vector<std::string> getFullJidsOfJid(const std::string& jid) const;

    /**
    * @brief Add a contact to the contact list.
    * @param userJid {string} user's bared JID
    * @returns {(void)}
    */
    void addContact(const std::string& userJid);

    /**
    * @brief Get a contact from the contact list.
    * @param userJid {string} user's bared JID
    * @returns {(shared_ptr<Contact>)} a reference to the contact.
    */
    const Contact& getContact(const std::string& userJid) const;

    /**
    * @brief Get the list of contact's bare Jids from the contact list.
    * @returns {(vector<std::string>)} A list of contact's bare Jids.
    */
    std::vector<std::string> getContactJids() const;

    /**
    * @brief Get the size of contacts in the contact list.
    * @returns {(unsigned int)} size of the contacts.
    */
    unsigned int size() const;

protected:
    static inline Presence presenceFromStanza(strophe::Stanza pres);

protected:
    StringMap contactsFullJid;
    /*contacts of user, and the identity of contact is BareJid*/
    PresentContactMap contacts;
    std::shared_ptr<strophe::Connection> connection;
    xmpp_handler mHandler;
};

inline std::string ContactList::presenceToText(Presence presence)
{
    if (presence == kPresenceOffline)
        return std::string("unavailable");
    else if (presence == kPresenceOnline)
        return std::string("available");
    else if (presence == kPresenceAway)
        return std::string("away");
    else if (presence == kPresenceBusy)
        return std::string("dnd");
    else
    {
        KR_LOG_WARNING("presenceToText: Unknown presence");
        return std::string("Unknown");
    }
}

inline Presence ContactList::textToPresence(const char* text)
{
    assert(text);
    if (!strcmp(text, "unavailable"))
        return kPresenceOffline;
    else if (!strcmp(text, "chat") || !strcmp(text, "available"))
        return kPresenceOnline;
    else if (!strcmp(text, "away"))
        return kPresenceAway;
    else if (!strcmp(text, "dnd"))
        return kPresenceBusy;
    else
    {
        KR_LOG_WARNING("textToPresence: Unknown presence '%s', returnink kPresenceOther", text);
        return kPresenceOther;
    }
}

inline Presence ContactList::presenceFromStanza(strophe::Stanza pres)
{
    assert(!strcmp(pres.name(), "presence"));
    auto rawShow = pres.rawChild("show");
    if (rawShow != NULL) {
        strophe::Stanza show(rawShow);
        auto text = show.text();
        return textToPresence(text.c_str());
    } else if (pres.attrOrNull("type") != NULL){
        return textToPresence(pres.attr("type"));
    } else {
        return kPresenceOnline;
    }
}
};
#endif // CONTACTLIST_H
