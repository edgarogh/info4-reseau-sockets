drop table if exists twiiiiits;
drop table if exists followings;
drop table if exists users;

pragma foreign_keys = on;

create table users (
    name text(6) not null primary key,
    last_online long not null
);

create table followings (
    follower text(6) not null references users (name) on delete cascade,
    followee text(6) not null references users (name) on delete cascade,

    unique (follower, followee)
);

create table twiiiiits (
    long date not null,
    author text(6) not null references users (name) on delete cascade,
    message text(20) not null
);
