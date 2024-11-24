#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // current state is LISTEN
    if (!_syn_flag) {
        if ((seg.header().syn) == true) {
            _syn_flag = true;
            _isn = seg.header().seqno;
        } else {
            return;
        }
    }
    // current state is SYN_RECEIVED
    uint64_t abs_ackno = _reassembler.stream_out().bytes_written() + 1;
    uint64_t current_abs_seqno = unwrap(seg.header().seqno, _isn, abs_ackno);
    uint64_t stream_index = current_abs_seqno - 1;
    if (seg.header().syn) {
        stream_index++;
    }
    if (seg.header().fin) {
        _reassembler.push_substring(seg.payload().copy(), stream_index, true);
    } else {
        _reassembler.push_substring(seg.payload().copy(), stream_index, false);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (!_syn_flag) {
        return {};
    }
    bool is_FIN_RECV = _reassembler.stream_out().input_ended();
    // not in LISTEN, need to add 1 for the SYN
    uint64_t abs_ackno = _reassembler.stream_out().bytes_written() + 1;
    // if FIN is received, need to add 1 for the FIN
    if (is_FIN_RECV) {
        abs_ackno++;
    }
    return WrappingInt32(_isn) + abs_ackno;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
