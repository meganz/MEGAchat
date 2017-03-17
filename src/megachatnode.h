#ifndef MEGACHATNODE_H
#define MEGACHATNODE_H

#include "megachatapi.h"

namespace megachat
{
class MegaChatNode
{
public:
    MegaChatNode(MegaChatHandle nodeId, const std::string name);

    MegaChatHandle getId() const;
    std::string getName() const;

protected:
    megachat::MegaChatHandle mId;
    std::string mName;
};
}  // namespace megachat

#endif // MEGACHATNODE_H
