#ifndef JSONNODE_H
#define JSONNODE_H

#include <map>
#include <string>
#include <vector>

namespace megachat
{
class JSonNode
{
public:
    JSonNode();
    JSonNode(const std::string& data);
    ~JSonNode();


    int getNumberVectorElement() const;
    const JSonNode getVectorElement(int index) const;

    const JSonNode getMapElement(const std::string& key) const;

    std::string getFinalValue() const;

    std::string getKey() const;
    void setKey(const std::string& key);

    JSonNode getValueNode() const;

    std::string getString() const;

    void setFinalValue(const std::string& value);
    void setKeyValueNode(const std::string& key, JSonNode node);
    void setMapNode(JSonNode node);
    void setVectorNode(JSonNode node);

private:
    std::vector<JSonNode> getNodes(const std::string& data);
    std::string extractToken(const std::string& data);

    std::string mData;
    int mElementNumber;

    std::vector<JSonNode> mKeyNodes;
    std::vector<JSonNode> mVectorNodes;
    std::vector<JSonNode> mFinalNode;
    std::string mFinalValue;
    std::string mKey;

};
}  // namespace megachat

#endif // JSONNODE_H
