#include "tcp_connection.hh"

#include <iostream>
// Dummy implementation of a TCP connection

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;
    // the receiver is robust, so no extra process is needed
    _receiver.segment_received(seg);
    // if the rst (reset) flag is set, reset immediately
    if (seg.header().rst) {
        _reset(false);
        return;
    }
    // if the ack flag is set, tells the TCPSender about the fields it cares about on incoming segments: ackno and
    // window_size
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    // if the receiver receives syn for the first time, connect
    // it must contain syn flag, so don't need to worry about empty segment
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        connect();
        return;
    }
    // If the inbound stream ends before the TCPConnection has reached EOF on its outbound stream, this variable needs
    // to be set to false
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED) {
        _linger_after_streams_finish = false;
    }
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish == false) {
        // clean shutdown
        _active_flag = false;
        return;
    }
    // if the incoming segment occupied any sequence numbers, but there is no segment to send
    // send an empty segment with ackno and window size
    if (seg.length_in_sequence_space()) {
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
        // _add_ack_wnd_field_send();
    }
    // notice！！ even if the segment occupied no sequence number, you should still send a segment
    // if `_sender.segments_out()` is not empty. because when `ack_received()` is called, window size may open up and
    // `fill_window()` may be called.
    _add_ack_wnd_field_send();
}

bool TCPConnection::active() const { return _active_flag; }

size_t TCPConnection::write(const string &data) {
    size_t bytes_written = _sender.stream_in().write(data);
    _sender.fill_window();
    _add_ack_wnd_field_send();
    return bytes_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    // if the number of consecutive retransmissions is more than an upper limit, reset immediately
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        // in `_sender.tick()` method, there may be an in-flight segment put into `_sender.segments_out()`. In this
        // case, `_reset()` don't need to send an empty segment
        _reset(true);
        return;
    }
    // notice!! After `_sender.tick()` is called, a in-flight segment may be resent, so you need to
    // call `_add_ack_wnd_field_send()`
    _add_ack_wnd_field_send();
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish) {
        if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            // clean shutdown
            _active_flag = false;
            _linger_after_streams_finish = false;
        }
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _add_ack_wnd_field_send();
}

void TCPConnection::connect() {
    _active_flag = true;
    _sender.fill_window();
    _add_ack_wnd_field_send();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            _reset(true);
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_reset(bool send_rst) {
    /* Any outgoing segment needs to have the proper sequence number.
    You can force the TCPSender to generate an empty segment with the proper sequence number by calling its
    send_empty_segment() method. Or you can make it fill the window (generating segments if it has outstanding
    information to send, e.g. bytes from the stream or SYN/FIN ) by calling its fill_window() method. */
    if (send_rst) {
        // If there is no segment in `_sender.segments_out()`, you need to send an empty segment
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        seg.header().rst = true;
        _segments_out.push(seg);
    }
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active_flag = false;
    _linger_after_streams_finish = false;
}

void TCPConnection::_add_ack_wnd_field_send() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
}