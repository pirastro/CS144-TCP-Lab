#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &header = seg.header();
    // if syn has not been set
    if (!_syn_flag) {
        if (!header.syn) {
            return;
        } else {
            _isn = header.seqno;
            _syn_flag = true;
        }
    }
    WrappingInt32 sqn = header.seqno;
    uint64_t check = _reassembler.stream_out().bytes_written() + 1;
    uint64_t absolute_sqn = unwrap(sqn, _isn, check);
    // stream index equals to absoulte squ - 1
    size_t idx = absolute_sqn - 1;
    if (header.syn) {
        // abandon the syn when pushing subtring
        _reassembler.push_substring(seg.payload().copy(), idx + 1, header.fin);
    } else {
        _reassembler.push_substring(seg.payload().copy(), idx, header.fin);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_flag) {
        return {};
    } else {
        WrappingInt32 ackno =
            wrap(_reassembler.stream_out().bytes_written() + 1, _isn);  // plus one because syn also occpuies one sqn
        if (_reassembler.stream_out().input_ended()) {
            ackno = operator+(ackno, 1);  // plus one because fin also occpuies one sqn
        }
        return ackno;
    }
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }