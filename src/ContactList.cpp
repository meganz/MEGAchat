#include "ContactList.h"
#include "ChatClient.h"

using namespace strophe;
using namespace promise;

namespace karere
{
Promise<int> ContactList::init()
{
    Promise<int> pms;
    mHandler = client.conn->addHandler([this, pms](Stanza presence, void*, bool& keep) mutable
    {
        const char* jid = presence.attrOrNull("from");
        if(!jid)
        {
            KR_LOG_WARNING("received presence stanza without 'from' attribute");
            return;
        }
        if (strstr(jid, "@conference."))
            return; //we are not interested in chatroom presences
        if (strcmp(jid, client.conn->jid()) == 0) //our own presence, this is the end of the presence list
        {
            if (!pms.done())
                pms.resolve(0);
            return;
        }
        auto status = presenceFromStanza(presence);
        contactsFullJid[jid] = status;
        auto& bareJidStatus = contactsBareJid[strophe::getBareJidFromJid(jid)];
        if (status > bareJidStatus)
            bareJidStatus = status;
    }, nullptr, "presence", nullptr, nullptr);
    return pms;
}

ContactList::~ContactList()
{
    if (mHandler)
        client.conn->removeHandler(mHandler);
}
}
