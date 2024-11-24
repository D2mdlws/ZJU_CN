#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    if (_arp_table.find(next_hop_ip) != _arp_table.end()) {
        // next hop is in ARP table
        const EthernetAddress& next_hop_eth = _arp_table[next_hop_ip].eth_addr;
        EthernetHeader header;
        header.dst = next_hop_eth;
        header.src = _ethernet_address;
        header.type = EthernetHeader::TYPE_IPv4;
        EthernetFrame frame;
        frame.header() = header;
        frame.payload() = dgram.serialize();
        _frames_out.push(frame);
    } else {
        // next hop is not in ARP table
        // add datagram to waiting queue
        _waiting_datagrams[next_hop_ip].push_back(dgram);

        // haen't sent ARP request for this IP address yet
        if (_arp_requests.find(next_hop_ip) == _arp_requests.end()) {
            // send ARP request
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ip_address = next_hop_ip;
            arp_request.target_ethernet_address = {};

            EthernetHeader header;
            header.dst = ETHERNET_BROADCAST;
            header.src = _ethernet_address;
            header.type = EthernetHeader::TYPE_ARP;
            EthernetFrame frame;
            frame.header() = header;
            frame.payload() = arp_request.serialize();
            _frames_out.push(frame); // send ARP request

            _arp_requests[next_hop_ip] = _timer; // record time of request
        }
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    EthernetAddress dst = frame.header().dst;
    uint16_t type = frame.header().type;
    BufferList payload = frame.payload();

    // early return if not for this interface
    if (dst != _ethernet_address && dst != ETHERNET_BROADCAST) {
        // not for this interface
        return {};
    }
    if (!(type == EthernetHeader::TYPE_IPv4 || type == EthernetHeader::TYPE_ARP)) {
        // not an IPv4 or ARP frame
        return {};
    }

    if (type == EthernetHeader::TYPE_IPv4) {
        // IPv4 frame
        InternetDatagram dgram;
        ParseResult res = dgram.parse(payload);
        if (res == ParseResult::NoError) {
            return dgram;
        } else {
            return {};
        }
    } else if (type == EthernetHeader::TYPE_ARP) {
        // ARP frame
        ARPMessage arp_msg;
        ParseResult res = arp_msg.parse(payload);

        if (res != ParseResult::NoError) {
            return {};
        }
        
        uint32_t sender_ip = arp_msg.sender_ip_address;
        _arp_table[sender_ip] = {arp_msg.sender_ethernet_address, _timer}; // 30s

        if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST && arp_msg.target_ip_address == _ip_address.ipv4_numeric()) {
            // ARP request
            ARPMessage arp_reply;
            arp_reply.opcode = ARPMessage::OPCODE_REPLY;
            arp_reply.sender_ethernet_address = _ethernet_address;
            arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
            arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;
            arp_reply.target_ip_address = arp_msg.sender_ip_address;

            EthernetHeader header;
            header.dst = arp_msg.sender_ethernet_address;
            header.src = _ethernet_address;
            header.type = EthernetHeader::TYPE_ARP;
            EthernetFrame reply_frame;
            reply_frame.header() = header;
            reply_frame.payload() = arp_reply.serialize();
            _frames_out.push(reply_frame); // send ARP reply
        }

        _arp_requests.erase(sender_ip); // remove from ARP requests

        // send waiting datagrams if any
        if (_waiting_datagrams.find(sender_ip) != _waiting_datagrams.end()) {
            for (const auto& dgram : _waiting_datagrams[sender_ip]) {
                send_datagram(dgram, Address::from_ipv4_numeric(sender_ip));
            }
            _waiting_datagrams.erase(sender_ip);
        }
    
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    _timer += ms_since_last_tick;

    // check ARP table
    auto it = _arp_table.begin();
    while (it != _arp_table.end()) {
        if (_timer - it->second.ttl > 30 * 1000) { // 30s
            it = _arp_table.erase(it);
        } else {
            ++it;
        }
    }

    // check ARP requests
    auto it2 = _arp_requests.begin();
    while (it2 != _arp_requests.end()) {
        if (_timer - it2->second >= 5 * 1000) { // 5s
            // resend ARP request
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ip_address = it2->first;

            EthernetHeader header;
            header.dst = ETHERNET_BROADCAST;
            header.src = _ethernet_address;
            header.type = EthernetHeader::TYPE_ARP;
            EthernetFrame frame;
            frame.header() = header;
            frame.payload() = arp_request.serialize();
            _frames_out.push(frame); // send ARP request

            it2->second = _timer; // record time of request
        }
        ++it2;
    }
}
