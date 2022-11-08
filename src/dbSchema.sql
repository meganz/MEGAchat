CREATE TABLE sending(rowid integer primary key autoincrement, msgid int64, keyid int,
    chatid int64 not null, type tinyint, ts int, updated smallint, msg blob,
    opcode smallint not null, msg_cmd blob, key_cmd blob, recipients blob not null,
    backrefid int64 not null, backrefs blob);

CREATE TABLE manual_sending(rowid integer primary key autoincrement, msgid int64,
    chatid int64 not null, type tinyint, ts int, updated smallint, msg blob,
    opcode smallint not null, reason smallint not null);

CREATE TABLE vars(name text not null primary key, value blob);

CREATE TABLE chats(chatid int64 unique primary key, shard tinyint,
    own_priv tinyint, peer int64 default -1, peer_priv tinyint default 0,
    title text, ts_created int64 not null default 0,
    last_seen int64 default 0, last_recv int64 default 0, archived tinyint default 0,
    mode tinyint default 0, unified_key blob, rsn blob, meeting tinyint default 0, chat_options tinyint default 0);

CREATE TABLE contacts(userid int64 PRIMARY KEY, email text, visibility int,
    since int64 not null default 0);

CREATE TABLE userattrs(userid int64 not null, type tinyint not null, data blob,
    err tinyint default 0, ts int default (cast(strftime('%s', 'now') as int)),
    UNIQUE(userid, type) ON CONFLICT REPLACE);

CREATE TABLE chat_peers(chatid int64 not null, userid int64, priv tinyint,
    UNIQUE(chatid, userid));

CREATE TABLE chat_vars(chatid int64 not null, name text not null, value text,
    UNIQUE(chatid, name));

CREATE TABLE history(idx int not null, chatid int64 not null, msgid int64 not null,
    userid int64, keyid int not null, type tinyint, updated smallint, ts int,
    is_encrypted tinyint, data blob, backrefid int64 not null, UNIQUE(chatid,msgid), UNIQUE(chatid,idx));

CREATE TABLE sendkeys(chatid int64 not null, userid int64 not null, keyid int32 not null, key blob not null,
    ts int not null, UNIQUE(chatid, userid, keyid));

CREATE TABLE node_history(idx int not null, chatid int64 not null, msgid int64 not null,
    userid int64, keyid int not null, type tinyint, updated smallint, ts int,
    is_encrypted tinyint, data blob, backrefid int64 not null, UNIQUE(chatid,msgid), UNIQUE(chatid,idx));

CREATE TABLE dns_cache(shard tinyint primary key, url text, ipv4 text, ipv6 text, sess_data blob);

CREATE TABLE chat_reactions(chatid int64 not null, msgid int64 not null, userid int64 not null, reaction text,
    UNIQUE(chatid, msgid, userid, reaction), FOREIGN KEY(chatid, msgid) REFERENCES history(chatid, msgid) ON DELETE CASCADE);

CREATE TABLE chat_pending_reactions(chatid int64 not null, msgid int64 not null, reaction text, encReaction blob,
    status tinyint, UNIQUE(chatid, msgid, reaction), FOREIGN KEY(chatid, msgid) REFERENCES history(chatid, msgid) ON DELETE CASCADE);

CREATE TABLE scheduledMeetings(schedid int64 unique primary key, chatid int64, organizerid int64, parentschedid int64, timezone text,
    startdatetime text, enddatetime text, title text, description text, attributes text, overrides text, cancelled tinyint default 0,
    flags int64 default 0, rules blob);

CREATE TABLE scheduledMeetingsOccurr(schedid int64, startdatetime text, enddatetime text, PRIMARY KEY (schedid, startdatetime),
    FOREIGN KEY(schedid) REFERENCES scheduledMeetings(schedid) ON DELETE CASCADE);
