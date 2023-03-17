#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

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
    if (_mapping.find(next_hop_ip) != _mapping.end()) {
        EthernetAddress dst = _mapping.at(next_hop_ip);
        EthernetAddress src = _ethernet_address;
        EthernetFrame frame;
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.header().dst = dst;
        frame.header().src = src;
        frame.payload() = dgram.serialize();
        frames_out().push(frame);
    } else {
        // queue the IP datagram so it can be sent after the ARP reply is received
        _datagramQue.emplace_back(dgram);
        // queue the IP of the datagram
        _dstQue.emplace_back(next_hop_ip);
        if (_arpRequest.find(next_hop_ip) != _arpRequest.end() && _arpRequest.at(next_hop_ip) <= 5 * 1000) {
            return;
        }
        ARPMessage msg;
        msg.opcode = ARPMessage::OPCODE_REQUEST;  // arp request
        msg.sender_ethernet_address = _ethernet_address;
        msg.sender_ip_address = _ip_address.ipv4_numeric();
        msg.target_ethernet_address = {};
        msg.target_ip_address = next_hop_ip;
        EthernetAddress dst = ETHERNET_BROADCAST;
        EthernetAddress src = _ethernet_address;
        EthernetFrame frame;
        frame.header().type = EthernetHeader::TYPE_ARP;
        frame.header().dst = dst;
        frame.header().src = src;
        frame.payload() = msg.serialize();
        frames_out().push(frame);
        _arpRequest.emplace(next_hop_ip, 0);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return nullopt;
    }
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(frame.payload()) == ParseResult::NoError) {
            return dgram;
        } else {
            return nullopt;
        }

    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage msg;
        if (msg.parse(frame.payload()) == ParseResult::NoError) {
            _mapping.emplace(msg.sender_ip_address, msg.sender_ethernet_address);
            _timing.emplace(msg.sender_ip_address, 0);
            // resent the datagram in queue if possible
            auto iter2 = _datagramQue.begin();
            for (auto iter1 = _dstQue.begin(); iter1 != _dstQue.end();) {
                if (*iter1 == msg.sender_ip_address) {
                    EthernetFrame resentFrame;
                    resentFrame.header().type = EthernetHeader::TYPE_IPv4;
                    resentFrame.header().dst = msg.sender_ethernet_address;
                    resentFrame.header().src = _ethernet_address;
                    InternetDatagram resentDgram = *iter2;
                    resentFrame.payload() = resentDgram.serialize();
                    frames_out().push(resentFrame);
                    iter1 = _dstQue.erase(iter1);
                    iter2 = _datagramQue.erase(iter2);
                } else {
                    ++iter1;
                    ++iter2;
                }
            }
            // if the incoming arp request's target is our ip address
            if (msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == _ip_address.ipv4_numeric()) {
                ARPMessage reply;
                reply.opcode = ARPMessage::OPCODE_REPLY;
                reply.sender_ethernet_address = _ethernet_address;
                reply.sender_ip_address = _ip_address.ipv4_numeric();
                reply.target_ethernet_address = msg.sender_ethernet_address;
                reply.target_ip_address = msg.sender_ip_address;
                EthernetFrame replyFrame;
                replyFrame.header().type = EthernetHeader::TYPE_ARP;
                replyFrame.header().src = _ethernet_address;
                replyFrame.header().dst = msg.sender_ethernet_address;
                replyFrame.payload() = reply.serialize();
                frames_out().push(replyFrame);
            }
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto iter = _arpRequest.begin(); iter != _arpRequest.end();) {
        iter->second += ms_since_last_tick;
        if (iter->second > 5 * 1000) {
            iter = _arpRequest.erase(iter);
        } else {
            ++iter;
        }
    }
    auto iter2 = _mapping.begin();
    for (auto iter1 = _timing.begin(); iter1 != _timing.end();) {
        iter1->second += ms_since_last_tick;
        if (iter1->second >= 30 * 1000) {
            iter1 = _timing.erase(iter1);
            iter2 = _mapping.erase(iter2);
        } else {
            ++iter1;
            ++iter2;
        }
    }
}