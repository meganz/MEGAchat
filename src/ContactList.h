#ifndef CONTACTLIST_H
#define CONTACTLIST_H
#include <mstrophepp.h>
#include "karereCommon.h"

namespace karere
{

class Client;
enum Presence //sorted by 'chatty-ness'
{
    kPresenceOffline = 0,
    kPresenceOther = 1,
    kPresenceBusy = 2,
    kPresenceAway = 3,
    kPresenceOnline = 4,
    kPresenceChatty = 5,
};
class ContactList
{
public:
    typedef std::map<std::string, Presence> OnlineMap;
    Client& client;
    ContactList(Client& aClient):client(aClient){}
    promise::Promise<int> init();
    ~ContactList();
    OnlineMap contactsFullJid;
    OnlineMap contactsBareJid;
protected:
    xmpp_handler mHandler;
};
static inline Presence textToPresence(const char* text)
{
    assert(text);
    if (!strcmp(text, "unavailable"))
        return kPresenceOffline;
    else if (!strcmp(text, "chat"))
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
static inline Presence presenceFromStanza(strophe::Stanza pres)
{
    assert(!strcmp(pres.name(), "presence"));
    auto rawShow = pres.rawChild("show");
    if (!rawShow)
        return kPresenceOnline;
    strophe::Stanza show(rawShow);
    auto text = show.text();
    return textToPresence(text.c_str());
}
};
#endif // CONTACTLIST_H
