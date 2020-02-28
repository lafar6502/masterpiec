#ifndef _VARHOLDER_H_INCLUDED_
#define _VARHOLDER_H_INCLUDED_

template<class T> class CircularBuffer
{
  private:
    T* _buf;
    uint16_t _bufLen;
    uint16_t _tail;
    uint16_t _head;
  
  public:
    //holds bufLength - 1 elements
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
      /*Serial.print("h");
      Serial.print(_head);
      Serial.print(" t");
      Serial.println(_tail);*/
    }

    const T& Dequeue() {
      if (_tail == _head) return NULL;
      uint16_t t0 = _tail;
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

    const T* GetAt(int16_t idx) {
      if (_head == _tail) return NULL;
      if (idx >= 0) {
        uint16_t f = _tail + idx;
        return _buf + (f >= _bufLen? f - _bufLen : f);  
      }
      else {
        return GetAt(idx + GetCount());
      }
    }
    
    void CopyTo(T* buf, int16_t count) {
      uint16_t tl = _tail;
      uint16_t pos = 0;
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
