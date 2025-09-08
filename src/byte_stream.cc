#include "byte_stream.hh"
#include <cstdint>
#include <string_view>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( available_capacity() == 0 )
    return;
  if ( data.size() > available_capacity() ) {
    data = data.substr( 0, available_capacity() );
  }
  for ( auto& ch : data ) {
    buffer_.push_back( ch );
  }
  total_pushed_ += data.size();
}

void Writer::close()
{
  closed_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return total_pushed_;
}

string_view Reader::peek() const
{
  return string_view( &buffer_.front(), 1 );
}

void Reader::pop( uint64_t len )
{
  for ( uint64_t i = 0; i < len; i++ ) {
    buffer_.pop_front();
  }
  total_popped_ += len;
}

bool Reader::is_finished() const
{
  return closed_ && ( buffer_.size() == 0 );
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}

uint64_t Reader::bytes_popped() const
{
  return total_popped_;
}
