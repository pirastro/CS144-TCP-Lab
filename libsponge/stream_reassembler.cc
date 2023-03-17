#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _first_unassembled(0)
    , _unassembled_storage(capacity, '\0')
    , _empty_flag(capacity, true)
    , _unassembled_bytes(0)
    , _eof_index(-1)
    , _output(capacity)
    , _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t first_unacceptable = _first_unassembled + _output.remaining_capacity();
    // if `data` completely exceeds the limit of capacity or it duplicates with the data that has been output
    if (index > first_unacceptable || index + data.size() < _first_unassembled) {
        return;
    }
    if (eof) {
        // _eof_flag = true;
        _eof_index = index + data.size();
    }
    // cut off the part of data which exceeds the capacity or duplicates
    size_t data_start = index;
    size_t data_end = index + data.size();
    if (data_start < _first_unassembled) {
        data_start = _first_unassembled;
    }
    if (data_end > first_unacceptable) {
        data_end = first_unacceptable;
    }
    string new_data = data.substr(data_start - index, data_start - data_end);
    // store unduplicated part of data into `_unassembled_storage`
    for (size_t i = data_start; i < data_end; i++) {
        if (_empty_flag.at(i - _first_unassembled) == true) {
            _unassembled_storage.at(i - _first_unassembled) = new_data[i - data_start];
            _empty_flag.at(i - _first_unassembled) = false;
            _unassembled_bytes++;
        }
    }
    string tmp;
    while (_empty_flag.front() == false) {
        tmp += _unassembled_storage.front();
        _unassembled_storage.pop_front();
        _unassembled_storage.push_back('\0');
        _empty_flag.pop_front();
        _empty_flag.push_back(true);
    }

    size_t tmp_size = _output.write(tmp);
    _unassembled_bytes -= tmp_size;
    _first_unassembled += tmp_size;
    if (_first_unassembled >= _eof_index) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }