#ifndef CONTACTLIST_H
#define CONTACTLIST_H
#include <mstrophepp.h>
#include <type_traits>
#include "busConstants.h"

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

    ContactList(std::shared_ptr<strophe::Connection> connection):connection(connection){
    }

    static inline Presence textToPresence(const char* text)
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

    static inline std::string presenceToText(Presence presence)
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
    promise::Promise<int> init()
    {
        promise::Promise<int> pms;
        mHandler = connection->addHandler([this, pms](strophe::Stanza presence, void*, bool& keep) mutable
        {
            const char* jid = presence.attrOrNull("from");
            if(!jid)
            {
                KR_LOG_WARNING("received presence stanza without 'from' attribute");
                return;
            }
            if (strstr(jid, "@conference."))
                return; //we are not interested in chatroom presences
            if (strcmp(jid, connection->fullJid()) == 0) //our own presence, this is the end of the presence list
            {
                if (!pms.done())
                    pms.resolve(0);
                return;
            }
            auto status = presenceFromStanza(presence);
            contactsFullJid[jid] = status;
            if (contacts.find(strophe::getBareJidFromJid(jid)) == contacts.end())
            {
                contacts[strophe::getBareJidFromJid(jid)] =  std::shared_ptr<Contact>(new Contact(strophe::getBareJidFromJid(jid), status));
                message_bus::SharedMessage<> busMessage(CONTACT_ADDED_EVENT);
                busMessage->addValue(CONTACT_JID, strophe::getBareJidFromJid(jid));
                message_bus::SharedMessageBus<>::getMessageBus()->alertListeners(CONTACT_ADDED_EVENT, busMessage);
            }
            auto& bareJidStatus = contacts[strophe::getBareJidFromJid(jid)];
            if (status != bareJidStatus->getPresence()) {
                message_bus::SharedMessage<> busMessage(CONTACT_CHANGED_EVENT);
                busMessage->addValue(CONTACT_JID, strophe::getBareJidFromJid(jid));
                busMessage->addValue(CONTACT_STATE, status);
                busMessage->addValue(CONTACT_OLD_STATE, bareJidStatus->getPresence());
                message_bus::SharedMessageBus<>::getMessageBus()->alertListeners(CONTACT_CHANGED_EVENT, busMessage);

                bareJidStatus->setPresence(status);
            }
            for (std::map<std::string, std::shared_ptr<Contact>>::iterator it = contacts.begin(); it != contacts.end(); it++)
            {
                KR_LOG_DEBUG("Bare Jid:%s --- State : %d\n", it->second->getBareJid().c_str(), it->second->getPresence());
            }
        }, nullptr, "presence", nullptr, nullptr);
        return pms;
    }

    ~ContactList()
    {
        if (mHandler)
            connection->removeHandler(mHandler);
    }

    /**
    * @brief Get a list of full Jids based on the give bare Jid.
    * @param jid {string} bare JID
    * @returns {(vector<std::string>)} A list of full Jids.
    */
    std::vector<std::string> getFullJidsOfJid(const std::string& jid) const
    {
        size_t len = jid.find("/");
        if (len == std::string::npos)
            len = jid.size();
        std::vector<std::string> result;
        for (auto& item: contactsFullJid)
        {
            const std::string& fjid = item.first;
            if ((strncmp(fjid.c_str(), jid.c_str(), len) == 0)
             && (fjid[len] == '/'))
                result.emplace_back(fjid);
        }
        return result;
    }

    /**
    * @brief Add a contact to the contact list.
    * @param userJid {string} user's bared JID
    * @returns {(void)}
    */
    void addContact(const std::string& userJid)
    {
        contacts[userJid] = std::shared_ptr<Contact>(new Contact(userJid));
    }

    /**
    * @brief Get a contact from the contact list.
    * @param userJid {string} user's bared JID
    * @returns {(shared_ptr<Contact>)} a reference to the contact.
    */
    const Contact& getContact(const std::string& userJid) const
    {
        PresentContactMap::const_iterator it = contacts.find(userJid);
        if (it != contacts.end()) {
            *it->second;
        } else {
            throw std::runtime_error("invalid user JID");
        }
    }

    /**
    * @brief Get the list of contact's bare Jids from the contact list.
    * @returns {(vector<std::string>)} A list of contact's bare Jids.
    */
    std::vector<std::string> getContactJids() const
	{
        std::vector<std::string> result;
        for (auto& item: contacts)
        {
            result.push_back(item.first);
        }
        return result;
	}

    /**
    * @brief Get the size of contacts in the contact list.
    * @returns {(unsigned int)} size of the contacts.
    */
    unsigned int size() const
    {
        return contacts.size();
    }

protected:
    static inline Presence presenceFromStanza(strophe::Stanza pres)
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
protected:
    StringMap contactsFullJid;
    /*contacts of user, and the identity of contact is BareJid*/
    PresentContactMap contacts;
    std::shared_ptr<strophe::Connection> connection;
    xmpp_handler mHandler;
};

};
#endif // CONTACTLIST_H
