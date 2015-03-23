#ifndef KAREREEVENTOBJECTS_H
#define KAREREEVENTOBJECTS_H

namespace karere
{

typedef enum presence_status {
    ONLINE,           //"chat"
	AVAILABLE,        //"available
	AWAY,             //"away"
	BUSY,             //"dnd"
	EXTENDED_AWAY,    //"xa"
	OFFLINE,          //"unavailable"
	NUMBER_OF_STATUS
} PRESENCE_STATUS;

typedef struct xmpp_node {
	std::string node_val;
} XMPP_NODE;

/**
* Event Object `Message`, this is a base class of all derived 'Message' Objects.
*
*/
class Message {
public:
	/*
	* @param toJid {string} recipient's JID
	* @param fromJid {string} sender's JID
	* @param roomJid {string} target room's JID
	* @constructor
	*/
    Message(const std::string& ToJid, const std::string& FromJid, const std::string& RoomJid)
	:toJid(ToJid)
	,fromJid(FromJid)
	,roomJid(RoomJid)
	{}
	/*
	* @deconstructor
	*/
    virtual ~Message()
    {}
    /**
    * Getter for property `toJid`
    *
    * @returns {(string)} recipient's JID
    */
    inline const std::string& getToJid() const
    {
        return this->toJid;
    }
    /**
    * Setter for property `toJid`
    *
    * @param val {string} recipient's JID
    * @returns void
    */
    inline void setToJid(const std::string& ToJid)
    {
        this->toJid = ToJid;
    }
    /**
     * Getter for property `fromJid`
     *
     * @returns {(string)} sender's JID
     */
    inline const std::string& getFromJid() const
    {
        return this->fromJid;
    }
    /**
    * Setter for property `fromJid`
    *
    * @param val {string} sender's JID
    * @returns void
    */
    inline void setFromJid(const std::string& FromJid)
    {
    	this->fromJid = FromJid;
    }
    /**
     * Getter for property `roomJid`
     *
     * @returns {(string)} room's JID
     */
    inline const std::string& getRoomJid() const
    {
        return this->roomJid;
    }
    /**
    * Setter for property `roomJid`
    *
    * @param val {string} room's JID
    * @returns void
    */
    inline void setRoomJid(const std::string& RoomJid)
    {
    	this->roomJid = RoomJid;
    }
protected:
    std::string toJid;
    std::string fromJid;
    std::string roomJid;
};
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
/**
* Meta Object `InviteMessage`
*
*/
class MessageMeta {
public:
	MessageMeta() {
    }
	MessageMeta (const MessageMeta& obj) {
        create_time = obj.create_time;
        type = obj.type;
        participants.clear();
        participants.assign(obj.participants.begin(), obj.participants.end());
        users.clear();
        users.assign(obj.users.begin(), obj.users.end());
	}
    std::string create_time;
    std::string type;
    std::vector<std::string> participants;
    std::vector<std::string> users;
    //StringMap data;
};
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
/**
* Event Object `InviteMessage`
*
*/

class InviteMessage : public Message{
typedef Message super;
public:
    typedef enum eInvitationType {
    	UNKNOWN = 0,
    	CREATE = 1,
		RESUME  = 2
    } INVITATION_TYPE;
	/*
	* @param ToJid {string} recipient's JID
	* @param FromJid {string} sender's JID
	* @param FoomJid {string} target room's JID
	* @param InvitationType {INVITATION_TYPE} invitation type
	* @param [Password] {string} optional, password for joining the room
	* @param [Meta] {Object} optional, attached META for this message (can be any JavaScript plain object)
	* @param [Delay] {number} unix time stamp saying when this message was sent
	* @constructor
	*/
	InviteMessage(const std::string& ToJid, const std::string& FromJid, const std::string& RoomJid, INVITATION_TYPE InvitationType, const MessageMeta& metaObj, const std::string& Password = "", int Delay = 0)
    :super(ToJid, FromJid, RoomJid)
    ,meta(metaObj)
    ,invitationType(InvitationType)
    ,password(Password)
    ,delay(Delay)
    {}
	/*
	* @deconstructor
	*/
	virtual ~InviteMessage()
	{}
	/**
	* Getter for property `password`
	*
	* @returns {(string)} password
	*/
    inline const std::string& getPassword() const
    {
    	return this->password;
    }
    /**
    * Setter for property `password`
    *
    * @param val {string} password
    * @returns void
    */
    inline void setPassword(const std::string& Password)
    {
    	this->password = Password;
    }
	/**
	* Getter for property `invitationType`
	*
	* @returns {(INVITATION_TYPE)} invitationType
	*/
    inline INVITATION_TYPE getInvitationType() const
    {
    	return this->invitationType;
    }
    /**
    * Setter for property `invitationType`
    *
    * @param val {INVITATION_TYPE} InvitationType
    * @returns void
    */
    inline void setInvitationType(INVITATION_TYPE InvitationType)
    {
    	this->invitationType = invitationType;
    }
	/**
	* Getter for property `meta`
	*
	* @returns {(Meta)}
	*/
    inline const MessageMeta& getMeta() const
    {
        return meta;
    }
public:
    MessageMeta meta;
protected:
    INVITATION_TYPE invitationType;
    std::string password;
    unsigned int delay;
};
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
/**
* Event Object `IncomingMessage`
*
*/

class IncomingMessage : public Message{
typedef Message super;
public:
    typedef enum eMessageType {
        CHAT = 0,
	    GROUPCHAT = 1,
	    ACTION =2
    } MESSAGE_TYPE;
	/*
	* @param toJid {string} recipient's JID
	* @param fromJid {string} sender's JID
	* @param roomJid {string}, may contain the Room JID (if this message was sent to a XMPP conf. room)
	* @param messageId {string} unique ID of the message
	* @param [contents] {string} optional, message contents
	* @param [rawMessage] {Element} Raw XML {Element} of the <message/> that was recieved by the XMP
	* @param [meta] {Object} optional, attached META for this message (can be any JavaScript plain object)
	* @param type {string} type of the message (most likely "Message")
	* @param rawType {string} XMPP type of the message
	* {@param [elements] {NodeList} child {Element} nodes from the XMPP's <message></message> node}
	* @param [delay] {number} unix time stamp saying when this message was sent
	* @param [seen] {boolean} used for notification to track whether we need to notify the message if it was not seen by the user
	* @constructor
	*/
	IncomingMessage(const std::string& ToJid,
			const std::string& FromJid,
			const std::string& RoomJid,
			const MESSAGE_TYPE Type = CHAT,
			const std::string& MessageId = "",
			const std::string& Contents = "",
			const std::string& RawMessage = "",
			const std::string& Meta = "",
			const std::string& RawType = "message",
		    int Delay = 0,
			bool Seen = true
			)
    :super(ToJid, FromJid, RoomJid)
    ,type(Type)
    ,messageId(MessageId)
    ,contents(Contents)
    ,rawMessage(RawMessage)
    ,meta(Meta)
    ,rawType(RawType)
    ,delay(Delay)
    ,seen(Seen)
    {
    }
	/*
	* @deconstructor
	*/
	virtual ~IncomingMessage()
	{}
	/**
	* Getter for property `type`
	*
	* @returns {(string)} type
	*/
    inline const MESSAGE_TYPE getType() const
    {
    	return this->type;
    }
    /**
    * Setter for property `type`
    *
    * @param val {string} type
    * @returns void
    */
    inline void setType(const MESSAGE_TYPE Type)
    {
    	this->type = Type;
    }
	/**
	* Getter for property `contents`
	*
	* @returns {(string)} contents
	*/
    inline const std::string& getContents() const
    {
    	return this->contents;
    }
    /**
    * Setter for property `contents`
    *
    * @param val {string} contents
    * @returns void
    */
    inline void setContents(const std::string& Contents)
    {
    	this->contents = Contents;
    }

protected:
    MESSAGE_TYPE type;
    std::string messageId;
    std::string contents;
    std::string rawMessage;
    std::string meta;
    std::string rawType;
    std::vector<XMPP_NODE> elements;
    unsigned int delay;
    bool seen;

};
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
/**
* Event Object `Presence`
*/
class PresenceMessage : public Message
{
typedef Message super;

