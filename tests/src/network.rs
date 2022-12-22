use byteorder::{ReadBytesExt, WriteBytesExt, BE};
use std::fmt::{Debug, Formatter};
use std::io;
use std::io::{Cursor, ErrorKind, Read, Seek, SeekFrom, Write};

pub const IO_BUFFER_SIZE: usize = 48;
pub const USERNAME_MAX_LENGTH: usize = 6;
pub const MESSAGE_MAX_LENGTH: usize = 20;

pub type Frame = [u8; IO_BUFFER_SIZE];
pub const EMPTY_FRAME: Frame = [0; IO_BUFFER_SIZE];

macro_rules! match_variant {
    ($reader:ident {
        $($int_val:literal => $maps_to:expr,)*
    } $name:literal) => {match $reader.read_u32::<BE>()? {
        $($int_val => $maps_to,)*
        _ => return Err(io::Error::new(ErrorKind::InvalidData, concat!("unknown ", $name))),
    }};
}

fn read_str0<'a, 'c, const MAX: usize>(cursor: &'c mut Cursor<&'a Frame>) -> &'a [u8] {
    let start = cursor.position() as usize;
    let slice = cursor.clone().into_inner();
    let end = cursor.seek(SeekFrom::Current(MAX as i64)).unwrap() as usize;
    let str = &slice[start..end];
    match str.iter().position(|c| *c == 0) {
        Some(end) => str.split_at(end).0,
        None => &str,
    }
}

#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
pub enum LoginStatus {
    Ok,
    AlreadyUsed,
    IllegalName,
}

#[derive(Clone, Copy, Hash, Eq, PartialEq)]
pub struct ReceivedMessage<'a> {
    pub date: i64,
    pub author: &'a [u8],
    pub message: &'a [u8],
}

impl<'a> Debug for ReceivedMessage<'a> {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        let mut stru = f.debug_struct("ReceivedMessage");
        stru.field("date", &self.date);

        match std::str::from_utf8(self.author) {
            Ok(str) => stru.field("author", &str),
            Err(_) => stru.field("author", &format_args!("{:?}", self.author)),
        };

        match std::str::from_utf8(self.message) {
            Ok(str) => stru.field("message", &str),
            Err(_) => stru.field("message", &format_args!("{:?}", self.message)),
        };

        stru.finish()
    }
}

#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
pub enum SubscribeResult {
    Ok,
    NotFound,
    Unchanged,
}

#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
pub enum KickReason {
    Closing,
    ProtocolError,
}

#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
pub enum MessageS2C<'a> {
    LoginStatus(LoginStatus),
    ReceivedMessage(ReceivedMessage<'a>),
    SubscribeResult(SubscribeResult),
    SubscriptionEntry(&'a [u8]),
    Kick(KickReason),
}

impl<'a> MessageS2C<'a> {
    pub fn decode(frame: &'a Frame) -> io::Result<Self> {
        let mut cursor = Cursor::new(frame);
        Ok(match_variant!(cursor {
            0 => Self::LoginStatus(match_variant!(cursor {
                0 => LoginStatus::Ok,
                1 => LoginStatus::AlreadyUsed,
                2 => LoginStatus::IllegalName,
            } "login status")),
            1 => Self::ReceivedMessage(ReceivedMessage {
                date: cursor.read_i64::<BE>()?,
                author: read_str0::<USERNAME_MAX_LENGTH>(&mut cursor),
                message: read_str0::<MESSAGE_MAX_LENGTH>(&mut cursor),
            }),
            2 => Self::SubscribeResult(match_variant!(cursor {
                0 => SubscribeResult::Ok,
                1 => SubscribeResult::NotFound,
                2 => SubscribeResult::Unchanged,
            } "subscribe result")),
            3 => Self::SubscriptionEntry(read_str0::<USERNAME_MAX_LENGTH>(&mut cursor)),
            4 => Self::Kick(match_variant!(cursor {
                0 => KickReason::Closing,
                1 => KickReason::ProtocolError,
            } "subscribe result")),
        } "tag"))
    }
}

#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
pub enum MessageC2S<'a> {
    JoinAs(&'a [u8]),
    SubscribeTo(&'a [u8]),
    UnsubscribeFrom(&'a [u8]),
    ListSubscription,
    Publish(&'a [u8]),
}

impl<'a> MessageC2S<'a> {
    pub fn encode(&self) -> io::Result<Frame> {
        let mut frame = Cursor::new(EMPTY_FRAME);

        let (tag, str, max_len) = match self {
            Self::JoinAs(str) => (0, *str, USERNAME_MAX_LENGTH),
            Self::SubscribeTo(str) => (1, *str, USERNAME_MAX_LENGTH),
            Self::UnsubscribeFrom(str) => (2, *str, USERNAME_MAX_LENGTH),
            Self::ListSubscription => (3, Default::default(), 0),
            Self::Publish(str) => (4, *str, MESSAGE_MAX_LENGTH),
        };

        frame.write_u32::<BE>(tag)?;

        if str.len() > max_len {
            Err(io::Error::new(ErrorKind::InvalidData, "str too long"))
        } else {
            frame.clone().into_inner()[frame.position() as usize..][..max_len].fill(0);
            frame.write_all(str)?;
            Ok(frame.into_inner())
        }
    }
}

pub trait ReadExt: Read {
    fn read_s2c<'a>(&mut self, buffer: &'a mut [u8; 48]) -> io::Result<MessageS2C<'a>>;

    fn list_subscriptions_iter<'a>(
        &'a mut self,
    ) -> Box<dyn Iterator<Item = io::Result<Box<[u8]>>> + 'a> {
        let mut buffer = EMPTY_FRAME;
        Box::new(std::iter::from_fn(move || {
            match self.read_s2c(&mut buffer) {
                Err(e) => Some(Err(e)),
                Ok(MessageS2C::SubscriptionEntry(name)) if name.is_empty() => None,
                Ok(MessageS2C::SubscriptionEntry(name)) => {
                    Some(Ok(name.to_vec().into_boxed_slice()))
                }
                Ok(other) => Some(Err(io::Error::new(
                    ErrorKind::Other,
                    format!("expected subscription entry, found {other:?}"),
                ))),
            }
        }))
    }
}

impl<R: Read> ReadExt for R {
    fn read_s2c<'a>(&mut self, buffer: &'a mut [u8; 48]) -> io::Result<MessageS2C<'a>> {
        self.read_exact(buffer)?;
        MessageS2C::decode(buffer)
    }
}

