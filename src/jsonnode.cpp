#include "jsonnode.h"

#include <iostream>
#include <cassert>

namespace megachat
{
JSonNode::JSonNode()
    : mData("")
    , mElementNumber(0)
{

}

JSonNode::JSonNode(const std::string &data)
    : mData(data)
    , mElementNumber(0)
{
    if (mData[0] == '{' && mData[mData.length() - 1] == '}')
    {
        mKeyNodes = getNodes(mData);
    }
    else if (mData[0] == '[' && mData[data.length() - 1] == ']')
    {
        mVectorNodes = getNodes(mData);
    }
    else
    {
        std::string token = extractToken(mData);

        if (token == mData)
        {
            mFinalValue = mData;
        }
        else
        {
            mKey = token;
            if (mKey.length() > 2)
            {
                mKey.erase(0, 1);
                mKey.erase(mKey.length() - 1, mKey.length());
            }

            // Format Token:Value
            std::string value = mData.substr(token.length() + 1);
            JSonNode node(value);
            mFinalNode.push_back(node);
        }
    }
}

JSonNode::~JSonNode()
{
}


int JSonNode::getNumberVectorElement() const
{
    return mVectorNodes.size();
}

const JSonNode JSonNode::getVectorElement(int index) const
{
    if (index >= 0 && index <= mVectorNodes.size())
    {
        return mVectorNodes.at(index);
    }

    return JSonNode();
}

const JSonNode JSonNode::getMapElement(const std::string &key) const
{
    for (int i = 0; i < mKeyNodes.size(); i++)
    {
        if (mKeyNodes[i].getKey() == key)
        {
            return mKeyNodes[i];
        }
    }

    return JSonNode();
}

std::string JSonNode::getFinalValue() const
{
    return mFinalValue;
}

std::string JSonNode::getKey() const
{
    return mKey;
}

void JSonNode::setKey(const std::string &key)
{
    mKey = key;
}

JSonNode JSonNode::getValueNode() const
{
    if (mFinalNode.size() == 1)
    {
        return mFinalNode[0];
    }

    return JSonNode();
}

std::string JSonNode::getString() const
{
    std::string jSonString;

    if (mFinalValue != "")
    {
        jSonString = mFinalValue;
    }
    else if (mFinalNode.size() == 1)
    {
        jSonString = mFinalNode[0].getString();
    }
    else if (mKeyNodes.size() > 0)
    {
        jSonString = "{";

        for (int i = 0; i < mKeyNodes.size(); ++ i)
        {
            jSonString += "\"" + mKeyNodes[i].getKey() + "\"" + ":" + mKeyNodes[i].getString() + ",";
        }

        jSonString[jSonString.length() - 1] = '}';
    }
    else if (mVectorNodes.size() > 0)
    {
        jSonString = "[";

        for (int i = 0; i < mVectorNodes.size(); ++ i)
        {
            jSonString += mVectorNodes.at(i).getString() + ",";
        }

        jSonString[jSonString.length() - 1] = ']';
    }

    return jSonString;
}

void JSonNode::setFinalValue(const std::string &value)
{
    assert(mVectorNodes.size() == 0 && mKeyNodes.size() == 0 && mFinalNode.size() == 0);

    mFinalValue = value;
}

void JSonNode::setKeyValueNode(const std::string &key, JSonNode node)
{
    assert(mVectorNodes.size() == 0 && mKeyNodes.size() == 0 && mFinalValue.length() == 0);

    setKey(key);

    mFinalNode.push_back(node);
}

void JSonNode::setMapNode(JSonNode node)
{
    assert(mVectorNodes.size() == 0 && mFinalNode.size() == 0 && mFinalValue.length() == 0);

    mKeyNodes.push_back(node);
}

void JSonNode::setVectorNode(JSonNode node)
{
    assert(mKeyNodes.size() == 0 && mFinalNode.size() == 0 && mFinalValue.length() == 0);

    mVectorNodes.push_back(node);
}

std::vector<JSonNode> JSonNode::getNodes(const std::string &data)
{
    std::string dataToParser = data;
    dataToParser.erase(0, 1);
    dataToParser = dataToParser.erase(dataToParser.length() - 1, dataToParser.length());

    std::vector<JSonNode> nodes;

    int i = 0;
    int bracesNumber = 0;
    int bracketsNumber = 0;
    int startSubNode = 0;
    int dataLength = 0;
    while (i < dataToParser.length() - 1 && dataToParser.length() > 0)
    {
        switch (dataToParser[i])
        {
        case '[':
            bracketsNumber ++;
            break;
        case '{':
            bracesNumber ++;
            break;
        case ']':
            bracketsNumber --;
            break;
        case '}':
            bracesNumber --;
            break;
        default:
            break;
        }

        if (bracesNumber == 0 && bracketsNumber == 0 && dataToParser[i + 1] == ',')
        {
            std::string data = dataToParser.substr(startSubNode, dataLength + 1);
            i += 2;
            startSubNode = i;
            dataLength = 0;
            JSonNode node(data);
            nodes.push_back(node);
        }
        else
        {
            i ++;
            dataLength ++;
        }
    }

    if (dataToParser.length() > 0)
    {
        std::string dataSubNode = dataToParser.substr(startSubNode, dataLength + 1);
        JSonNode node(dataSubNode);
        nodes.push_back(node);
    }
    else
    {
        nodes.clear();
    }

    return nodes;
}

std::string JSonNode::extractToken(const std::string &data)
{
    std::string token;

    if (data[0] == '"')
    {
        int i = 1;
        token.push_back('"');
        while (data[i] != '"' && i < data.length())
        {
            token.push_back(data[i]);
            ++i;
        }

        if (data[i] == '"')
        {
            token.push_back('"');
        }
    }
    else
    {
        token = data;
    }

    return token;
}
}  // namespace megachat

