#include "megachatnode.h"

namespace megachat
{
MegaChatNode::MegaChatNode(MegaChatHandle nodeId, const std::string name)
    : mId(nodeId)
    , mName(name)
{
}

MegaChatHandle MegaChatNode::getId() const
{
    return mId;
}

std::string MegaChatNode::getName() const
{
    return mName;
}

}  // namespace megachat
