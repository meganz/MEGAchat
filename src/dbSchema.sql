CREATE TABLE sending(rowid integer primary key autoincrement, edits int64, edits_is_xid tynyint, chatid int64 not null, ts int, data blob);
CREATE TABLE vars(name text not null primary key, value blob);
CREATE TABLE chats(chatid int64 unique primary key, url text, shard tinyint, own_priv tinyint, peer int64 default -1, peer_priv tinyint default 0, title text);
CREATE TABLE contacts(userid int64 PRIMARY KEY, email text);
CREATE TABLE history(idx int not null, chatid int64 not null, msgid int64 primary key, userid int64, ts int, data blob, edits int64);
CREATE TABLE userattrs(userid int64 not null, type tinyint not null, data blob, err tinyint default 0, ts int default (cast(strftime('%s', 'now') as int)), UNIQUE(userid, type) ON CONFLICT REPLACE);
CREATE TABLE chat_peers(chatid int64, userid int64, priv tinyint, UNIQUE(chatid, userid));
