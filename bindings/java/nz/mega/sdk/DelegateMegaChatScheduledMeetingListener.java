package nz.mega.sdk;

class DelegateMegaChatScheduledMeetingListener extends MegaChatScheduledMeetingListener{

    MegaChatApiJava megaChatApi;
    MegaChatScheduledMeetingListenerInterface listener;

    DelegateMegaChatScheduledMeetingListener(MegaChatApiJava megaApi, MegaChatScheduledMeetingListenerInterface listener) {
        this.megaChatApi = megaApi;
        this.listener = listener;
    }

    MegaChatScheduledMeetingListenerInterface getUserListener() {
        return listener;
    }

    @Override
    public void onChatSchedMeetingUpdate(MegaChatApi api, MegaChatScheduledMeeting scheduledMeeting) {
        if (listener != null) {
            final MegaChatScheduledMeeting megaScheduledMeeting = scheduledMeeting.copy();
            megaChatApi.runCallback(() -> listener.onChatSchedMeetingUpdate(megaChatApi, megaScheduledMeeting));
        }
    }

    @Override
    public void onSchedMeetingOccurrencesUpdate(MegaChatApi api, long chatid, boolean append) {
        if (listener != null) {
            megaChatApi.runCallback(() -> listener.onSchedMeetingOccurrencesUpdate(megaChatApi, chatid, append));
        }
    }
}
