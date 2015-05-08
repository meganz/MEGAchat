#include "contactList.h"
#include <mstrophepp.h>
#include <type_traits>
#include "busConstants.h"
#include "karereCommon.h"

namespace karere
{
ContactList::ContactList(std::shared_ptr<strophe::Connection> connection)
: connection(connection)
{}

promise::Promise<int> ContactList::init()
{
    promise::Promise<int> pms;
    connection->addHandler([this, pms](strophe::Stanza presence, void*, bool& keep) mutable
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
            CHAT_LOG_DEBUG("Bare Jid:%s --- State : %d", it->second->getBareJid().c_str(), it->second->getPresence());
        }
    }, nullptr, "presence", nullptr, nullptr);
    return pms;
}

std::vector<std::string> ContactList::getFullJidsOfJid(const std::string& jid) const
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

std::vector<std::string> ContactList::getContactJids() const
{
    std::vector<std::string> result;
    for (auto& item: contacts)
    {
        result.push_back(item.first);
    }
    return result;
}

unsigned int ContactList::size() const
{
    return contacts.size();
}

void ContactList::addContact(const std::string& userJid)
{
    contacts[userJid] = std::shared_ptr<Contact>(new Contact(userJid));
}

const Contact& ContactList::getContact(const std::string& userJid) const
{
    PresentContactMap::const_iterator it = contacts.find(userJid);
    if (it != contacts.end()) {
        return *it->second;
    } else {
        throw std::runtime_error("invalid user JID");
    }
}

ContactList::~ContactList()
{}

}
