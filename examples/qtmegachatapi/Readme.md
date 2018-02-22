# Example qtmegachatapi #

## Introduction ##

This project is a test project based on Mega chat application with support to WebRTC

## Known bugs

| Bug | Title                                                | Description                                                                                                                                                                |
|-----|------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| #1  | Chat list order                                      | The chat list is not ordered by last timestampt                                                                                                                            |
| #2  | Sending messages appear below manual sending mesages | If we have messages in manual sending mode and we tried to send a message without connection, the new message in sending status will appear below manual sending messages. |
| #3  | Truncate                                             | Sometimes truncate option doesn't work properly, check it.                                                                                                                 |
| #4  | Participants icon                                    | In group chats the participant icon doesn't appear, check the ui file.                                                                                                     |
| #5  | On resize chat list is repainted                     | If we have some chats hidden when we resize main window, the chat list is repainted                                                                                        |
| #6  | Chat window parent                                   | At this moment the Chatwindow parent is 0 so the window won't close when we close Mainwindow. We must set it's parent to Mainwindow.                                       |
| #7  | Contacts doesn't appear when we create a new session | When we log in with (user+password), the app doesn't show contacts                                                                                                         |



## Pending Tasks

| Task         | Title                                             | Description                                                |
|--------------|---------------------------------------------------|------------------------------------------------------------|
| #1           | Add logic to manage message with an attached node | Add funcionality to handle a message with an attached node |
| #2           | Add videocalls                                    | Add videocalls funcionality                                |
| #3           | Add logout button                                 | Add logout button                                          |
| #4           | Send different messages types                     | Send messages of different types                           |
