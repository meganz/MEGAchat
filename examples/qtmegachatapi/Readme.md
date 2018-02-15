# Example qtmegachatapi #

## Introduction ##

This project is a test project based on Mega chat application with support to WebRTC

## Install guide

The following steps will guide you to build MEGAchat library (including tests and Qt app).
In summary, you need to:
-Build WebRTC
- Clone MEGAchat
- Clone MEGA SDK in <MEGAchat>/third-party/

### Install depot tools
* git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
* export PATH=$PATH:/path/to/depot_tools

### Download WebRT
* mkdir webrtc
* cd webrtc
* fetch --nohooks webrtc
* gclient sync    (not sure if really needed)
* cd src
* git checkout c1a58bae4196651d2f7af183be187 webrtc build8bb00d45a57
* gclient sync
* ln -s <webrtc> $HOME/webrtc   (optional, you can download it directly at this path)

### Clone MEGAchat:
* git clone git@github.com:meganz/MEGAchat.git
* cd MEGAchat/src
* (note you need to install `libuv` in the system)

### Clone SDK:
* cd <MEGAchat>/third-party
* git clone git@github.com:meganz/sdk.git
* mv sdk mega
* cd mega
* ./autogen
* ./configure
* cd bindings/qt
* ./build_with_webrtc.sh all
* (note you need to install additional SDK dependencies, like libmediainfo and others)

  Now you're ready. Open <MEGAchat>/contrib/qt/MEGAchat.pro in QtCreator and hit Build :)
  You may need to change the "Build directory" in the project setting to <MEGAchat>/build if building complains about files not found.

## Known bugs

| Bug | Title                                                | Description                                                                                                                                                                |
|-----|------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| #1  | Chat list order                                      | The chat list is not ordered by last timestampt                                                                                                                            |
| #2  | Sending messages appear below manual sending mesages | If we have messages in manual sending mode and we tried to send a message without connection, the new message in sending status will appear below manual sending messages. |
| #3  | Truncate                                             | Sometimes truncate option doesn't work properly, check it.                                                                                                                 |
| #4  | Participants icon                                    | In group chats the participant icon doesn't appear, check the ui file.                                                                                                     |
| #5  | On resize chat list is repainted                     | If we have some chats hidden when we resize main window, the chat list is repainted                                                                                        |
| #6  | Chat window parent                                   | At this moment the Chatwindow parent is 0 so the window won't close when we close Mainwindow. We must set it's parent to Mainwindow.                                       |


## Pending Tasks

| Task         | Title                                             | Description                                                |
|--------------|---------------------------------------------------|------------------------------------------------------------|
| #1           | Add logic to manage message with an attached node | Add funcionality to handle a message with an attached node |
| #2           | Add videocalls                                    | Add videocalls funcionality                                |
| #3           | Add logout button                                 | Add logout button                                          |
| #4           | Send different messages types                     | Send messages of different types                           |
