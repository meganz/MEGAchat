package nz.mega.sdk;

public interface MegaChatScheduledMeetingListenerInterface {
    void onChatSchedMeetingUpdate(MegaChatApiJava api, MegaChatScheduledMeeting scheduledMeeting);
    void onSchedMeetingOccurrencesUpdate(MegaChatApiJava api, long chatid);
}