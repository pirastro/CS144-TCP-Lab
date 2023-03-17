#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check_lab4`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    // if the window size is 0, consider it as 1
    size_t cur_wnd_size = _wnd_size ? _wnd_size : 1;
    while (_bytes_in_flight < cur_wnd_size) {
        TCPSegment seg;
        // if this is the first segment
        if (!_syn_flag) {
            seg.header().syn = true;
            _syn_flag = true;
        }
        seg.header().seqno = next_seqno();
        // the size of current segment's payload
        size_t payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, cur_wnd_size - _bytes_in_flight - seg.header().syn);
        string payload_string = _stream.read(payload_size);
        if (_stream.eof() && (_bytes_in_flight + payload_string.size()) < cur_wnd_size && !_fin_flag) {
            seg.header().fin = true;
            _fin_flag = true;
        }
        seg.payload() = Buffer(move(payload_string));
        if (seg.length_in_sequence_space() == 0) {
            break;
        }
        // if no segment is in flight, reset the timer
        if (_in_flight_map.empty()) {
            _rto = _initial_retransmission_timeout;
            _timer = 0;
        }
        _segments_out.push(seg);
        _bytes_in_flight += seg.length_in_sequence_space();
        // keep a copy of this segment, and the key is absolute sqn
        _in_flight_map.emplace(_next_seqno, seg);
        _next_seqno += seg.length_in_sequence_space();
        if (seg.header().fin) {
            break;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t absolute_ackno = unwrap(ackno, _isn, _next_seqno);
    if (absolute_ackno > _next_seqno) {
        return;
    }
    while (!_in_flight_map.empty()) {
        auto iter = _in_flight_map.begin();
        const TCPSegment &tmp_seg = iter->second;
        // if the segment in _in_flight_map is acked
        if (iter->first + tmp_seg.length_in_sequence_space() <= absolute_ackno) {
            _in_flight_map.erase(iter);
            _bytes_in_flight -= tmp_seg.length_in_sequence_space();
            _rto = _initial_retransmission_timeout;
            _timer = 0;
        } else {
            break;
        }
    }
    _wnd_size = window_size;
    _consecutive_retransmissions = 0;
    // fill the window again if new space has opened up
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer += ms_since_last_tick;
    if (_timer >= _rto) {
        if (!_in_flight_map.empty()) {
            const TCPSegment &seg = _in_flight_map.begin()->second;
            _segments_out.push(seg);
            // if window size is nonzero
            if (_wnd_size > 0) {
                _rto *= 2;
            }
            _consecutive_retransmissions++;
            // reset the retransmission timer
            _timer = 0;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}