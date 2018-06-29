#pragma once
#include "connection.h"
#include "notifications.h"
#include <unordered_map>

namespace beam {

class Protocol;

/// Active connections
class ConnectedPeers {
public:
    using OnConnectionRemoved = std::function<void(StreamId)>;

    ConnectedPeers(P2PNotifications& notifications, Protocol& protocol, OnConnectionRemoved removedCallback);

    uint32_t total_connected() const;

    void add_connection(Connection::Ptr&& conn);

    void update_peer_state(StreamId streamId, PeerState&& newState);

    void remove_connection(StreamId id);

    io::Result send_message(StreamId id, const io::SharedBuffer& msg);

    io::Result send_message(StreamId id, const SerializedMsg& msg);

    void ping(const io::SharedBuffer& msg);

    void query_known_servers();

private:
    struct Info {
        Connection::Ptr conn;
        PeerState ps;
        Time lastUpdated=0;
        bool knownServersChanged=false;
    };

    using Container = std::unordered_map<StreamId, Info>;

    void broadcast(const io::SharedBuffer& msg, std::function<bool(Info&)>&& filter);

    bool find(StreamId id, Container::iterator& it);

    P2PNotifications& _notifications;
    OnConnectionRemoved _removedCallback;
    io::SharedBuffer _knownServersQueryMsg;
    Container _connections;
    std::vector<StreamId> _toBeRemoved;
};

} //namespace
