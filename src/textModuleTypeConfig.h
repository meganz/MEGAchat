#ifndef TEXTMODULE_TYPE_CONFIG
#define TEXTMODULE_TYPE_CONFIG
#include <group_member.h>
#include <upper_handler.h>

namespace karere
{
class TextModuleTypeConfig
{
//#define MPENC_T_PARAMS mpenc_cpp::Member,mpenc_cpp::GroupMember,mpenc_cpp::SharedMemberPtr,mpenc_cpp::UpperHandler
//#define MPENC_T_DYMMY_PARAMS DummyMember, DummyGroupMember,SharedDummyMember, DummyEncProtocolHandle

public:
#ifndef KARERE_TESTING
    /**
     * @brief The member type used for Client.
     */
    typedef mpenc_cpp::Member MemberType;
    /**
     * @brief The group member type used for Client.
     */
    typedef mpenc_cpp::GroupMember GroupMemberType;
    /**
     * @brief The shared member class used for Client.
     */
    typedef mpenc_cpp::SharedMemberPtr SharedMemberPtrType;
    /**
     * @brief The encryption handler type used for Client.
     */
    typedef mpenc_cpp::UpperHandler EncryptionHandlerType;
#else
    typedef DummyMember MemberType;
    typedef DummyGroupMember GroupMemberType;
    typedef SharedMemberPtrType SharedDummyMember;
    typedef DummyEncProtocolHandle EncryptionHandlerType;
#endif
};
}
#endif
