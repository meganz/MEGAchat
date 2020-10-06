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

class DelegateMegaChatRoomListener extends MegaChatRoomListener {

    MegaChatApiJava megaChatApi;
    MegaChatRoomListenerInterface listener;
    boolean singleListener;

    DelegateMegaChatRoomListener(MegaChatApiJava megaApi, MegaChatRoomListenerInterface listener) {
        this.megaChatApi = megaApi;
        this.listener = listener;
        this.singleListener = true;
    }

    MegaChatRoomListenerInterface getUserListener() {
        return listener;
    }
    
    boolean invalidateUserListener() {
		if (listener != null) {
			listener = null;
			return true;
		}
		return false;
	}    

    @Override
    public void onChatRoomUpdate(MegaChatApi api, MegaChatRoom chat){
        if (listener != null) {
            final MegaChatRoom megaChatRoom = chat.copy();
            megaChatApi.runCallback(new Runnable() {
                public void run() {
					if (listener != null)
						listener.onChatRoomUpdate(megaChatApi, megaChatRoom);
                }
            });
        }
    }

    @Override
    public void onMessageLoaded(MegaChatApi api, MegaChatMessage msg){
        if (listener != null) {
            if(msg!=null){
                final MegaChatMessage megaChatMessage = msg.copy();
                megaChatApi.runCallback(new Runnable() {
                    public void run() {
						if (listener != null)
							listener.onMessageLoaded(megaChatApi, megaChatMessage);
                    }
                });
            }
            else{
                megaChatApi.runCallback(new Runnable() {
                    public void run() {
						if (listener != null)
							listener.onMessageLoaded(megaChatApi, null);
                    }
                });
            }
        }
    }

    @Override
    public void onMessageReceived(MegaChatApi api, MegaChatMessage msg){
        if (listener != null) {
            final MegaChatMessage megaChatMessage = msg.copy();
            megaChatApi.runCallback(new Runnable() {
                public void run() {
					if (listener != null)
						listener.onMessageReceived(megaChatApi, megaChatMessage);
                }
            });
        }
    }

    @Override
    public void onMessageUpdate(MegaChatApi api, MegaChatMessage msg){
        if (listener != null) {
            final MegaChatMessage megaChatMessage = msg.copy();
            megaChatApi.runCallback(new Runnable() {
                public void run() {
					if (listener != null)
						listener.onMessageUpdate(megaChatApi, megaChatMessage);
                }
            });
        }
    }

    @Override
    public void onHistoryReloaded(MegaChatApi api, MegaChatRoom chat) {
        if (listener != null) {
            final MegaChatRoom megaChatRoom = chat.copy();
            megaChatApi.runCallback(new Runnable() {
                public void run() {
					if (listener != null)
						listener.onHistoryReloaded(megaChatApi, megaChatRoom);
                }
            });
        }
    }

    @Override
    public void onReactionUpdate(MegaChatApi api, long msgid, String reaction, int count){
        if (listener != null) {
            megaChatApi.runCallback((Runnable) () -> {
                if (listener != null)
                    listener.onReactionUpdate(megaChatApi, msgid, reaction, count);
            });
        }
    }
}
