#pragma once
#include "notifications.h"
#include "p2p_settings.h"
#include "rnd_gen.h"
#include "handshake.h"
#include "connected_peers.h"
#include "protocol.h"

#include "utility/asynccontext.h"
#include "utility/io/tcpserver.h"

namespace beam {

class P2P : public IErrorHandler, protected AsyncContext {
public:
    P2P(P2PNotifications& notifications, P2PSettings settings);

    ~P2P();

    Protocol& get_protocol() { return _protocol; }

    void start();

    io::Result send_message(StreamId peer, const io::SharedBuffer& msg);
    io::Result send_message(StreamId peer, const SerializedMsg& msg);

    void update_tip(uint32_t newTip);
    void ban_peer(StreamId id, Time until);
    void ban_ip(uint32_t ip, Time until);

private:
    // IErrorHandler overrides
    void on_protocol_error(uint64_t id, ProtocolError error) override;
    void on_connection_error(uint64_t id, io::ErrorCode errorCode) override;

    void on_start_server(TimerID);
    void on_stream_accepted(io::TcpStream::Ptr&& newStream, io::ErrorCode errorCode);

    void on_connect_to_peers(TimerID);
    void on_stream_connected(uint64_t id, io::TcpStream::Ptr&& newStream, io::ErrorCode errorCode);

    // Handshakes callbacks
    void on_peer_handshaked(Connection::Ptr&& conn, bool isServer);
    void on_handshake_error(StreamId streamId, const HandshakeError& e);

    void on_peer_removed(StreamId streamId);

    void cleanup_connection(StreamId streamId);

    void on_ping_timer(TimerID);
    void on_known_servers_timer(TimerID);

    bool on_peer_state(uint64_t id, PeerState&& state);
    bool on_known_servers_request(uint64_t id, VoidMessage&&);
    bool on_known_servers(uint64_t id, KnownServers&& servers);

    RandomGen           _rdGen;
    P2PNotifications&   _notifications;
    P2PSettings         _settings;
    io::TcpServer::Ptr  _thisServer;
    ConnectPool         _connectPool;
    Protocol            _protocol;
    HandshakingPeers    _handshakes;
    ConnectedPeers      _connectedPeers;

    KnownServers        _knownServers;
    bool                _knownServersUpdated=false;
    io::SharedBuffer    _knownServersMsg;

    PeerState           _peerState;
    bool                _peerStateUpdated=false;
    io::SharedBuffer    _peerStateMsg;
};

} //namespace
