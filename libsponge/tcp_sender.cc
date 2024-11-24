#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _sent_out_bytes; }

void TCPSender::fill_window() {
    size_t window_size = _latest_window_size == 0 ? 1 : _latest_window_size;
    while(window_size > _sent_out_bytes) {
        TCPSegment seg;
        // if not yet sent synchronized segment, set header syn flag
        if (!_syn_flag) {
            seg.header().syn = true;
            _syn_flag = true;
        }
        // set seqno and payload
        seg.header().seqno = next_seqno();
        size_t payload_size = min(window_size - _sent_out_bytes - (seg.header().syn), TCPConfig::MAX_PAYLOAD_SIZE);
        string payload = _stream.read(payload_size);

        // increse fin if satisfy the condition
        if (!_fin_flag && _stream.eof() && window_size - payload.size() - _sent_out_bytes >= 1) {
            seg.header().fin = true;
            _fin_flag = true;
        }
        
        // if no any data to send, break
        if (payload.empty() && !seg.header().syn && !seg.header().fin) {
            break;
        }
        // if no waiting data seg, reset 
        if (_sent_segments.empty()) {
            _timer = 0;
            _retransmission_timeout = _initial_retransmission_timeout;
        }
        // send seg and track the sent seg
        seg.payload() = Buffer(std::move(payload));
        _sent_segments.insert(make_pair(_next_seqno, seg));
        _segments_out.push(seg);
        _sent_out_bytes += seg.length_in_sequence_space();
        _next_seqno += seg.length_in_sequence_space();

        // if set fin flag, break
        if (seg.header().fin) {
            break;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // if ackno is invalid, return
    auto abs_ackno = unwrap(ackno, _isn, _next_seqno);
    if (abs_ackno > _next_seqno) {
        return;
    }

    // update the sent out bytes
    auto iter = _sent_segments.begin();
    while (iter != _sent_segments.end()) {
        if (abs_ackno >= iter->first + iter->second.length_in_sequence_space()) {
            // if ackno is valid, erase the sent seg
            _sent_out_bytes -= iter->second.length_in_sequence_space();
            iter = _sent_segments.erase(iter);

            // reset the timer and retransmission_timeout
            _timer = 0;
            _retransmission_timeout = _initial_retransmission_timeout;
        } else {
            break;
        }
        
    }
    // reset the retransmission counter
    _retransmission_counter = 0;
    // update the window size
    _latest_window_size = window_size;
    // if ackno is valid, fill the window
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer += ms_since_last_tick;
    auto iter = _sent_segments.begin();
    while (iter != _sent_segments.end()) {
        if (_timer >= _retransmission_timeout) {
            // reset the timer
            _timer = 0;
            _segments_out.push(iter->second);
            if (_latest_window_size > 0) {
                _retransmission_timeout *= 2;
            }
            _retransmission_counter++;
        } else {
            break;
        }
        
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _retransmission_counter;
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
