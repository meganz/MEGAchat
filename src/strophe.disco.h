#include <mstrophepp.h>
#include <set>
#include <memory>

#define DP_THROW_IF_NULL(param) \
do {  if (!param) throw std::runtime_error(__FUNCTION__+std::string(": Parameter '")+#param+"' cannot be NULL");} while (0)

#define DP_THROW_IF_EMPTY(param) \
do {  if ((param).empty()) throw std::runtime_error(__FUNCTION__+std::string(": Parameter '")+#param+"' cannot be empty");} while (0)

namespace disco
{
class DiscoPlugin;
class Node;

typedef std::map<std::string, std::string> StringMap;
typedef std::set<std::string> FeatureSet;
typedef std::map<std::string, std::shared_ptr<Node> > NodeMap;

struct Identity
{
    std::string name;
    //set iterators are const, but we want to update the below members, and they don't affect key of the set
    mutable std::string type;
    mutable std::string category;
    bool operator<(const Identity& rhs) const {return name < rhs.name;}
    Identity(const std::string& aName, const std::string& aType="", const std::string& aCategory="")
        :type(aType), category(aCategory)
    {
        if (aName.empty())
            throw std::runtime_error("Identity name must not be empty");
        name = aName;
    }
};
typedef std::set<Identity> IdentitySet;

class Node
{
protected:
    DiscoPlugin& mDisco;
//used in items query
    std::string jid;
    std::string name;
    std::string node;
//used in info query
    std::unique_ptr<FeatureSet> mFeatures;
    std::unique_ptr<IdentitySet> mIdentities;
//subondes
    std::unique_ptr<NodeMap> mNodes;
    struct QueryResponse
    {
        strophe::Stanza iq;
        strophe::Stanza query;
        template <class CtxSrc>
        QueryResponse(CtxSrc aCtxSrc): iq(aCtxSrc){}
    };

    QueryResponse responseFromQuery(strophe::Stanza req);
    strophe::Stanza replyInfo(strophe::Stanza req);
    strophe::Stanza replyItems(strophe::Stanza req, const char* node=NULL);
    strophe::Stanza replyOwnItems(strophe::Stanza req);
    strophe::Stanza replyNodeNotFound(strophe::Stanza req);

// We need to access some stuff of the DiscoPlugin this class, but since it is not yet
// defined (it derived from us), we implement this access via inline methods defined
// out-of-class, after DiscoPlugin is defined
    inline const char* rootJid() const;
public:
    Node(DiscoPlugin& disco, const char* identityName=NULL);
    void checkCreateIdentity()
    {
        if (mIdentities.get())
            return;
        mIdentities.reset(new IdentitySet);
    }

    bool setIdentity(const std::string& name, const std::string& type, const std::string& category);
    Node* addNode(const char* node, const char* name, const char* jid=NULL);
    bool addFeature(const std::string& feature);
};

class DiscoPlugin: public strophe::Plugin, public Node
{
protected:
public:
    DiscoPlugin(strophe::Connection& aConn, const char* identity=NULL);
    virtual void onConnState(const xmpp_conn_event_t status,
            const int error, xmpp_stream_error_t * const stream_error);

    promise::Promise<strophe::Stanza> request(const char* to, const char* ns, const char* node=NULL);
    promise::Promise<strophe::Stanza> info(const char* to, const char* node=NULL)
    {
        return request(to, XMPP_NS_DISCO_INFO, node);
    }

    promise::Promise<strophe::Stanza> items(const char* to, const char* node=NULL)
    {
        return request(to, XMPP_NS_DISCO_ITEMS, node);
    }
};

inline const char* Node::rootJid() const
{
    return mDisco.mConn.jid();
}
} //end namespace disco