pub trait WriteExt: Write {
    fn write_c2s(&mut self, message: MessageC2S) -> io::Result<()>;

    fn join_as(&mut self, name: &[u8]) -> io::Result<()> {
        self.write_c2s(MessageC2S::JoinAs(name))
    }

    fn subscribe_to(&mut self, name: &[u8]) -> io::Result<()> {
        self.write_c2s(MessageC2S::SubscribeTo(name))
    }

    fn unsubscribe_from(&mut self, name: &[u8]) -> io::Result<()> {
        self.write_c2s(MessageC2S::UnsubscribeFrom(name))
    }

    fn list_subscriptions(&mut self) -> io::Result<()> {
        self.write_c2s(MessageC2S::ListSubscription)
    }

    fn publish(&mut self, twiiiiit: &[u8]) -> io::Result<()> {
        self.write_c2s(MessageC2S::Publish(twiiiiit))
    }
}

impl<W: Write> WriteExt for W {
    fn write_c2s(&mut self, message: MessageC2S) -> io::Result<()> {
        self.write_all(&message.encode()?)
    }
}

pub trait ReadWriteExt {
    fn join_as(&mut self, name: &[u8]) -> io::Result<LoginStatus>;

    fn subscribe_to(&mut self, name: &[u8]) -> io::Result<SubscribeResult>;

    fn unsubscribe_from(&mut self, name: &[u8]) -> io::Result<SubscribeResult>;

    fn list_subscriptions<'a>(
        &'a mut self,
    ) -> io::Result<Box<dyn Iterator<Item = io::Result<Box<[u8]>>> + 'a>>;

    fn publish(&mut self, twiiiiit: &[u8]) -> io::Result<()>;

    fn receive(&mut self) -> io::Result<ReceivedMessage>;
}

fn unexpected_s2c(message: MessageS2C) -> ! {
    panic!("received unexpected message {message:?}")
}

impl<RW: ReadExt + WriteExt> ReadWriteExt for RW {
    fn join_as(&mut self, name: &[u8]) -> io::Result<LoginStatus> {
        let mut frame = EMPTY_FRAME;
        WriteExt::join_as(self, name)?;
        match ReadExt::read_s2c(self, &mut frame)? {
            MessageS2C::LoginStatus(status) => Ok(status),
            other => unexpected_s2c(other),
        }
    }

    fn subscribe_to(&mut self, name: &[u8]) -> io::Result<SubscribeResult> {
        let mut frame = EMPTY_FRAME;
        WriteExt::subscribe_to(self, name)?;
        match ReadExt::read_s2c(self, &mut frame)? {
            MessageS2C::SubscribeResult(res) => Ok(res),
            other => unexpected_s2c(other),
        }
    }

    fn unsubscribe_from(&mut self, name: &[u8]) -> io::Result<SubscribeResult> {
        let mut frame = EMPTY_FRAME;
        WriteExt::unsubscribe_from(self, name)?;
        match ReadExt::read_s2c(self, &mut frame)? {
            MessageS2C::SubscribeResult(res) => Ok(res),
            other => unexpected_s2c(other),
        }
    }

    fn list_subscriptions<'a>(
        &'a mut self,
    ) -> io::Result<Box<dyn Iterator<Item = io::Result<Box<[u8]>>> + 'a>> {
        WriteExt::list_subscriptions(self)?;
        Ok(ReadExt::list_subscriptions_iter(self))
    }

    fn publish(&mut self, twiiiiit: &[u8]) -> io::Result<()> {
        WriteExt::publish(self, twiiiiit)
    }

    /// Leaks the messages
    fn receive(&mut self) -> io::Result<ReceivedMessage<'static>> {
        let mut frame = EMPTY_FRAME;
        match ReadExt::read_s2c(self, &mut frame)? {
            MessageS2C::ReceivedMessage(msg) => Ok(ReceivedMessage {
                author: Box::leak(Vec::from(msg.author).into_boxed_slice()),
                message: Box::leak(Vec::from(msg.message).into_boxed_slice()),
                ..msg
            }),
            other => unexpected_s2c(other),
        }
    }
}
