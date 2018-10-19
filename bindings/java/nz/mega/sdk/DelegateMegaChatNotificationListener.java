package nz.mega.sdk;

public class DelegateMegaChatNotificationListener extends MegaChatNotificationListener{

    MegaChatApiJava megaChatApi;
    MegaChatNotificationListenerInterface listener;

    DelegateMegaChatNotificationListener(MegaChatApiJava megaApi, MegaChatNotificationListenerInterface listener) {
        this.megaChatApi = megaApi;
        this.listener = listener;
    }

    MegaChatNotificationListenerInterface getUserListener() {
        return listener;
    }

    @Override
    public void onChatNotification(MegaChatApi api, long chatid, MegaChatMessage msg){
        if (listener != null) {
            final long chatId = chatid;
            final MegaChatMessage megaChatMessage = msg.copy();
            megaChatApi.runCallback(new Runnable() {
                public void run() {
                    listener.onChatNotification(megaChatApi, chatId, megaChatMessage);
                }
            });
        }
    }
}
