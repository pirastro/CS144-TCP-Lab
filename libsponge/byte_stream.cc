#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _buf_capacity(capacity), _error(false) {
    // DUMMY_CODE(capacity);
}

// Write a string of bytes into the stream. Write as many
// as will fit, and return the number of bytes written.
size_t ByteStream::write(const string &data) {
    // the bytes of data that is about to writen into stream
    size_t bytes_to_write = (remaining_capacity() >= data.size()) ? data.size() : remaining_capacity();
    // write into buffer
    for (size_t i = 0; i < bytes_to_write; i++) {
        _buffer.push_back(data[i]);
    }
    _total_written += bytes_to_write;
    return bytes_to_write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
// Peek at next "len" bytes of the stream
string ByteStream::peek_output(const size_t len) const {
    // DUMMY_CODE(len);
    // return {};
    string p;
    for (size_t i = 0; i < len && i < _buffer.size(); i++) {
        p.push_back(_buffer.at(i));
    }
    return p;
}

//! \param[in] len bytes will be removed from the output side of the buffer
// Remove "len" bytes from the buffer
void ByteStream::pop_output(const size_t len) {
    size_t true_len = min(len, _buffer.size());
    _total_read += true_len;
    while (true_len > 0) {
        _buffer.pop_front();
        true_len--;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t true_len = min(len, _buffer.size());
    string copy = peek_output(true_len);
    pop_output(true_len);
    return copy;
}

void ByteStream::end_input() { _input_eof = true; }

bool ByteStream::input_ended() const { return _input_eof; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return _total_written; }

size_t ByteStream::bytes_read() const { return _total_read; }

size_t ByteStream::remaining_capacity() const { return _buf_capacity - buffer_size(); }