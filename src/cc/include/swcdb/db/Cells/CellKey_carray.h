/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swcdb_db_cells_CellKey_h
#define swcdb_db_cells_CellKey_h

#include <vector>
#include "swcdb/core/PageArena.h"
#include "swcdb/core/Serialization.h"
#include "swcdb/core/Comparators.h"


namespace SWC { namespace DB { namespace Cell {
  
using Fraction = Mem::Item::Ptr;

class Key final {
  private:

  typedef Fraction*    Fractions;
  Fraction*            _fractions;
  uint32_t             _size;
  static const uint8_t  _szof = sizeof(Fractions);

  void _add(Fraction ptr) {
    auto old = _fractions;
    _fractions = (Fractions)memcpy(new Fraction[++_size], old, _size*_szof);
    delete [] old;
    *(_fractions + _size - 1) = ptr;
  }

  void _insert(const uint32_t idx, Fraction ptr) {
    if(!_size)
      return _add(ptr);

    auto old = _fractions;
    _fractions = new Fraction[++_size];
    if(idx)
      memcpy(_fractions, old, idx*_szof);
      
    if(_size-1 >= idx+1)
      memcpy(_fractions+idx+1, old+idx, (_size-1 - idx) * _szof);
    delete [] old;

    *(_fractions + idx) = ptr;
  }

  void _set(const uint32_t idx, Fraction ptr) {
    auto f = _fractions + idx;
    (*f)->release();
    *f = ptr;
  }
  

  public:

  typedef std::shared_ptr<Key>  Ptr;

  explicit Key(): _fractions(0), _size(0)  { }
  
  explicit Key(const Key &other): _fractions(0), _size(0) {
    copy(other);
  }
  
  virtual ~Key() {
    free();
  }

  Key operator=(const Key &other) = delete;

  void free() {
    if(!_size || !_fractions)
      return;
    do {
      (*(_fractions+(--_size)))->release();
    } while (_size);
    delete [] _fractions;
    _size = 0;
    _fractions = nullptr;
  }

  void copy(const Key &other) {
    free();
    if(!other._size || !other._fractions)
      return;

    
    auto ptr = _fractions = new Fraction[_size = other._size];
    auto ptr2 = other._fractions;
    auto end = ptr2 + _size;
    do {
      *ptr-- = (*ptr2)->use();
    } while(--ptr2 < end);
  }

  const uint32_t size() const {
    return _size;
  }

  const bool empty() const {
    return !_size;
  }

  const bool equal(const Key &other) const {
    return Condition::eq(
      (const uint8_t*)_fractions, _size*_szof, 
      (const uint8_t*)other._fractions, other._size*_szof);
  }

  Fractions begin() const {
    return _fractions;
  }
  Fractions end() const {
    return _fractions + _size;
  }
  
  Fraction& operator[](uint32_t pos) {
    return *(begin()+pos);
  }
  const Fraction& operator[](uint32_t pos) const {
    return *(begin()+pos);
  }

  //add fraction
  void add(const uint8_t* buf, const uint32_t len) {
    _add(Mem::Item::make(buf, len));
  }

  void add(Fraction fraction) {    
    _add(fraction->use());
  }

  void add(const std::string& fraction) {
    add((const uint8_t*)fraction.data(), fraction.length());
  }

  void add(const std::string_view& fraction) {
    add((const uint8_t*)fraction.data(), fraction.length());
  }

  void add(const char* fraction) {
    add((const uint8_t*)fraction, strlen(fraction));
  }

  void add(const char* fraction, const uint32_t len) {
    add((const uint8_t*)fraction, len);
  }

  //insert fraction
  void insert(const uint32_t idx, const uint8_t* buf, const uint32_t len) {
    _insert(idx, Mem::Item::make(buf, len));
  }

  void insert(const uint32_t idx, const char* fraction, const uint32_t len) {
    insert(idx, (const uint8_t*)fraction, len);
  }

  void insert(const uint32_t idx, const std::string& fraction) {
    insert(idx, (const uint8_t*)fraction.data(), fraction.length());
  }

  void insert(const uint32_t idx, const std::string_view& fraction) {
    insert(idx, (const uint8_t*)fraction.data(), fraction.length());
  }

  void insert(const uint32_t idx, const char* fraction) {
    insert(idx, (const uint8_t*)fraction, strlen(fraction));
  }

