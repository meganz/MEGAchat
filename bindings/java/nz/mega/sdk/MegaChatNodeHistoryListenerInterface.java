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
public interface MegaChatNodeHistoryListenerInterface {
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
    public void onAttachmentLoaded(MegaChatApiJava api, MegaChatMessage msg);

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
    public void onAttachmentReceived(MegaChatApiJava api, MegaChatMessage msg);

    /**
     * This function is called when an attachment message is deleted
     *
     * @param api MegaChatApi connected to the account
     * @param msgid id of the message that has been deleted
     */
    public void onAttachmentDeleted(MegaChatApiJava api, long msgid);

    /**
     * This function is called when history is trucated
     *
     * If no messages are left in the node-history, the msgid will be MEGACHAT_INVALID_HANDLE.
     *
     * @param api MegaChatApi connected to the account
     * @param msgid id of the message from which history has been trucated
     */
    public void onTruncate(MegaChatApiJava api, long msgid);
}
