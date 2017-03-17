#include "megachatuser.h"

namespace megachat
{

MegaChatUser::MegaChatUser(MegaChatHandle contactId, const std::string &email, const std::string name)
    : mId(contactId)
    , mEmail(email)
    , mName(name)
{
}

MegaChatHandle MegaChatUser::getId() const
{
    return mId;
}

std::string MegaChatUser::getEmail() const
{
    return mEmail;
}

std::string MegaChatUser::getName() const
{
    return mName;
}
}  // namespace megachat
