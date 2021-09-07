#pragma once

#include <libutils/std.h>

namespace Utils
{

template <typename T, size_t N>
struct InlineRingBuffer
{

private:
    size_t _head = 0;
    size_t _tail = 0;
    size_t _used = 0;

    T _buffer[N] = {};

public:
    InlineRingBuffer() {}

    InlineRingBuffer(const InlineRingBuffer &other)
        : _head(other._head),
          _tail(other._tail),
          _used(other._used)
    {
        memcpy(_buffer, other._buffer, N);
    }

    InlineRingBuffer(InlineRingBuffer &&other)
        : _head(other._head),
          _tail(other._tail),
          _used(other._used),
          _buffer(other._buffer)
    {
        other._head = 0;
        other._tail = 0;
        other._used = 0;
        other._buffer = nullptr;
    }

        void flush()
    {
        _head = 0;
        _tail = 0;
        _used = 0;
    }

    InlineRingBuffer &operator=(const InlineRingBuffer &other)
    {
        return *this = InlineRingBuffer(other);
    }

    InlineRingBuffer &operator=(InlineRingBuffer &&other)
    {
        std::swap(_head, other._head);
        std::swap(_tail, other._tail);
        std::swap(_used, other._used);
        std::swap(_buffer, other._buffer);

        return *this;
    }

    bool empty() const { return _used == 0; }

    bool full() const { return _used == N; }

    size_t used() const { return _used; }

    void put(T c)
    {
        assert(!full());

        _buffer[_head] = c;
        _head = (_head + 1) % N;
        _used++;
    }

    T get()
    {
        assert(!empty());

        T c = _buffer[_tail];
        _tail = (_tail + 1) % N;
        _used--;

        return c;
    }




};

}