	/*
	* @param toJid {string} recipient's JID
	* @param fromJid {string} sender's JID
	* @param roomJid {string}, may contain the Room JID (if this message was sent to a XMPP conf. room)
	* @param [type] {string} type of the presence event
	* @param [status] {string} status of the user
	* @param [delay] {number} unix time stamp saying when this presence was last updated
	* @constructor
	*/
public:
    PresenceMessage(const std::string& ToJid, const std::string& FromJid, const std::string& RoomJid, const std::string& Type = "available", const std::string& Status = "", unsigned int Delay = 0)
    :super(ToJid, FromJid, RoomJid)
    ,type(Type)
    ,status(Status)
    ,delay(Delay)
    {}

	/*
	* @deconstructor
	*/
    virtual ~PresenceMessage()
    {}

	/**
	* Getter for property `type`
	*
	* @returns {(string)}
	*/
    inline const std::string& getType() const
    {
        return this->type;
    }

    /**
    * Setter for property `type`
    *
    * @param val {string} type
    * @returns void
    */
    inline void setType(const std::string& Type)
    {
        this->type = Type;
    }

protected:
    std::string type;
    std::string status;
    unsigned int delay;
};
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
/**
* Event Object `UsersJoned`
*/

class UsersJoinedMessage : public Message
{
typedef Message super;
	/*
	* @param ToJid {string} recipient's JID
	* @param FromJid {string} sender's JID
	* @param RoomJid {string}, may contain the Room JID (if this message was sent to a XMPP conf. room)
	* @param CurrentUsers {std::vector<string>} list of the current users.
	* @param NewUsers {std::vector<string>} list of the new users
	* @constructor
	*/

public:
    UsersJoinedMessage(const std::string& ToJid, const std::string& FromJid, const std::string& RoomJid, const std::vector<std::string>& CurrentUsers, const std::vector<std::string>& NewUsers)
    :super(ToJid, FromJid, RoomJid)
    {
        mCurrentUsers.clear();
        mCurrentUsers.assign(CurrentUsers.begin(), CurrentUsers.end());
        mNewUsers.clear();
        mNewUsers.assign(NewUsers.begin(), NewUsers.end());
    }

