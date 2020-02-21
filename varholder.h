#ifndef _VARHOLDER_H_INCLUDED_
#define _VARHOLDER_H_INCLUDED_

class VarHandlerBase {
  public:
    virtual void beginEdit() = 0;
    virtual void endEdit() = 0;
    virtual void adjust(int increment) = 0;
    virtual void printTo(char* buf) = 0;
};

template<class T> class VarHandler : VarHandlerBase {
    private:
      T * _original;
      const char* _name;
      static T _copy;
      static bool _editing;
    public:
      VarHandler<T>(const char* name, T* pdata, const T minV, const T maxV) {
        _original = pdata;
        _name = name;
      };

      void beginEdit() {
        _copy = *_original;
        _editing = true;
      }

      void endEdit(bool save) {
        _editing = false;
        if (save) {
          *_original = _copy;
        } 
        else {
          
        }
      }

      void printTo(char* buf) {
        String s = _editing ? new String(_copy) : new String(*_original);
        return;
      }
      
};

#endif
