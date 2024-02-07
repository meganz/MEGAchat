#include "mclc_autocompletion.h"

#include "mclc_globals.h"
#include "mclc_commands.h"

namespace mclc::clc_ac
{

using namespace mclc::clc_cmds;

bool extractflag(const std::string& flag, std::vector<ac::ACState::quoted_word>& words)
{
    for (auto i = words.begin(); i != words.end(); ++i)
    {
        if (i->s == flag && !i->q.quoted)
        {
            words.erase(i);
            return true;
        }
    }
    return false;
}

bool extractflagparam(const std::string& flag, std::string& param, std::vector<ac::ACState::quoted_word>& words)
{
    for (auto i = words.begin(); i != words.end(); ++i)
    {
        if (i->s == flag)
        {
            auto j = i;
            ++j;
            if (j != words.end())
            {
                param = j->s;
                words.erase(i, ++j);
                return true;
            }
        }
    }
    return false;
}

#ifndef NO_READLINE
#ifdef HAVE_AUTOCOMPLETE
char* longestCommonPrefix(ac::CompletionState& acs)
{
    std::string s = acs.completions[0].s;
    for (size_t i = acs.completions.size(); i--; )
    {
        for (unsigned j = 0; j < s.size() && j < acs.completions[i].s.size(); ++j)
        {
            if (s[j] != acs.completions[i].s[j])
            {
                s.erase(j, std::string::npos);
                break;
            }
        }
    }
    return _strdup(s.c_str());
}

char** my_rl_completion(const char *, int , int end)
{
    rl_attempted_completion_over = 1;

    std::string line(rl_line_buffer, static_cast<size_t>(end));
    ac::CompletionState acs = ac::autoComplete(line, line.size(), clc_global::g_autocompleteTemplate, true);

    if (acs.completions.empty())
    {
        return NULL;
    }

    if (acs.completions.size() == 1 && !acs.completions[0].couldExtend)
    {
        acs.completions[0].s += " ";
    }

    char** result = (char**)malloc((sizeof(char*)*(2 + acs.completions.size())));
    for (size_t i = acs.completions.size(); i--; )
    {
        result[i + 1] = _strdup(acs.completions[i].s.c_str());
    }
    result[acs.completions.size() + 1] = NULL;
    result[0] = longestCommonPrefix(acs);
    //for (int i = 0; i <= acs.completions.size(); ++i)
    //{
    //    cout << "i " << i << ": " << result[i] << endl;
    //}
    rl_completion_suppress_append = true;
    rl_basic_word_break_characters = " \r\n";
    rl_completer_word_break_characters = _strdup(" \r\n");
    rl_completer_quote_characters = "";
    rl_special_prefixes = "";
    return result;
}
#endif
#endif

ac::ACN autocompleteSyntax()
{
    using namespace ac;
    std::unique_ptr<Either> p(new Either("      "));

    p->Add(exec_initanonymous, sequence(text("initanonymous")));
    p->Add(exec_login,      sequence(text("login"), either(sequence(param("email"), opt(param("password"))), param("session"), sequence(text("autoresume"), opt(param("id"))) )));
    p->Add(exec_logout, sequence(text("logout")));
    p->Add(exec_session,    sequence(text("session"), opt(sequence(text("autoresume"), opt(param("id")))) ));
    p->Add(exec_debug,
            sequence(
                text("debug"),
                opt(either(
                        flag("-noconsole"),
                        sequence(
                            flag("-console"),
                            either(text("info"), text("debug"), text("warning"), text("error"), text("all"))
                            )
                        )
                    ),
                opt(either(
                        flag("-nofile"),
                        sequence(
                            flag("-file"),
                            either(text("info"), text("debug"), text("warning"), text("error"), text("all")),
                            localFSFile("log_file_name")
                            )
                        )
                    )
                )
            );

    p->Add(exec_easy_debug, sequence(text("easy_debug"), localFSFile("log_file_name")));
    p->Add(exec_setonlinestatus,    sequence(text("setonlinestatus"), either(text("offline"), text("away"), text("online"), text("busy"))));
    p->Add(exec_setpresenceautoaway, sequence(text("setpresenceautoaway"), either(text("on"), text("off")), wholenumber(30)));
    p->Add(exec_setpresencepersist, sequence(text("setpresencepersist"), either(text("on"), text("off"))));
    p->Add(exec_signalpresenceperiod, sequence(text("signalpresenceperiod"), wholenumber(5)));
    p->Add(exec_getonlinestatus, sequence(text("getonlinestatus")));

    p->Add(exec_getuserfirstname,   sequence(text("getuserfirstname"), param("userid")));
    p->Add(exec_getuserlastname,    sequence(text("getuserlastname"), param("userid")));
    p->Add(exec_getuseremail,       sequence(text("getuseremail"), param("userid")));
    p->Add(exec_getcontactemail,    sequence(text("getcontactemail"), param("userid")));
    p->Add(exec_getuserhandlebyemail, sequence(text("getuserhandlebyemail"), param("email")));
    p->Add(exec_getmyuserhandle,      sequence(text("getmyuserhandle")));
    p->Add(exec_getmyfirstname,     sequence(text("getmyfirstname")));
    p->Add(exec_getmylastname,      sequence(text("getmylastname")));
    p->Add(exec_getmyfullname,      sequence(text("getmyfullname")));
    p->Add(exec_getmyemail,         sequence(text("getmyemail")));

    p->Add(exec_getchatrooms,       sequence(text("getchatrooms")));
    p->Add(exec_getchatroom,        sequence(text("getchatroom"), param("roomid")));
    p->Add(exec_getchatroombyuser,  sequence(text("getchatroombyuser"), param("userid")));
    p->Add(exec_getchatlistitems,   sequence(text("getchatlistitems")));
    p->Add(exec_getchatlistitem,    sequence(text("getchatlistitem"), param("roomid")));
    p->Add(exec_getunreadchats,     sequence(text("getunreadchats"), param("roomid")));
    p->Add(exec_getinactivechatlistitems, sequence(text("getinactivechatlistitems"), param("roomid")));
    p->Add(exec_getunreadchatlistitems, sequence(text("getunreadchatlistitems"), param("roomid")));
    p->Add(exec_getchathandlebyuser, sequence(text("getchathandlebyuser"), param("userid")));
    p->Add(exec_chatinfo,           sequence(text("chatinfo"), opt(param("roomid"))));

    p->Add(exec_createchat,         sequence(text("createchat"), opt(flag("-group")), opt(flag("-public")), opt(flag("-meeting")), repeat(param("userid"))));
    p->Add(exec_invitetochat,       sequence(text("invitetochat"), param("roomid"), param("userid")));
    p->Add(exec_removefromchat,     sequence(text("removefromchat"), param("roomid"), param("userid")));
    p->Add(exec_leavechat,          sequence(text("leavechat"), param("roomid")));
    p->Add(exec_updatechatpermissions, sequence(text("updatechatpermissions"), param("roomid"), param("userid")));
    p->Add(exec_truncatechat,       sequence(text("truncatechat"), param("roomid"), param("msgid")));
    p->Add(exec_clearchathistory,   sequence(text("clearchathistory"), param("roomid")));
    p->Add(exec_setchattitle,       sequence(text("setchattitle"), param("roomid"), param("title")));
    p->Add(exec_setRetentionTime,   sequence(text("setretentiontime"), param("roomid"), param("period")));
    p->Add(exec_getRetentionTime,   sequence(text("getretentiontime"), param("roomid")));

    p->Add(exec_openchatroom,       sequence(text("openchatroom"), param("roomid")));
    p->Add(exec_closechatroom,      sequence(text("closechatroom"), param("roomid")));
    p->Add(exec_loadmessages,       sequence(text("loadmessages"), param("roomid"), wholenumber(10), opt(either(text("human"), text("developer")))));
    p->Add(exec_reviewpublicchat,   sequence(text("rpc"), param("chatlink"), opt(wholenumber(5000))));
    p->Add(exec_isfullhistoryloaded, sequence(text("isfullhistoryloaded"), param("roomid")));
    p->Add(exec_getmessage,         sequence(text("getmessage"), param("roomid"), param("msgid")));
    p->Add(exec_getmanualsendingmessage, sequence(text("getmanualsendingmessage"), param("roomid"), param("tempmsgid")));
    p->Add(exec_sendmessage,        sequence(text("sendmessage"), param("roomid"), param("text")));
    p->Add(exec_attachcontacts,     sequence(text("attachcontacts"), param("roomid"), repeat(param("userid"))));
    p->Add(exec_attachnode,         sequence(text("attachnode"), param("roomid"), param("nodeid")));
    p->Add(exec_revokeattachmentmessage, sequence(text("revokeattachmentmessage"), param("roomid"), param("msgid")));
    p->Add(exec_editmessage,        sequence(text("editmessage"), param("roomid"), param("msgid"), param("text")));
    p->Add(exec_setmessageseen,     sequence(text("setmessageseen"), param("roomid"), param("msgid")));
    p->Add(exec_getLastMessageSeen, sequence(text("getLastMessageSeen"), param("roomid")));
    p->Add(exec_removeunsentmessage, sequence(text("removeunsentmessage"), param("roomid"), param("tempid")));
    p->Add(exec_sendtypingnotification, sequence(text("sendtypingnotification"), param("roomid")));
    p->Add(exec_ismessagereceptionconfirmationactive, sequence(text("ismessagereceptionconfirmationactive")));
    p->Add(exec_savecurrentstate, sequence(text("savecurrentstate")));

    p->Add(exec_openchatpreview,    sequence(text("openchatpreview"), param("chatlink")));
    p->Add(exec_closechatpreview,   sequence(text("closechatpreview"), param("chatid")));

    p->Add(exec_joinCallViaMeetingLink,
            sequence(
                text("joinCallViaMeetingLink"),
                opt(flag("-novideo")),
                opt(flag("-noaudio")),
                opt(sequence(flag("-wait"), param("timeSeconds"))),
                opt(sequence(flag("-videoInputDevice"), param("videoDevice"))),
                param("meetingLink"))
            );

    p->Add(exec_dumpchathistory,   sequence(text("dumpchathistory"), param("roomid"), param("fileName")));

#ifndef KARERE_DISABLE_WEBRTC
    p->Add(exec_getchatvideoindevices, sequence(text("getchatvideoindevices")));
    p->Add(exec_setchatvideoindevice, sequence(text("setchatvideoindevice"), param("device")));
    p->Add(exec_startchatcall, sequence(text("startchatcall"), param("roomid"), opt(either(text("true"), text("false")))));
    p->Add(exec_answerchatcall, sequence(text("answerchatcall"), param("roomid"), opt(either(text("true"), text("false")))));
    p->Add(exec_hangchatcall, sequence(text("hangchatcall"), param("callid")));
    p->Add(exec_enableaudio, sequence(text("enableaudio"), param("roomid")));
    p->Add(exec_disableaudio, sequence(text("disableaudio"), param("roomid")));
    p->Add(exec_enablevideo, sequence(text("enablevideo"), param("roomid")));
    p->Add(exec_disablevideo, sequence(text("disablevideo"), param("roomid")));
    p->Add(exec_getchatcall, sequence(text("getchatcall"), param("roomid")));
    p->Add(exec_setignoredcall, sequence(text("setignoredcall"), param("roomid")));
    p->Add(exec_getchatcallbycallid, sequence(text("getchatcallbycallid"), param("callid")));
    p->Add(exec_getnumcalls, sequence(text("getnumcalls")));
    p->Add(exec_getchatcalls, sequence(text("getchatcalls")));
    p->Add(exec_getchatcallsids, sequence(text("getchatcallsids")));
#endif

    p->Add(exec_detail,     sequence(text("detail"), opt(either(text("high"), text("low")))));
#ifdef WIN32
    p->Add(exec_dos_unix,   sequence(text("autocomplete"), opt(either(text("unix"), text("dos")))));
#endif
    p->Add(exec_help,       sequence(either(text("help"), text("?"))));
#ifdef WIN32
    p->Add(exec_history,    sequence(text("history")));
#endif
    p->Add(exec_repeat,     sequence(text("repeat"), wholenumber(5), param("command")));
    p->Add(exec_quit,       sequence(either(text("quit"), text("q"))));
    p->Add(exec_quit,       sequence(text("exit")));

    // sdk level commands (intermediate layer of megacli commands)
    p->Add(exec_catchup, sequence(text("catchup"), opt(wholenumber(3))));
    p->Add(exec_smsverify, sequence(text("smsverify"), either(sequence(text("send"), param("phoneNumber"), opt(text("to"))), sequence(text("code"), param("code")), text("allowed"), text("phone"))));
    p->Add(exec_apiurl, sequence(text("apiurl"), param("url"), opt(param("disablepkp"))));
    p->Add(exec_getaccountachievements, sequence(text("getaccountachievements")));
    p->Add(exec_getmegaachievements, sequence(text("getmegaachievements")));

    p->Add(exec_recentactions, sequence(text("recentactions"), opt(sequence(param("days"), param("nodecount")))));
    p->Add(exec_getspecificaccountdetails, sequence(text("getspecificaccountdetails"), repeat(either(flag("-storage"), flag("-transfer"), flag("-pro")))));


    p->Add(exec_backgroundupload, sequence(text("backgroundupload"), either(
        sequence(text("new"), param("name")),
        sequence(text("resume"), param("name"), param("serializeddata")),
        sequence(text("analyse"), param("name"), localFSFile()),
        sequence(text("encrypt"), param("name"), localFSFile(), localFSFile(), param("startPos"), param("length"), either(text("false"), text("true"))),
        sequence(text("geturl"), param("name"), param("filesize")),
        sequence(text("serialize"), param("name")),
        sequence(text("upload"), param("url"), localFSFile()),
        sequence(text("putthumbnail"), param("name"), localFSFile()),
        sequence(text("putpreview"), param("name"), localFSFile()),
        sequence(text("setthumbnail"), param("name"), param("handle")),
        sequence(text("setpreview"), param("name"), param("handle")),
        sequence(text("setcoordinates"), param("name"), opt(flag("-shareable")), param("latitude"), param("longitude")),
        sequence(text("complete"), param("name"), param("nodename"), param("remoteparentpath"), param("fingerprint"), param("originalfingerprint"), param("uploadtoken")))));

    p->Add(exec_ensuremediainfo, sequence(text("ensuremediainfo")));

    p->Add(exec_getfingerprint, sequence(text("getfingerprint"), either(
        sequence(text("local"), localFSFile()),
        sequence(text("remote"), param("remotefile")),
        sequence(text("original"), param("remotefile")))));

    p->Add(exec_setthumbnailbyhandle, sequence(text("setthumbnailbyhandle"), param("remotepath"), param("attributehandle")));
    p->Add(exec_setpreviewbyhandle, sequence(text("setpreviewbyhandle"), param("remotepath"), param("attributehandle")));
    p->Add(exec_setnodecoordinates, sequence(text("setnodecoordinates"), param("remotepath"), param("latitude"), param("longitude")));
    p->Add(exec_setunshareablenodecoordinates, sequence(text("setunshareablenodecoordinates"), param("remotepath"), param("latitude"), param("longitude")));
    p->Add(exec_createthumbnail, sequence(text("createthumbnail"), opt(flag("-tempmegaapi")), opt(sequence(flag("-parallel"), param("count"))), localFSFile(), localFSFile()));
    p->Add(exec_createpreview, sequence(text("createpreview"), localFSFile(), localFSFile()));
    p->Add(exec_getthumbnail, sequence(text("getthumbnail"), param("node"), localFSFile()));
    p->Add(exec_cancelgetthumbnail, sequence(text("cancelgetthumbnail"), param("node")));
    p->Add(exec_getpreview, sequence(text("getpreview"), param("node"), localFSFile()));
    p->Add(exec_cancelgetpreview, sequence(text("cancelgetpreview"), param("node")));
    p->Add(exec_testAllocation, sequence(text("testAllocation"), param("count"), param("size")));
    p->Add(exec_getnodebypath, sequence(text("getnodebypath"), param("remotepath")));
    p->Add(exec_ls, sequence(text("ls"), repeat(either(flag("-recursive"), flag("-handles"), flag("-ctime"), flag("-mtime"), flag("-size"), flag("-versions"), sequence(flag("-order"), param("order")), sequence(flag("-refilter"), param("regex")))), param("path")));
    p->Add(exec_createfolder, sequence(text("createfolder"), param("name"), param("remotepath")));
    p->Add(exec_remove, sequence(text("remove"), param("remotepath")));
    p->Add(exec_renamenode, sequence(text("renamenode"), param("remotepath"), param("newname")));
    p->Add(exec_setmaxuploadspeed, sequence(text("setmaxuploadspeed"), param("bps")));
    p->Add(exec_setmaxdownloadspeed, sequence(text("setmaxdownloadspeed"), param("bps")));
    p->Add(exec_startupload, sequence(text("startupload"), localFSPath(), param("remotepath"), opt(flag("-withcanceltoken")), opt(flag("-logstage")), opt(sequence(flag("-filename"), param("newname")))));
    p->Add(exec_startdownload, sequence(text("startdownload"), param("remotepath"), localFSPath(), opt(flag("-withcanceltoken")), (flag("-logstage"))));
    p->Add(exec_pausetransfers, sequence(text("pausetransfers"), param("pause"), opt(param("direction"))));
    p->Add(exec_pausetransferbytag, sequence(text("exec_pausetransferbytag"), param("tag"), param("pause")));
    p->Add(exec_canceltransfers, sequence(text("canceltransfers"), param("direction")));
    p->Add(exec_canceltransferbytag, sequence(text("canceltransferbytag"), param("tag")));
    p->Add(exec_gettransfers, sequence(text("gettransfers"), param("type")));

    p->Add(exec_cancelbytoken, sequence(text("cancelbytoken"), opt(param("token-id"))));

    p->Add(exec_exportNode, sequence(text("exportnode"), opt(sequence(flag("-writable"), either(text("true"), text("false")))), opt(sequence(flag("-expiry"), param("time_t"))), param("remotepath")));


    p->Add(exec_pushreceived, sequence(text("pushreceived"), opt(flag("-beep")), opt(param("chatid"))));
    p->Add(exec_getcloudstorageused, sequence(text("getcloudstorageused")));

    p->Add(exec_cp, sequence(text("cp"), param("remotesrc"), param("remotedst")));
    p->Add(exec_mv, sequence(text("mv"), param("remotesrc"), param("remotedst"), opt(sequence(flag("-rename"), param("newname")))));

    p->Add(exec_setCameraUploadsFolder, sequence(text("setcamerauploadsfolder"), param("remotedst")));
    p->Add(exec_getCameraUploadsFolder, sequence(text("getcamerauploadsfolder")));
    p->Add(exec_setCameraUploadsFolderSecondary, sequence(text("setcamerauploadsfoldersecondary"), param("remotedst")));
    p->Add(exec_getCameraUploadsFolderSecondary, sequence(text("getcamerauploadsfoldersecondary")));

    p->Add(exec_getContact, sequence(text("getcontact"), param("email")));

    p->Add(exec_getDefaultTZ, sequence(text("getdefaulttz")));
    p->Add(exec_isGeolocOn, sequence(text("isgeolocationenabled")));
    p->Add(exec_setGeolocOn, sequence(text("setgeolocation"), text("on")));


    // Helpers
    p->Add(exec_treecompare, sequence(text("treecompare"), localFSPath(), param("remotepath")/*remoteFSPath(client, &cwd)*/));
    p->Add(exec_generatetestfilesfolders, sequence(text("generatetestfilesfolders"), repeat(either(sequence(flag("-folderdepth"), param("depth")),
        sequence(flag("-folderwidth"), param("width")),
        sequence(flag("-filecount"), param("count")),
        sequence(flag("-filesize"), param("size")),
        sequence(flag("-nameprefix"), param("prefix")))), localFSFolder("parent")));


    p->Add(exec_syncadd,
        sequence(text("sync"),
            text("add"),
            opt(flag("-backup")),
            opt(sequence(flag("-external"), param("drivePath"))),
            localFSFolder("source"),
            param("remotetarget")));

    p->Add(exec_syncclosedrive,
           sequence(text("sync"),
                    text("closedrive"),
                    localFSFolder("drive")));

    p->Add(exec_syncexport,
           sequence(text("sync"),
                    text("export"),
                    opt(localFSFile("outputFile"))));

    p->Add(exec_syncimport,
           sequence(text("sync"),
                    text("import"),
                    localFSFile("inputFile")));

    p->Add(exec_syncopendrive,
        sequence(text("sync"),
            text("opendrive"),
            localFSFolder("drive")));

    p->Add(exec_synclist,
        sequence(text("sync"), text("list")));

    p->Add(exec_syncremove,
        sequence(text("sync"),
            text("remove"),
            either(
                sequence(flag("-id"), param("backupId")),
                sequence(flag("-path"), param("targetpath")))));

    p->Add(exec_syncxable,
           sequence(text("sync"),
                    either(text("disable"), text("enable")),
                    param("id")));

    p->Add(exec_setmybackupsfolder, sequence(text("setmybackupsfolder"), param("remotefolder")));
    p->Add(exec_getmybackupsfolder, sequence(text("getmybackupsfolder")));

    return p;
}

}