    const std::vector<std::string>& getCurrentUsers() const
    {
        return mCurrentUsers;
    }

    const std::vector<std::string>& getNewUsers() const
	{
        return mNewUsers;
	}
	/*
	* @deconstructor
	*/
    virtual ~UsersJoinedMessage()
    {}
protected:
    std::vector<std::string> mCurrentUsers;
    std::vector<std::string> mNewUsers;
};
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
/**
* Event Object `Action`
*/

class ActionMessage : public Message
{
typedef Message super;
	/*
	* @param ToJid {string} recipient's JID
	* @param FromJid {string} sender's JID
	* @param RoomJid {string}, may contain the Room JID (if this message was sent to a XMPP conf. room)
	* @param CurrentUsers {std::vector<string>} list of the current users.
	* @param NewUsers {std::vector<string>} list of the new users
	* @constructor
	*/

public:
    typedef enum eActionType
    {
        CONVERSATION_START = 0,
		CONVERSATION_END = 1,
		USER_JOIN = 2,
		USER_LEFT = 3
    } ACTION_TYPE;

    ActionMessage(const std::string& ToJid, const std::string& FromJid, const std::string& RoomJid, ACTION_TYPE ActionType)
    :super(ToJid, FromJid, RoomJid)
    ,mActionType(ActionType)
    {}

	/**
	* Getter for property `mActionType`
	*
	* @returns {(ACTIONTYPE)}
	*/
    const ACTION_TYPE getActionType() const
    {
        return mActionType;
    }

    /**
    * Setter for property `mActionType`
    *
    * @param val {ACTIONTYPE} contents
    * @returns void
    */
    void setActionType(const ACTION_TYPE ActionType)
	{
        mActionType = ActionType;
	}
	/*
	* @deconstructor
	*/
    virtual ~ActionMessage()
    {}
protected:
    ACTION_TYPE mActionType;
};
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
/**
* Event Object `ChatState`
*/

class ChatState : public Message
{
typedef Message super;
	/*
	* @param ToJid {string} recipient's JID
	* @param FromJid {string} sender's JID
	* @param RoomJid {string}, may contain the Room JID (if this message was sent to a XMPP conf. room)
	* @param CurrentUsers {std::vector<string>} list of the current users.
	* @param NewUsers {std::vector<string>} list of the new users
	* @constructor
	*/

public:
    typedef enum eStateType
    {
        INACTIVE = 0,
        ACTIVE = 1,
        COMPOSING = 2,
        PAUSED = 3,
        GONE = 4
    } STATE_TYPE;

    ChatState(const std::string& ToJid, const std::string& FromJid, const std::string& RoomJid, STATE_TYPE StateType)
    :super(ToJid, FromJid, RoomJid)
    ,mStateType(StateType)
    {}

	/**
	* Getter for property `mStateType`
	*
	* @returns {(STATE_TYPE)}
	*/
    const STATE_TYPE getStateType() const
    {
        return mStateType;
    }

    /**
    * Setter for property `mStateType`
    *
    * @param val {STATE_TYPE} contents
    * @returns void
    */
    void setStateType(const STATE_TYPE StateType)
    {
        mStateType = StateType;
    }

	/*
	* @deconstructor
	*/
    virtual ~ChatState()
    {}

    static std::string convertStateToString(const STATE_TYPE StateType)
    {
        std::string strState = "unknown";
        switch(StateType)
        {
        case INACTIVE:
            strState = "inactive";
            break;
        case ACTIVE:
            strState = "active";
            break;
        case COMPOSING:
            strState = "composing";
            break;
        case PAUSED:
            strState = "paused";
            break;
        case GONE:
            strState = "gone";
            break;
        }
        return strState;
    }

    static STATE_TYPE convertStringToState(const std::string& strState)
    {
        STATE_TYPE state = INACTIVE;
        if (strState.compare("inactive") == 0) {
            state = INACTIVE;
        } else if (strState.compare("active") == 0) {
            state = ACTIVE;
        } else if (strState.compare("composing") == 0) {
            state = COMPOSING;
        } else if (strState.compare("paused") == 0) {
            state = PAUSED;
        } else if (strState.compare("gone") == 0) {
            state = GONE;
        } else {
            assert(0);
        }
        return state;
    }
protected:
    STATE_TYPE mStateType;
};
}
#endif //KAREREEVENTOBJECTS_H
