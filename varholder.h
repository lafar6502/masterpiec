#ifndef _VARHOLDER_H_INCLUDED_
#define _VARHOLDER_H_INCLUDED_

template<class T> class CircularBuffer
{
  private:
    T* _buf;
    int16_t _bufLen;
    int16_t _tail;
    int16_t _head;
  
  public:
    CircularBuffer(T* buf, uint16_t bufLength) 
    {
      _buf = buf;
      _bufLen = bufLength;
      _head = _tail = 0; //head==tail -> empty, head=tail-1 -> full  
    };

    void Enqueue(const T& d) {
      _buf[_head] = d;
      _head = (_head + 1) % _bufLen;
      if (_tail == _head) { //head has trumped tail
        _tail = (_tail + 1) % _bufLen;
      }
    }

    const T& Dequeue() {
      if (_tail == _head) return NULL;
      int16_t t0 = _tail;
      _tail = (_tail + 1) % _bufLen;
      return _buf[t0];
    }

    uint16_t GetCount() {
      return _head >= _tail ? _head - _tail : _head - _tail + _bufLen;
    }

    const T* GetFirst() {
      if (_head == _tail) return NULL;
      return _buf + _tail;
    }
    
    const T* GetLast() {
      if (_head == _tail) return NULL;
      return _buf + (_head > 0 ? _head - 1 : _head - 1 + _bufLen);
    }

    const T* GetAt(uint16_t idx) {
      if (_head == _tail) return NULL;
      int16_t f = _tail + idx;
      return _buf + (f >= _bufLen? f - _bufLen : f);
    }
    
    void CopyTo(T* buf, uint16_t count) {
      int16_t tl = _tail;
      int16_t pos = 0;
      while(tl != _head && pos < count) {
        buf[pos++] = _buf[tl];
        tl = (tl + 1) % _bufLen;
      }  
    }

    bool IsEmpty() {
      return _tail == _head;
    }

    bool IsFull() {
      return  _tail == (_head + 1) % _bufLen;
    }
};

#endif