  //set fraction
  void set(const uint32_t idx, const uint8_t* buf, const uint32_t len) {
    _set(idx, Mem::Item::make(buf, len));
  }

  void set(const uint32_t idx, Fraction fraction) {    
    _set(idx, fraction->use());
  }

  void set(const uint32_t idx, const char* fraction, const uint32_t len) {
    set(idx, (const uint8_t*)fraction, len);
  }

  void set(const uint32_t idx, const std::string& fraction) {
    set(idx, (const uint8_t*)fraction.data(), fraction.length());
  }

  void set(const uint32_t idx, const std::string_view& fraction) {
    set(idx, (const uint8_t*)fraction.data(), fraction.length());
  }

  //remove fraction
  void remove(const uint32_t idx, bool recursive=false) {
    if(idx >= _size)
      return;
    (*this)[idx]->release();

    if(!--_size) {
      delete [] _fractions;
      _fractions = nullptr;
      return;
    }

    auto old = _fractions;
    _fractions = new Fraction[_size];
    if(idx)
      memcpy(_fractions, old, idx * _szof);

    if(recursive) {
      uint32_t size = _size - 1;
      do {
        (*(old+(--size)))->release();
      } while (size);

    } else if(_size >= idx+1) {
      memcpy(_fractions+idx, old+idx+1, (_size - idx) * _szof);
    }
    delete [] old;
  }

  //get fraction
  const std::string_view get(const uint32_t idx) const {
    return (*this)[idx]->to_string();
  }

  void get(const uint32_t idx, std::string& fraction) const {
    fraction.append(get(idx));
  }
  
  void get(const uint32_t idx, std::string_view& fraction) const {
    fraction = get(idx);
  }
  
  void get(const uint32_t idx, const char** fraction, uint32_t* len) const {
    auto f = (*this)[idx];
    *fraction = (const char*)f->data();
    *len = f->size();
  }

  //compare fraction
  const Condition::Comp compare(const Key& other, uint32_t max=0, 
                                bool empty_ok=false) const {
    Condition::Comp comp = Condition::EQ;
    bool on_fraction = max;
    auto f1_end = end();
    auto f2 = other.begin();
    auto f2_end = other.end();
    for(auto f1 = begin(); f1 < f1_end && f2 < f2_end; ++f1, ++f2) {
      if(!(*f1)->size() && empty_ok)
        continue;

      if((comp = Condition::condition((*f1)->data(), (*f1)->size(), 
                                      (*f2)->data(), (*f2)->size()))
                  != Condition::EQ || (on_fraction && !--max))
        return comp;
    }
    return size() == other.size() 
          ? comp 
          : (size() > other.size() ? Condition::LT : Condition::GT);
  }

  const bool compare(const Key& other, Condition::Comp break_if,
                     uint32_t max=0, bool empty_ok=false) const {
    bool on_fraction = max;
    auto f1_end = end();
    auto f2 = other.begin();
    auto f2_end = other.end();
    for(auto f1 = begin(); f1 < f1_end && f2 < f2_end; ++f1, ++f2) {
      if(!(*f1)->size() && empty_ok)
        continue;

      if(Condition::condition((*f1)->data(), (*f1)->size(), 
                              (*f2)->data(), (*f2)->size()) == break_if)
        return false;
      if(on_fraction && !--max)
        return true;
    }
    return size() == other.size() 
          ? true 
          : (size() > other.size() 
            ? break_if != Condition::LT 
            : break_if != Condition::GT);
  }

  //align other Key
  const bool align(const Key& other, Condition::Comp comp) {
    bool chg;
    if(chg = empty()) {
      if(chg = !other.empty())
        copy(other);
      return chg;
    }
    auto f1_end = end();
    auto f2 = other.begin();
    auto f2_end = other.end();
    for(auto f1 = begin(); f1 < f1_end && f2 < f2_end; ++f1, ++f2) {

      if(Condition::condition((const uint8_t*)(*f1)->data(), (*f1)->size(),
                              (const uint8_t*)(*f2)->data(), (*f2)->size()
                              ) == comp) {
        (*f1)->release();
        *f1 = (*f2)->use();
        chg = true;
      }
    }
    for(;f2 < f2_end; ++f2) {
      add(*f2);
      chg = true;
    }
    return chg;
  }

