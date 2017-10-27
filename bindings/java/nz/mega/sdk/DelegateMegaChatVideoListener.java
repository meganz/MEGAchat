/*
 * (c) 2013-2015 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,\
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * @copyright Simplified (2-clause) BSD License.
 * You should have received a copy of the license along with this
 * program.
 */
package nz.mega.sdk;

class DelegateMegaChatVideoListener extends MegaChatVideoListener{

    MegaChatApiJava megaChatApi;
    MegaChatVideoListenerInterface listener;
    boolean remote;
    boolean removed;

    DelegateMegaChatVideoListener(MegaChatApiJava megaApi, MegaChatVideoListenerInterface listener, boolean remote) {
        this.megaChatApi = megaApi;
        this.listener = listener;
        this.remote = remote;
        this.removed = false;
    }

    MegaChatVideoListenerInterface getUserListener() {
        return listener;
    }

    boolean isRemote() {
        return remote;
    }

    void setRemoved() {
        removed = true;
    }

    @Override
    public void onChatVideoData(MegaChatApi api, long chatid, int width, int height, byte[] byteBuffer)
    {
        if (listener != null) {
            // Uncomment this if it's needed to send the callback to the GUI thread
            // Initially disabled for performance

            //final MegaChatCall megaChatCall = chatCall.copy();
            //final byte[] megaByteBuffer = byteBuffer.clone();
            //megaChatApi.runCallback(new Runnable() {
            //    public void run() {
                    if (!removed) {
                        listener.onChatVideoData(megaChatApi, chatid, width, height, byteBuffer);
                    }
            //    }
            //});
        }
    }
}
