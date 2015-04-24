/** Log channel configuration

KR_LOGGER_CONFIG(krLogChannel_<channel_id1>, krLogChannel_<channel_id2>,...)

//always configure the default channel. Normally it does not have a prefix displayed
     KR_LOGCHANNEL(default NULL Debug 0);
     KR_LOGCHANNEL(<channel_id1> "<prefix1>" <debug_level1> <channel_flags>);
     KR_LOGCHANNEL(<channel_id2> "<prefix2>" <debug_level2> <channel_flags>);
     ...
//optional settings
    flags = flags | krLogNoTimestamp; //modify global flags to suit your needs
    logToConsole(); //enable console logging, disabled by default
    logToFile("log.txt", <rotate_size>); //enable file logging, disabled by default
//end optional
KR_LOGGER_CONFIG_END()

The order of the KR_LOGCHANNEL does not have to be the same as the one in KR_LOGGER_CONFIG, but
all listed channels must be configured. If some is missed, this will result in an error message on the
console and application abort

channel_id in KR_LOGCHANNEL must match a krLogChannel_<channel_id> specified in KR_LOGGER_CONFIG
debug_level can be: Debug,Warn,Error,Off. Note that this is not in quites
prefix - the string that is prefixed before each log line for that channel. Can be NULL
channel_flags - currently only the lower 4 bits are used, which define the color of the messages in the console:
0-7 correspond to terminal escape codes \033[0;30m - \033[0;37m. These are dark colors
0-8 correspond to terminal escape codes \033[1;30m - \033[1;37m. These are bright colors
log_file - if not NULL, enables logging to that file.
rotate_size - the maximum size of the log file, in kbytes, after which the log file is truncated in half
*/

KR_LOGGER_CONFIG(krLogChannel_xmpp, krLogChannel_strophe, krLogChannel_rtcevent, krLogChannel_textchat)
    KR_LOGCHANNEL(default, NULL, Debug, 0);
    KR_LOGCHANNEL(xmpp, "xmpp", Info, krLogNoLevel|7);
    KR_LOGCHANNEL(strophe, "strophe", Debug, 0);
    KR_LOGCHANNEL(rtcevent, "rtc_event", Debug, 11);
    KR_LOGCHANNEL(textchat, "chat", Debug, 12);
    logToConsole();
    logToFile("log.txt", 500);
KR_LOGGER_CONFIG_END()
