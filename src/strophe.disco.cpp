#include <set>
#include <stdexcept>
#include <mstrophepp.h>
#include "strophe.disco.h"

namespace disco
{
using namespace std;

Node::Node(DiscoPlugin& disco, const char* identityName)
:mDisco(disco)
{
    if (identityName)
    {
        mIdentities.reset(new IdentitySet);
        mIdentities->emplace(identityName);
    }
}

bool Node::setIdentity(const std::string& name, const std::string& type, const std::string& category)
{
    checkCreateIdentity();
    auto ret = mIdentities->emplace(name, type, category);
    if (!ret.second) //update existing
    {
        auto& identity = *ret.first; //identity is const (set iterators are const), but type and category are mutable
        identity.type = type;
        identity.category = category;
        return false;
    }
    else
        return true;
}
Node* Node::addNode(const char* node, const char* name, const char* jid)
{
        DP_THROW_IF_NULL(node);
        DP_THROW_IF_NULL(name);
        if (!mNodes)
            mNodes.reset(new NodeMap);
        std::shared_ptr<Node> nodeInst(new Node(mDisco));
        auto res = mNodes->emplace(node, nodeInst);
        if (!res.second)
            throw std::runtime_error("Node with that name already exists");
        nodeInst->node = node;
        nodeInst->name = name;
        nodeInst->jid = jid?jid:rootJid();

        return nodeInst.get();
}
bool Node::addFeature(const std::string& feature)
{
    DP_THROW_IF_EMPTY(feature);
    if (!mFeatures)
        mFeatures.reset(new FeatureSet);
    return mFeatures->emplace(feature).second;
}


Node::ResponseIq Node::responseFromQuery(strophe::Stanza req)
{
    ResponseIq res(mDisco.mConn);
    res.setName("iq")
       .setAttr("to", req.attr("from"))
       .setAttr("id", req.attr("id"))
       .setAttr("type", "result");
    res.query = res.c("query");
    auto reqQuery = req.child("query");

    const char* attr = reqQuery.attrOrNull("xmlns");
    if (attr)
        res.query.setAttr("xmlns", attr);
    if ((attr = reqQuery.attrOrNull("node")))
        res.query.setAttr("node", attr);

    return res;
}

strophe::Stanza Node::replyInfo(strophe::Stanza req)
{
    auto res = responseFromQuery(req);
    auto& resQuery = res.query;
    FeatureSet& features = mFeatures?*mFeatures:*mDisco.mFeatures;
    IdentitySet& identities = mIdentities?*mIdentities:*mDisco.mIdentities;
    for (auto& identity: identities)
    {
        auto s = resQuery.c("identity");
        s.setAttr("name", identity.name.c_str());
        if (!identity.type.empty())
            s.setAttr("type", identity.type.c_str());
        if (!identity.category.empty())
            s.setAttr("category", identity.category.c_str());
    }
    for (auto& feature: features)
        resQuery.c("feature").setAttr("var", feature.c_str());
    return res;
}
strophe::Stanza Node::replyItems(strophe::Stanza req, const char* node)
{
    if (!mNodes)
        return replyNodeNotFound(req);

    if (!node)
        node = req.child("query").attrOrNull("node");
    if (!node)
        return replyOwnItems(req);

    std::string subnodeName;
    const char* pos = strchr(node, '/');
    if (!pos)
        subnodeName = node;
    else
        subnodeName.assign(node, pos-node);
    auto nodeIt = mNodes->find(subnodeName);
    if (nodeIt == mNodes->end())
        return replyNodeNotFound(req);
    else
        return nodeIt->second->replyItems(req, pos+1);
}

strophe::Stanza Node::replyOwnItems(strophe::Stanza req)
{

    auto res = responseFromQuery(req);
    if (!mNodes)
        return res;

    for (auto& nodeIt: *mNodes)
    {
        auto& node = *nodeIt.second;
        auto item = res.query.c("item");
        item.setAttr("jid", (!node.jid.empty())?node.jid.c_str():mDisco.rootJid());
        if (!node.name.empty())
            item.setAttr("name", node.name.c_str());
        if (!node.node.empty())
            item.setAttr("node", node.node.c_str());
    }
    return res;
}

strophe::Stanza Node::replyNodeNotFound(strophe::Stanza req)
{
    auto res = responseFromQuery(req);
    res.query.c("error").setAttr("type", "cancel")
            .c("item-not-found").setAttr("xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas");
    return res;
}

template <typename P>
static inline P* throwIfNull(P* ptr, const char* msg)
{
    if (!ptr)
    {
        if (!msg)
            msg = "unexpected NULL value";
        throw runtime_error(msg);
    }
    return ptr;
}

DiscoPlugin::DiscoPlugin(strophe::Connection& aConn, const char* identity)
:Plugin(aConn), Node(*this, throwIfNull(identity, "No identity specified"))
{
    assert(mIdentities.get());
    mFeatures.reset(new FeatureSet);
}
void DiscoPlugin::onConnState(const xmpp_conn_event_t status,
            const int error, xmpp_stream_error_t * const stream_error)
{
    if (status == XMPP_CONN_CONNECT)
    {
        mConn.addHandler([this](strophe::Stanza stanza, void* userdata, bool& keep)
        {
            xmpp_send(mConn, replyInfo(stanza));
        }, XMPP_NS_DISCO_INFO, "iq", "get");
        mConn.addHandler([this](strophe::Stanza stanza, void* userData, bool& keep)
        {
            xmpp_send(mConn, replyItems(stanza));
        }, XMPP_NS_DISCO_ITEMS, "iq", "get");
    }
}

promise::Promise<strophe::Stanza>
DiscoPlugin::request(const char* to, const char* ns, const char* node)
{
    DP_THROW_IF_NULL(to);
    DP_THROW_IF_NULL(ns);
    strophe::Stanza iq(mConn);
    iq.setName("iq")
            .setAttr("to", to)
            .setAttr("from", mConn.jid());
    auto query = iq.c("query");
    query.setAttr("xmlns", ns);
    if (node)
        query.setAttr("node", node);

    return mConn.sendIqQuery(iq, "disco");
}
}
