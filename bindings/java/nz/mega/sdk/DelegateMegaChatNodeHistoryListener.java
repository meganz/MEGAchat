package nz.mega.sdk;

/**
 * Interface to receive information about node history of a chatroom.
 *
 * A pointer to an implementation of this interface is required when calling MegaChatApi::openNodeHistory.
 * When node history of a chatroom is closed (MegaChatApi::closeNodeHistory), the listener is automatically removed.
 * You can also register additional listeners by calling MegaChatApi::addNodeHistoryListener and remove them
 * by using MegaChatApi::removeNodeHistoryListener
 *
 * The implementation will receive callbacks from an internal worker thread.
 */
class DelegateMegaChatNodeHistoryListener extends MegaChatNodeHistoryListener {

    MegaChatApiJava megaChatApi;
    MegaChatNodeHistoryListenerInterface listener;

    DelegateMegaChatNodeHistoryListener(MegaChatApiJava megaApi, MegaChatNodeHistoryListenerInterface listener) {
        this.megaChatApi = megaApi;
        this.listener = listener;
    }

    MegaChatNodeHistoryListenerInterface getUserListener() {
        return listener;
    }

    /**
     * This function is called when new attachment messages are loaded
     *
     * You can use MegaChatApi::loadAttachments to request loading messages.
     *
     * When there are no more message to load from the source reported by MegaChatApi::loadAttachments or
     * there are no more history at all, this function is also called, but the second parameter will be NULL.
     *
     * The SDK retains the ownership of the MegaChatMessage in the second parameter. The MegaChatMessage
     * object will be valid until this function returns. If you want to save the MegaChatMessage object,
     * use MegaChatMessage::copy for the message.
     *
     * @param api MegaChatApi connected to the account
     * @param msg The MegaChatMessage object, or NULL if no more history available.
     */
    @Override
    public void onAttachmentLoaded(MegaChatApi api, MegaChatMessage msg){
        if (listener != null) {
            if(msg!=null){
                final MegaChatMessage megaChatMessage = msg.copy();
                megaChatApi.runCallback(new Runnable() {
                    public void run() {
                        listener.onAttachmentLoaded(megaChatApi, megaChatMessage);
                    }
                });
            }
            else{
                megaChatApi.runCallback(new Runnable() {
                    public void run() {
                        listener.onAttachmentLoaded(megaChatApi, null);
                    }
                });
            }
        }
    }

    /**
     * This function is called when a new attachment message is received
     *
     * The SDK retains the ownership of the MegaChatMessage in the second parameter. The MegaChatMessage
     * object will be valid until this function returns. If you want to save the MegaChatMessage object,
     * use MegaChatMessage::copy for the message.
     *
     * @param api MegaChatApi connected to the account
     * @param msg MegaChatMessage representing the received message
     */
    @Override
    public void onAttachmentReceived(MegaChatApi api, MegaChatMessage msg){
        if (listener != null) {
            final MegaChatMessage megaChatMessage = msg.copy();
            megaChatApi.runCallback(new Runnable() {
                public void run() {
                    listener.onAttachmentReceived(megaChatApi, megaChatMessage);
                }
            });
        }
    }

    /**
     * This function is called when an attachment message is deleted
     *
     * @param api MegaChatApi connected to the account
     * @param msgid id of the message that has been deleted
     */
    @Override
    public void onAttachmentDeleted(MegaChatApi api, long msgid){
        if (listener != null) {
            final long msgId = msgid;
            megaChatApi.runCallback(new Runnable() {
                public void run() {
                    listener.onAttachmentDeleted(megaChatApi, msgId);
                }
            });
        }
    }

    /**
     * This function is called when history is trucated
     *
     * If no messages are left in the node-history, the msgid will be MEGACHAT_INVALID_HANDLE.
     *
     * @param api MegaChatApi connected to the account
     * @param msgid id of the message from which history has been trucated
     */
    @Override
    public void onTruncate(MegaChatApi api, long msgid) {
        if (listener != null) {
            final long msgId = msgid;
            megaChatApi.runCallback(new Runnable() {
                public void run() {
                    listener.onTruncate(megaChatApi, msgId);
                }
            });
        }
    }
}