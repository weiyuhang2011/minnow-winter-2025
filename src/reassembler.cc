#include "reassembler.hh"
#include "debug.hh"
#include <cstdint>
#include <utility>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  debug( "insert({}, {}, {}) called", first_index, data, is_last_substring );

  if ( is_last_substring ) {
    stream_end_idx = first_index + data.size();
    has_last_substring = true;
  }

  auto maybe_close = [&] {
    if ( has_last_substring && buffer_.empty() && writer().bytes_pushed() == stream_end_idx ) {
      output_.writer().close();
    }
  };

  // Bytes that are the next bytes in the stream.
  uint64_t pushed = writer().bytes_pushed();
  if ( ( first_index + data.size() ) <= pushed ) {
    debug( "situation 1: duplicate data {} {}", first_index, data );
    maybe_close();
    return;
  }

  if ( first_index <= pushed ) {
    debug( "situation 2: data directly pushs to stream {} {}", first_index, data );
    output_.writer().push( data.substr( static_cast<size_t>( pushed - first_index ) ) );
    pushed = writer().bytes_pushed();

    while ( 1 ) {
      bool progressed = false;

      auto it = buffer_.upper_bound( pushed );
      if ( it != buffer_.begin() ) {
        auto cand = it;
        --cand;

        const uint64_t start = cand->first;
        const auto& s = cand->second;
        const uint64_t end = start + s.size();

        if ( ( start <= pushed ) && ( end > pushed ) ) {
          debug( "situation 2.5: buffered data pushs to stream {} {}", start, s );
          buffered_bytes -= s.size();
          if ( start == pushed ) {
            output_.writer().push( s );
          } else {
            output_.writer().push( s.substr( pushed - start ) );
          }
          pushed = writer().bytes_pushed();
          buffer_.erase( cand );
          progressed = true;
        } else if ( end <= pushed ) {
          buffered_bytes -= s.size();
          buffer_.erase( cand );
          progressed = true;
        }
        if ( progressed )
          continue;
      }
      break;
    }
    maybe_close();
    return;
  }

  const uint64_t avail = writer().available_capacity();
  if ( avail <= buffered_bytes ) {
    debug( "situation 3: no available capacity, {} {}", avail, buffered_bytes );
    return;
  }

  // overlapping
  uint64_t new_start = first_index;
  uint64_t new_end = first_index + data.size();
  string new_data = move( data );

  for ( auto it = buffer_.begin(); it != buffer_.end(); ) {
    auto old_start = it->first;
    auto old_end = it->first + it->second.size();

    // no overlapping
    if ( ( new_end <= old_start ) || ( new_start >= old_end ) ) {
      debug( "no overlapping" );
      ++it;
      continue;
    }

    // overlapping, from stream_size
    if ( new_start >= old_start && new_end <= old_end ) {
      debug( "情况 3：旧数据完全覆盖新数据 -> 返回旧数据" );
      return;
    }

    if ( new_start <= old_start && new_end >= old_end ) {
      debug( "情况 4：新数据完全覆盖旧数据 -> 替换为新数据" );
      buffered_bytes -= it->second.size();
      it = buffer_.erase( it );
      continue;
    }

    if ( new_end > old_start && new_start < old_start ) {
      debug( "情况 1：新数据在前，旧数据在后" );
      uint64_t overlap = new_end - old_start;
      new_data = new_data.substr( 0, new_data.size() - overlap ) + it->second;
      buffered_bytes -= it->second.size();
      it = buffer_.erase( it );
      continue;
    }
    if ( new_start < old_end && new_end > old_end ) {
      debug( "情况 2：新数据在后，旧数据在前" );
      uint64_t overlap = old_end - new_start;
      new_data = it->second + new_data.substr( overlap );
      new_start = old_start;
      buffered_bytes -= it->second.size();
      it = buffer_.erase( it );
      continue;
    }
  }
  uint64_t cap_end = pushed + avail;
  if ( new_start >= cap_end ) {
    debug( "not enough size to buffer new data" );
    return;
  }
  if ( new_start + new_data.size() <= cap_end ) {
    debug( "enough size to buffer new data {}", new_data );
    buffered_bytes += new_data.size();
    buffer_.insert( { new_start, move( new_data ) } );
  } else {
    string ndata = new_data.substr( 0, cap_end - new_start );
    debug( "not enough size to buffer new data {} {}", cap_end, ndata );
    buffered_bytes += ( cap_end - new_start );
    buffer_.insert( { new_start, move( ndata ) } );
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t pending_bytes {};
  for ( const auto& kv : buffer_ ) {
    pending_bytes += kv.second.size();
  }
  return pending_bytes;
}
