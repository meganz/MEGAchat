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

import java.util.concurrent.atomic.AtomicInteger;

class DelegateMegaChatVideoListener extends MegaChatVideoListener{

    MegaChatApiJava megaChatApi;
    MegaChatVideoListenerInterface listener;
    AtomicInteger pendingFrames;
    boolean remote;
    boolean removed;

    DelegateMegaChatVideoListener(MegaChatApiJava megaApi, MegaChatVideoListenerInterface listener, boolean remote) {
        this.megaChatApi = megaApi;
        this.listener = listener;
        this.remote = remote;
        this.removed = false;
        this.pendingFrames = new AtomicInteger(0);
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
            int pending = pendingFrames.incrementAndGet();
            if (pending > 2)
            {
                pendingFrames.decrementAndGet();
                return;
            }

            final byte[] megaByteBuffer = byteBuffer;
            final long megaChatid = chatid;
            final int megaWidth = width;
            final int megaHeigth = height;
            final DelegateMegaChatVideoListener delegate = this;
            megaChatApi.runCallback(() -> {
                if (!delegate.removed) {
                    delegate.pendingFrames.decrementAndGet();
                    listener.onChatVideoData(megaChatApi, megaChatid, megaWidth, megaHeigth, megaByteBuffer);
                }
            });
        }
    }
}
