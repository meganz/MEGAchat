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

class DelegateMegaChatCallListener extends MegaChatCallListener{

    MegaChatApiJava megaChatApi;
    MegaChatCallListenerInterface listener;

    DelegateMegaChatCallListener(MegaChatApiJava megaApi, MegaChatCallListenerInterface listener) {
        this.megaChatApi = megaApi;
        this.listener = listener;
    }

    MegaChatCallListenerInterface getUserListener() {
        return listener;
    }

    @Override
    public void onChatCallStart(MegaChatApi api, MegaChatCall call) {
        if (listener != null) {
            final MegaChatCall megaChatCall = call.copy();
            megaChatApi.runCallback(new Runnable() {
                public void run() {
                    listener.onChatCallStart(megaChatApi, megaChatCall);
                }
            });
        }
    }

    @Override
    public void onChatCallIncoming(MegaChatApi api, MegaChatCall call){
        if (listener != null) {
            final MegaChatCall megaChatCall = call.copy();
            megaChatApi.runCallback(new Runnable() {
                public void run() {
                    listener.onChatCallIncoming(megaChatApi, megaChatCall);
                }
            });
        }
    }

    @Override
    public void onChatCallStateChange(MegaChatApi api, MegaChatCall call){
        if (listener != null) {
            final MegaChatCall megaChatCall = call.copy();
            megaChatApi.runCallback(new Runnable() {
                public void run() {
                    listener.onChatCallStateChange(megaChatApi, megaChatCall);
                }
            });
        }
    }

    @Override
    public void onChatCallTemporaryError(MegaChatApi api, MegaChatCall call, MegaChatError error){
        if (listener != null) {
            final MegaChatCall megaChatCall = call.copy();
            final MegaChatError megaChatError = error.copy();
            megaChatApi.runCallback(new Runnable() {
                public void run() {
                    listener.onChatCallTemporaryError(megaChatApi, megaChatCall, megaChatError);
                }
            });
        }
    }

    @Override
    public void onChatCallFinish(MegaChatApi api, MegaChatCall call, MegaChatError error){

        if (listener != null) {
            final MegaChatCall megaChatCall = call.copy();
            final MegaChatError megaChatError = error.copy();
            megaChatApi.runCallback(new Runnable() {
                public void run() {
                    listener.onChatCallFinish(megaChatApi, megaChatCall, megaChatError);
                }
            });
        }
    }
}