  const bool align(Key& start, Key& finish) const {
    bool chg = false;
    uint32_t c = 0;
    auto it_end = end();
    for(auto it = begin(); it < it_end; ++it, ++c) {

      if(c == start.size()) {
        start.add(*it);
        chg = true;
      } else if(Condition::condition(
                  start[c]->data(), start[c]->size(),
                  (*it)->data(), (*it)->size()
              ) == Condition::LT) {
        start.set(c, *it);
        chg = true;
      }

      if(c == finish.size()) {
        finish.add(*it);
        chg = true;
      } else if(Condition::condition(
                  finish[c]->data(), finish[c]->size(),
                  (*it)->data(), (*it)->size()
                ) == Condition::GT) {
        finish.set(c, *it);
        chg = true;
      }
    }
    return chg;
  }

  //Serialization
  const uint32_t encoded_length() const {
    uint32_t len = Serialization::encoded_length_vi32(size());
    for(auto it = begin(); it < end(); ++it)
      len += Serialization::encoded_length_vi32((*it)->size()) + (*it)->size();
    return len;
  }
  
  void encode(uint8_t **bufp) const {
    Serialization::encode_vi32(bufp, size());
    uint32_t len;
    for(auto it = begin(); it < end(); ++it) {
      Serialization::encode_vi32(bufp, len = (*it)->size());
      memcpy(*bufp, (*it)->data(), len);
      *bufp += len;
    }
  }

  void decode(const uint8_t **bufp, size_t* remainp, bool owner = true) {
    free();
    uint32_t len;
    for(auto c = Serialization::decode_vi32(bufp, remainp); c;--c) {
      len = Serialization::decode_vi32(bufp, remainp);
      add(*bufp, len);
      *bufp += len;
      *remainp -= len;
    }
  }

  //from/to std::vector<std::string>
  void convert_to(std::vector<std::string>& key) const {
    key.clear();
    key.resize(size());
    auto f = key.begin();
    for(auto it = begin(); it<end(); ++it, ++f)
      f->append((const char*)(*it)->data(), (*it)->size());
  }

  void read(const std::vector<std::string>& key)  {
    free();
    for(auto it = key.begin(); it<key.end(); ++it)
      add(*it);
  }

  const bool equal(const std::vector<std::string>& key) const {
    if(key.size() != size())
      return false;
    auto f = key.begin(); 
    for(auto it = begin(); it<end(); ++it, ++f) {
      if((*it)->size() != f->length() || 
         memcmp((*it)->data(), f->data(), f->length()) != 0)
        return false;
    }
    return true;
  }

  //Output
  const std::string to_string() const {
    std::string s("Key(");
    s.append("sz=");
    s.append(std::to_string(size()));
    s.append(" fractions=[");
    for(auto it = begin(); it < end();) {
      s.append((*it)->to_string());
      if(++it < end())
        s.append(", ");
    }
    s.append("])");
    return s;
  }

  void display(std::ostream& out, bool pretty=false) const {
    out << "["; 
    if(empty()) {
      out << "]"; 
      return;
    }
      
    uint32_t len;
    const uint8_t* ptr;
    char hex[2];
    for(auto it = begin(); it < end();) {
      ptr = (*it)->data();
      for(len = (*it)->size(); len--; ++ptr) {
        if(pretty && (*ptr < 32 || *ptr > 126)) {
          sprintf(hex, "%X", *ptr);
          out << "0x" << hex;
        } else
          out << *ptr; 
      }
      if(++it < end())
        out << ", "; 
    }
    out << "]"; 
    
  }

  void display_details(std::ostream& out, bool pretty=false) const {
    out << "size=" << size() << " fractions=[";
    uint32_t len;
    const uint8_t* ptr;
    char hex[2];
    for(auto it = begin(); it < end();) {
      out << '"';
      ptr = (*it)->data();
      for(len = (*it)->size(); len--; ++ptr) {
        if(pretty && (*ptr < 32 || *ptr > 126)) {
          sprintf(hex, "%X", *ptr);
          out << "0x" << hex;
        } else
          out << *ptr; 
      }
      out << '"';
      if(++it < end())
        out << ", "; 
    }
    out << "]"; 
  }

};

}}}

#endif