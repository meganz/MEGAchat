#include "ContactList.h"
#include "ChatClient.h"

using namespace strophe;
using namespace promise;
using namespace std;

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
        if (strcmp(jid, client.conn->fullJid()) == 0) //our own presence, this is the end of the presence list
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
vector<string> ContactList::getFullJidsOfJid(const string& jid)
{
    size_t len = jid.find("/");
    if (len == string::npos)
        len = jid.size();
    vector<string> result;
    for (auto& item: contactsFullJid)
    {
        const string& fjid = item.first;
        if ((strncmp(fjid.c_str(), jid.c_str(), len) == 0)
         && (fjid[len] == '/'))
            result.emplace_back(fjid);
    }
    return result;
}

}
