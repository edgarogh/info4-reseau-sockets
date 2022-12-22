#![cfg(test)]

mod network;
mod test_server;

use crate::network::{
    KickReason, LoginStatus, MessageS2C, ReadWriteExt, SubscribeResult, EMPTY_FRAME,
    MESSAGE_MAX_LENGTH,
};
use std::net::Shutdown;
use std::time::Duration;

/// Définis des clients connectés à un même serveur, dans la portée d'invocation, ainsi qu'un buffer
/// pour la reception
macro_rules! clients {
    ($server_name:ident: $($client_name:ident)*) => {
        let $server_name = $crate::test_server::TestServer::start();
        $(#[allow(unused_mut)] let mut $client_name = $server_name.connect().unwrap();)*
    };
    ($($client_name:ident)*) => {
        clients!(_server: $($client_name)*);
    };
}

macro_rules! assert_twiiiiit_eq {
    ($twiiiiit:expr, $author:expr, $message:expr) => {
        let twiiiiit: $crate::network::ReceivedMessage = $twiiiiit;
        assert_eq!(twiiiiit.author, $author);
        assert_eq!(twiiiiit.message, $message);
    };
}

#[test]
fn test_join_ok() {
    clients!(edgar);
    assert_eq!(edgar.join_as(b"Edgar").unwrap(), LoginStatus::Ok);
}

#[test]
fn test_join_longest_username_ok() {
    clients!(matteo);
    assert_eq!(matteo.join_as(b"Matteo").unwrap(), LoginStatus::Ok);
}

#[test]
fn test_join_same_username() {
    clients!(bob1 bob2);
    assert_eq!(bob1.join_as(b"Bob").unwrap(), LoginStatus::Ok);
    assert_eq!(bob2.join_as(b"Bob").unwrap(), LoginStatus::AlreadyUsed);
}

#[test]
fn test_join_empty_name() {
    clients!(ana);
    assert_eq!(ana.join_as(b"").unwrap(), LoginStatus::IllegalName);
}

#[test]
fn test_join_twice() {
    clients!(alice);
    assert_eq!(alice.join_as(b"Alice").unwrap(), LoginStatus::Ok);
    network::WriteExt::join_as(&mut alice, b"Alice").unwrap();
    let mut frame = EMPTY_FRAME;
    let protocol_error = network::ReadExt::read_s2c(&mut alice, &mut frame).unwrap();
    assert_eq!(protocol_error, MessageS2C::Kick(KickReason::ProtocolError));
}

#[test]
fn test_twiiiiit() {
    clients!(stream);

    assert_eq!(stream.join_as(b"Edgar").unwrap(), LoginStatus::Ok);

    let twiiiiit = b"Hello world!";
    assert!(twiiiiit.len() < MESSAGE_MAX_LENGTH);

    stream.publish(twiiiiit).unwrap();
    assert_twiiiiit_eq!(stream.receive().unwrap(), b"Edgar", twiiiiit);
}

#[test]
fn test_twiiiiit_longest() {
    clients!(stream);

    assert_eq!(stream.join_as(b"Edgar").unwrap(), LoginStatus::Ok);

    let twiiiiit = b"Hello world! padding";
    assert_eq!(twiiiiit.len(), MESSAGE_MAX_LENGTH);

    stream.publish(twiiiiit).unwrap();
    assert_twiiiiit_eq!(stream.receive().unwrap(), b"Edgar", twiiiiit);
}

#[test]
fn test_subscribe_ok() {
    clients!(alice bob);
    assert_eq!(bob.join_as(b"Bob").unwrap(), LoginStatus::Ok);
    assert_eq!(alice.join_as(b"Alice").unwrap(), LoginStatus::Ok);

    // Both subscription lists are empty
    assert_eq!(bob.list_subscriptions().unwrap().count(), 0);
    assert_eq!(alice.list_subscriptions().unwrap().count(), 0);

    // Bob subscribes to Alice, but not the other way round
    assert_eq!(bob.subscribe_to(b"Alice").unwrap(), SubscribeResult::Ok);
    let bob_subscriptions = bob.list_subscriptions().unwrap().collect::<Vec<_>>();
    assert_eq!(bob_subscriptions.len(), 1);
    assert_eq!(bob_subscriptions[0].as_ref().unwrap().as_ref(), b"Alice");
    assert_eq!(alice.list_subscriptions().unwrap().count(), 0);
}

#[test]
fn test_subscribe_not_found() {
    clients!(bob);
    assert_eq!(bob.join_as(b"Bob").unwrap(), LoginStatus::Ok);

    // Bob tries to subscribe to Alice, but she has never joined
    assert_eq!(
        bob.subscribe_to(b"Alice").unwrap(),
        SubscribeResult::NotFound
    );
}

#[test]
fn test_subscribe_unchanged() {
    clients!(alice bob);
    assert_eq!(bob.join_as(b"Bob").unwrap(), LoginStatus::Ok);
    assert_eq!(alice.join_as(b"Alice").unwrap(), LoginStatus::Ok);

    // Bob tries to subscribe to Alice twice
    assert_eq!(bob.subscribe_to(b"Alice").unwrap(), SubscribeResult::Ok);
    assert_eq!(
        bob.subscribe_to(b"Alice").unwrap(),
        SubscribeResult::Unchanged
    );
}

#[test]
fn test_unsubscribe_ok() {
    clients!(alice bob);
    assert_eq!(bob.join_as(b"Bob").unwrap(), LoginStatus::Ok);
    assert_eq!(alice.join_as(b"Alice").unwrap(), LoginStatus::Ok);

    // Subscribe-unsubscribe
    assert_eq!(bob.subscribe_to(b"Alice").unwrap(), SubscribeResult::Ok);
    assert_eq!(bob.list_subscriptions().unwrap().count(), 1);
    assert_eq!(bob.unsubscribe_from(b"Alice").unwrap(), SubscribeResult::Ok);
    assert_eq!(bob.list_subscriptions().unwrap().count(), 0);
}

#[test]
fn test_unsubscribe_not_found() {
    clients!(bob);
    assert_eq!(bob.join_as(b"Bob").unwrap(), LoginStatus::Ok);
    assert_eq!(
        bob.subscribe_to(b"Alice").unwrap(),
        SubscribeResult::NotFound
    );
}

#[test]
fn test_unsubscribe_unchanged() {
    clients!(alice bob);
    assert_eq!(bob.join_as(b"Bob").unwrap(), LoginStatus::Ok);
    assert_eq!(alice.join_as(b"Alice").unwrap(), LoginStatus::Ok);

    // Subscribe-unsubscribe
    assert_eq!(bob.subscribe_to(b"Alice").unwrap(), SubscribeResult::Ok);
    assert_eq!(bob.unsubscribe_from(b"Alice").unwrap(), SubscribeResult::Ok);
    assert_eq!(
        bob.unsubscribe_from(b"Alice").unwrap(),
        SubscribeResult::Unchanged
    );
}

#[test]
fn test_twiiiiit_live_subscriptions() {
    clients!(alice bob);
    assert_eq!(bob.join_as(b"Bob").unwrap(), LoginStatus::Ok);
    assert_eq!(alice.join_as(b"Alice").unwrap(), LoginStatus::Ok);

    // Bob subscribes to Alice, but not the other way round
    assert_eq!(bob.subscribe_to(b"Alice").unwrap(), SubscribeResult::Ok);

    // Alice twiiiiits, both receive
    alice.publish(b"Hi Bob!").unwrap();
    assert_twiiiiit_eq!(bob.receive().unwrap(), b"Alice", b"Hi Bob!");
    assert_twiiiiit_eq!(alice.receive().unwrap(), b"Alice", b"Hi Bob!");

    // Bob twiiiiits, he's his only reader
    bob.publish(b"Hi Alice!").unwrap();
    assert_twiiiiit_eq!(bob.receive().unwrap(), b"Bob", b"Hi Alice!");

    // Alice twiiiiits again
    alice.publish(b"Why don't you answer").unwrap();
    assert_twiiiiit_eq!(bob.receive().unwrap(), b"Alice", b"Why don't you answer");
    assert_twiiiiit_eq!(alice.receive().unwrap(), b"Alice", b"Why don't you answer");
}

#[test]
fn test_twiiiiit_follower_offline_catches_up() {
    clients!(server: alice bob);

    // Everyone joins
    assert_eq!(bob.join_as(b"Bob").unwrap(), LoginStatus::Ok);
    assert_eq!(alice.join_as(b"Alice").unwrap(), LoginStatus::Ok);

    // Bob subscribes to Alice and logs off
    assert_eq!(bob.subscribe_to(b"Alice").unwrap(), SubscribeResult::Ok);
    bob.shutdown(Shutdown::Both).unwrap();
    drop(bob);

    std::thread::sleep(Duration::from_millis(5));

    // Alice twiiiiits but her only follower is online
    alice.publish(b"I like chocolate").unwrap();
    assert_twiiiiit_eq!(alice.receive().unwrap(), b"Alice", b"I like chocolate");

    std::thread::sleep(Duration::from_millis(5));

    // Bob logs in again, and catches up with the missed twiiiiit
    let mut bob = server.connect().unwrap();
    assert_eq!(bob.join_as(b"Bob").unwrap(), LoginStatus::Ok);
    assert_twiiiiit_eq!(bob.receive().unwrap(), b"Alice", b"I like chocolate");
}
