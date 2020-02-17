/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swcdb_db_cells_CellKey_h
#define swcdb_db_cells_CellKey_h

#include <vector>
#include "swcdb/core/PageArena.h"
#include "swcdb/core/Serialization.h"
#include "swcdb/db/Cells/Comparators.h"


namespace SWC { namespace DB { namespace Cell {
  
using Fraction = Mem::Item::Ptr;

class Key final {
  public:

  typedef std::shared_ptr<Key>  Ptr;
  typedef std::vector<Fraction> Fractions;

  explicit Key() { }
  
  explicit Key(const Key &other) {
    copy(other);
  }
  
  virtual ~Key() {
    free();
  }

  Key operator=(const Key &other) = delete;

  void free() {
    if(empty())
      return;
    for(auto it = fractions.begin(); it < fractions.end(); ++it) {
      (*it)->release();
    }
    fractions.clear();
  }

  void copy(const Key &other) {
    free();
    fractions.assign(other.begin(), other.end());
    for(auto it = fractions.begin(); it < fractions.end(); ++it)
      (*it)->use();
  }

  const uint32_t size() const {
    return fractions.size();
  }

  const bool empty() const {
    return fractions.empty();
  }

  const bool equal(const Key &other) const {
    return fractions == other.fractions;
  }

  Fractions::const_iterator begin() const {
    return fractions.begin();
  }
  Fractions::const_iterator end() const {
    return fractions.end();
  }

  //add fraction
  void add(const uint8_t* buf, const uint32_t len) {
    fractions.push_back(Mem::Item::make(buf, len));
  }

  void add(Fraction fraction) {    
    fractions.push_back(fraction->use());
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
    fractions.insert(begin()+idx, Mem::Item::make(buf, len));
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
    auto& f = fractions[idx];
    f->release();
    f = Mem::Item::make(buf, len);
  }

  void set(const uint32_t idx, Fraction fraction) {    
    auto& f = fractions[idx];
    f->release();
    f = fraction->use();
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
    if(idx >= size())
      return;
    auto it = fractions.begin()+idx;
    if(!recursive) {
      (*it)->release();
      fractions.erase(it);
    } else {
      for(;it<fractions.end();++it) {
        (*it)->release();
        fractions.erase(it);
      }
    }
  }

  //get fraction
  const std::string_view get(const uint32_t idx) const {
    return fractions[idx]->to_string();
  }

  void get(const uint32_t idx, std::string& fraction) const {
    fraction.append(get(idx));
  }
  
  void get(const uint32_t idx, std::string_view& fraction) const {
    fraction = get(idx);
  }
  
  void get(const uint32_t idx, const char** fraction, uint32_t* len) const {
    auto f = fractions[idx];
    *fraction = (const char*)f->data();
    *len = f->size();
  }
  
  Fraction& operator[](uint32_t pos) {
    return fractions[pos];
  }

  //compare fraction
  const Condition::Comp compare(const Key& other, uint32_t max=0, 
                                bool empty_ok=false) const {
    Condition::Comp comp = Condition::EQ;
    bool on_fraction = max;
    auto f2 = other.begin();
    for(auto f1 = begin(); f1 < end() && f2 < other.end(); ++f1, ++f2) {
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
    auto f1 = begin();
    auto f2 = other.begin();
    for(; f1 < end() && f2 < other.end(); ++f1, ++f2) {
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
    auto f2 = other.begin();
    for(auto f1 = fractions.begin(); 
        f1 < fractions.end() && f2 < other.end(); ++f1, ++f2) {

      if(Condition::condition((const uint8_t*)(*f1)->data(), (*f1)->size(),
                              (const uint8_t*)(*f2)->data(), (*f2)->size()
                              ) == comp) {
        (*f1)->release();
        *f1 = (*f2)->use();
        chg = true;
      }
    }
    for(;f2 < other.end(); ++f2) {
      add(*f2);
      chg = true;
    }
    return chg;
  }

  const bool align(Key& start, Key& finish) const {
    bool chg = false;
    uint32_t c = 0;
    for(auto it = begin(); it < end(); ++it, ++c) {

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

  private:
  Fractions fractions;
};

}}}

#endif