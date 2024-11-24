#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) { 
    // _buffer_output = "";
}

size_t ByteStream::write(const string &data) {
    if (_end_input_flag) return 0;
    
    size_t len = data.size();
    if (len + _buffer_input.length()> _capacity) {
        len = _capacity - _buffer_input.length();
    }
    if (len <= 0) {
        return 0;
    }

    _buffer_input += data.substr(0, len);
    _bytes_written += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t len_peek = len;
    if (len_peek > _buffer_input.length()) {
        len_peek = _buffer_input.length();
    }
    return _buffer_input.substr(0, len_peek);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t len_output  = len;
    if (len_output > _buffer_input.length()) {
        len_output = _buffer_input.length();
    }
    _buffer_input = _buffer_input.substr(len_output);
    _bytes_read += len_output;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string result = peek_output(len);
    pop_output(len);
    return result;
}

void ByteStream::end_input() {
    _end_input_flag = true;
}

bool ByteStream::input_ended() const {
    return _end_input_flag;
}

size_t ByteStream::buffer_size() const {
    return _buffer_input.length();
}

bool ByteStream::buffer_empty() const {
    return _buffer_input.empty();
}

bool ByteStream::eof() const {
    return _end_input_flag && buffer_empty();
}

size_t ByteStream::bytes_written() const {
    return _bytes_written;
}

size_t ByteStream::bytes_read() const {
    return _bytes_read;
}

size_t ByteStream::remaining_capacity() const {
    return _capacity - _buffer_input.length();
}
