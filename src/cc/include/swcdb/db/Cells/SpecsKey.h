/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swcdb_db_cells_SpecsKey_h
#define swcdb_db_cells_SpecsKey_h

#include "swcdb/db/Cells/Cell.h"

namespace SWC { namespace DB { namespace Specs {
      

struct Fraction final {
  Condition::Comp comp;
  std::string     value;

  bool operator==(const Fraction &other) const {
    return other.comp == comp && value.length() == other.value.length() && 
           memcmp(value.data(), other.value.data(), value.length()) == 0;
  }
  
  const uint32_t encoded_length() const {
    return Serialization::encoded_length_vi32(value.size()) + value.size();
  }
  
  void encode(uint8_t **bufp) const {
    Serialization::encode_i8(bufp, (uint8_t)comp);
    Serialization::encode_vi32(bufp, value.size());
    memcpy(*bufp, value.data(), value.size());
    *bufp += value.size();
  }

  void decode(const uint8_t **bufp, size_t* remainp) {
    value.clear();
    comp = (Condition::Comp)Serialization::decode_i8(bufp, remainp);
    uint32_t len = Serialization::decode_vi32(bufp, remainp);
    value.append((const char*)*bufp, len);
    *bufp += len;
    *remainp -= len;
  }
};


class Key : public std::vector<Fraction> {
  public:

  using std::vector<Fraction>::vector;
  using std::vector<Fraction>::insert;

  typedef std::shared_ptr<Key> Ptr;

  explicit Key(const DB::Cell::Key &cell_key, Condition::Comp comp) {
    set(cell_key, comp);
  }

  void free() {
    clear();
  }

  void copy(const Key &other) {
    clear();
    assign(other.begin(), other.end());
  }

  const bool equal(const Key &other) const {
    return *this == other;
  }

  void set(const DB::Cell::Key &cell_key, Condition::Comp comp) {
    clear();
    resize(cell_key.size());
    auto it2=cell_key.begin();
    for(auto it=begin(); it < end(); ++it, ++it2 ) {
      it->comp = comp;
      it->value.append((const char*)(*it2)->data(), (*it2)->size());
    }
  }

  void set(int32_t idx, Condition::Comp comp) {
    if(empty())
      return;
    (begin()+idx)->comp = comp;
  }

  void add(const char* buf, uint32_t len, Condition::Comp comp) {
    Fraction& f = emplace_back();
    f.comp = comp;
    f.value.append(buf, len);
  }

  void add(const std::string& fraction, Condition::Comp comp) {
    add(fraction.data(), fraction.length(), comp);
  }

  void add(const std::string_view& fraction, Condition::Comp comp) {
    add(fraction.data(), fraction.length(), comp);
  }

  void add(const char* fraction, Condition::Comp comp) {
    add(fraction, strlen(fraction), comp);
  }

  void add(const uint8_t* fraction, uint32_t len, Condition::Comp comp) {
    add((const char*)fraction, len, comp);
  }
  

  void insert(uint32_t idx, const char* buf, uint32_t len, 
              Condition::Comp comp) {
    auto it = emplace(begin()+idx);
    it->comp = comp;
    it->value.append(buf, len);
  }

  void insert(uint32_t idx, const std::string& fraction, 
              Condition::Comp comp) {
    insert(idx, fraction.data(), fraction.length(), comp);
  }

  void insert(uint32_t idx, const std::string_view& fraction, 
              Condition::Comp comp) {
    insert(idx, fraction.data(), fraction.length(), comp);
  }

  void insert(uint32_t idx, const uint8_t* fraction, uint32_t len,
              Condition::Comp comp) {
    insert(idx, (const char*)fraction, len, comp);
  }

  void insert(uint32_t idx, const char* fraction, Condition::Comp comp) {
    insert(idx, fraction, strlen(fraction), comp);
  }


  const std::string_view get(const uint32_t idx, Condition::Comp& comp) const {
    auto& f = (*this)[idx];
    comp = f.comp;
    return f.value;
  }

  const std::string_view get(const uint32_t idx) const {
    return (*this)[idx].value;
  }

  void get(DB::Cell::Key& key) const {
    key.free();
    if(!empty()) 
      for(auto it=begin(); it < end(); ++it)
        key.add(it->value);
  }

  void remove(uint32_t idx, bool recursive=false) {
    if(recursive)
      erase(begin()+idx, end());
    else
      erase(begin()+idx);
  }
  
  const bool is_matching(const DB::Cell::Key &other) const {
    Condition::Comp comp = Condition::NONE;

    auto it_other = other.begin();
    for(auto it = begin(); 
        it < end() && it_other < other.end() &&
        Condition::is_matching(
          comp = it->comp, 
          (const uint8_t*)it->value.data(), it->value.size(),
          (*it_other)->data(), (*it_other)->size()
        );  it++, it_other++);

    if(size() == other.size()) 
      return true;

    switch(comp) {
      case Condition::LT:
      case Condition::LE:
        return empty() || size() > other.size();
      case Condition::GT:
      case Condition::GE:
        return empty() || size() < other.size();
      case Condition::PF:
      case Condition::RE:
        return size() < other.size();
      case Condition::NE:
      case Condition::NONE:
        return true;          
      default: // Condition::EQ:
        return false;
    }
  }

  const uint32_t encoded_length() const {
    uint32_t len = Serialization::encoded_length_vi32(size()) + size();
    for(auto it = begin(); it < end(); ++it)
      len += it->encoded_length();
    return len;
  }
  
  void encode(uint8_t **bufp) const {
    Serialization::encode_vi32(bufp, size());
    for(auto it = begin(); it < end(); ++it) 
      it->encode(bufp);
  }

  void decode(const uint8_t **bufp, size_t* remainp, bool owner = true) {
    clear();
    resize(Serialization::decode_vi32(bufp, remainp));
    for(auto it = begin(); it < end(); ++it)
      it->decode(bufp, remainp);
  }

  const std::string to_string() const {
    std::string s("Key(size=");
    s.append(std::to_string(size()));
    s.append(" fractions=[");

    for(auto it = begin(); it < end();) {
      s.append(Condition::to_string(it->comp));
      s.append("(");
      s.append(it->value);
      s.append(")");
      if(++it < end())
        s.append(",");
    }
    s.append("])");
    return s;
  }

  void display(std::ostream& out) const {
    out << "size=" << size() << " fractions=[";
    for(auto it = begin(); it < end();) {
      out << Condition::to_string(it->comp);
      out << '"' << it->value << '"';
      if(++it < end())
        out << ", "; 
    }
    out << "]"; 
  }

};


}}}
#endif
