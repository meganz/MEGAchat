#ifndef IJINGLESESSION_H
#define IJINGLESESSION_H

namespace rtcModule
{
/** audio&video enabled flags used in many places */
struct AvFlags
{
    bool audio = false;
    bool video = false;
    AvFlags(){}
    AvFlags(bool a, bool v): audio(a), video(v){}
};

/** This is the interface of the session object exposed to the application via events etc */
struct IJingleSession
{
    typedef void(*DeleteFunc)(void*);

    virtual const char* getSid() const = 0;
    virtual const char* getJid() const = 0;
    virtual const char* getPeerJid() const = 0;
    virtual bool isCaller() const = 0;
    virtual const char* getPeerAnonId() const = 0;
    virtual const char* getCallId() const = 0;
/** Returns whether the session's udnerlying media connection is relayed via a TURN server or not
 * @return -1 if unknown, 1 if true, 0 if false
 */
    virtual int isRelayed() const = 0;
    virtual void setUserData(void*, DeleteFunc delFunc) = 0;
    virtual void* getUserData() const = 0;
    virtual bool isRealSession() const {return true;}
};
}

#endif // IJINGLESESSION_H
