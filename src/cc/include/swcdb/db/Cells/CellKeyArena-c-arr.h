/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swcdb_db_cells_CellKeyArena_h
#define swcdb_db_cells_CellKeyArena_h

#include "swcdb/core/PageArena.h"

namespace SWC { namespace DB { namespace Cell {
  

class KeyArena  {
  public:

  using Fraction = Mem::Item::Ptr;

  explicit KeyArena() : count(0), data(0) { }

  explicit KeyArena(const KeyArena &other) : data(0) {
    copy(other);
  }

  virtual ~KeyArena() {
    free();
  }

  void free() {
    if(data) {
      for(auto c=0; c<count; ++c)
        fraction(c)->release();
      delete [] data;
      data = 0;
      count = 0;
    }
  }

  void copy(const KeyArena &other) {
    free(); 
    if(count = other.count) {
      allocate();
      for(auto c=0; c < count; ++c)
        set_fraction(c, other.fraction(c)->use());
    } else {
      data = 0;
    }
  }

  const bool equal(const KeyArena &other) const {
    return count == other.count && 
           memcmp(data, other.data, count * _size) == 0;
  }

  const uint32_t size() const { 
    return count;
  }

  const bool empty() const { 
    return !count;
  }

  void add(const uint8_t* buf, const uint32_t len) {
    auto f = Mem::Item::make(buf, len);
    if(++count == 1) {
      allocate();
      set_fraction(0, f);
      return;
    }
    auto data_old = data;
    allocate();
    memcpy(data, data_old, (count - 1) * _size);
    set_fraction(count - 1, f);
    delete [] data_old;
  }

  void add(const Fraction fraction) {
    if(++count == 1) {
      allocate();
      set_fraction(0, fraction->use());
      return;
    }
    auto data_old = data;
    allocate();
    memcpy(data, data_old, (count - 1) * _size);
    set_fraction(count - 1, fraction->use());
    delete [] data_old;
  }

  void add(const std::string& fraction) {
    add((const uint8_t*)fraction.data(), fraction.length());
  }

  void add(const char* fraction) {
    add((const uint8_t*)fraction, strlen(fraction));
  }

  void add(const char* fraction, const uint32_t len) {
    add((const uint8_t*)fraction, len);
  }


  void insert(const uint32_t idx, const uint8_t* buf, const uint32_t len) {
    if(!count) {
      add(buf, len);
      return;
    } 
    auto data_old = data;
    ++count;
    allocate();
    if(idx)
      memcpy(data, data_old, idx * _size);
      
    set_fraction(idx, Mem::Item::make(buf, len));
    
    if(count-1 >= idx+1)
      memcpy(data+idx+1, data_old+idx, (count-1 - idx) * _size);
    delete [] data_old;
  }

  void insert(const uint32_t idx, const char* fraction, const uint32_t len) {
    insert(idx, (const uint8_t*)fraction, len);
  }

  void insert(const uint32_t idx, const std::string& fraction) {
    insert(idx, (const uint8_t*)fraction.data(), fraction.length());
  }

  void insert(const uint32_t idx, const char* fraction) {
    insert(idx, (const uint8_t*)fraction, strlen(fraction));
  }


  void set(const uint32_t idx, const uint8_t* buf, const uint32_t len) {
    set_fraction(idx, Mem::Item::make(buf, len));
  }

  void set(const uint32_t idx, const char* fraction, const uint32_t len) {
    set(idx, (const uint8_t*)fraction, len);
  }

  void set(const uint32_t idx, const std::string& fraction) {
    set(idx, (const uint8_t*)fraction.data(), fraction.length());
  }


  void remove(const uint32_t idx) {
    if(idx >= count)
      return;
    fraction(idx)->release();

    if(!--count) {
      delete [] data;
      data = nullptr;
      return;
    }

    auto data_old = data;
    allocate();
    if(idx)
      memcpy(data, data_old, idx * _size);
    if(count >= idx+1)
      memcpy(data+idx, data_old+idx+1, (count - idx) * _size);
    delete [] data_old;
  }


  const std::string_view get(const uint32_t idx) const {
    return fraction(idx)->to_string();
  }

  void get(const uint32_t idx, std::string& fraction) const {
    fraction.append(get(idx));
  }


  const bool align(const KeyArena& other, Condition::Comp comp) {
    bool chg;
    if(chg = empty()) {
      copy(other);
      return chg;
    }
    bool smaller = count < other.size();
    uint32_t min = smaller ? count : other.size();
    for(uint32_t c = 0; c < min; ++c) {
      auto r1 = fraction(c);
      auto r2 = other.fraction(c);
      if(Condition::condition((const uint8_t*)r1->data(), r1->size(),
                              (const uint8_t*)r2->data(), r2->size()
                              ) == comp) {
        r1->release();
        set_fraction(c, r2->use());
        chg = true;
      }
    }
    if(smaller) {
      for(uint32_t c = count; c < other.size(); ++c) {
        add(other.fraction(c));
        chg = true;
      }
    }
    return chg;
  }


  const uint32_t encoded_length() const {
    uint32_t len = Serialization::encoded_length_vi32(count);
    Fraction f;
    for(uint32_t c = 0; c < count; ++c) {
      f = fraction(c);
      len += Serialization::encoded_length_vi32(f->size()) + f->size();
    }
    return len;
  }
  
  void encode(uint8_t **bufp) const {
    Serialization::encode_vi32(bufp, count);
    uint32_t len;
    Fraction f;
    for(uint32_t c = 0; c < count; ++c) {
      Serialization::encode_vi32(bufp, len = (f = fraction(c))->size());
      memcpy(*bufp, f->data(), len);
      *bufp += len;
    }
  }

  void decode(const uint8_t **bufp, size_t* remainp) {
    free();
    if(count = Serialization::decode_vi32(bufp, remainp)) {
      allocate();
      uint32_t len;
      for(uint32_t c = 0; c < count; ++c) {
        len = Serialization::decode_vi32(bufp);
        set_fraction(c, Mem::Item::make(*bufp, len));
        *bufp += len;
      }
    }
  }

  const std::string to_string() const {
    std::string s("Key(");
    s.append("sz=");
    s.append(std::to_string(count));
    s.append(" fractions=[");
      for(uint32_t c = 0; c < count; ++c) {
      s.append(get(c));
      s.append(", ");
    }
    s.append("])");
    return s;
  }


  private:

  void allocate() {
    data = new Fraction[count];
  }

  Fraction fraction(uint32_t c) const {
    return *(data+c);
  }

  void set_fraction(uint32_t c, Fraction fraction) {
    *(data+c) = fraction;
  }

  static const uint8_t _size = sizeof(Fraction);
  
  uint32_t    count;
  Fraction*   data;

};

}}}

#endif