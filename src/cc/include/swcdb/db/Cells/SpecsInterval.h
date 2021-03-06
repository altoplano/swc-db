/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */


#ifndef swcdb_db_cells_SpecsInterval_h
#define swcdb_db_cells_SpecsInterval_h


#include <memory>

#include "swcdb/db/Cells/SpecsKey.h"
#include "swcdb/db/Cells/SpecsTimestamp.h"
#include "swcdb/db/Cells/SpecsValue.h"
#include "swcdb/db/Cells/SpecsFlags.h"
#include "swcdb/db/Cells/Cell.h"


namespace SWC { namespace DB { namespace Specs {

class Interval {
  public:

  typedef std::shared_ptr<Interval> Ptr;

  static Ptr make_ptr();

  static Ptr make_ptr(
      const Key& key_start, const Key& key_finish, const Value& value,
      const Timestamp& ts_start, const Timestamp& ts_finish, 
      const Flags& flags=Flags());
  
  static Ptr make_ptr(
      const Cell::Key& range_begin, const Cell::Key& range_end,
      const Key& key_start, const Key& key_finish, 
      const Value& value, 
      const Timestamp& ts_start, const Timestamp& ts_finish, 
      const Flags& flags=Flags());
  
  static Ptr make_ptr(const uint8_t **bufp, size_t *remainp);

  static Ptr make_ptr(const Interval& other);

  static Ptr make_ptr(Ptr other);
  

  explicit Interval();

  explicit Interval(const Cell::Key& range_begin, const Cell::Key& range_end);

  explicit Interval(const Key& key_start, const Key& key_finish, 
                    const Value& value, 
                    const Timestamp& ts_start, const Timestamp& ts_finish, 
                    const Flags& flags=Flags());

  explicit Interval(const Cell::Key& range_begin, const Cell::Key& range_end, 
                    const Key& key_start, const Key& key_finish, 
                    const Value& value, 
                    const Timestamp& ts_start, const Timestamp& ts_finish, 
                    const Flags& flags=Flags());
  
  explicit Interval(const uint8_t **bufp, size_t *remainp);

  explicit Interval(const Interval& other);

  void copy(const Interval& other);

  virtual ~Interval();
  
  void free() ;

  //void expand(const Cells::Cell& cell);

  bool equal(const Interval& other) const;

  const bool is_matching(const Cell::Key& key, 
                         int64_t timestamp, bool desc) const;

  const bool is_matching(int64_t timestamp, bool desc) const;

  const bool is_matching(const Cells::Cell& cell) const;

  const bool is_matching(const Cells::Cell& cell, Types::Column typ) const;

  const bool is_matching_begin(const DB::Cell::Key& key) const;

  const bool is_matching_end(const DB::Cell::Key& key) const;

  const size_t encoded_length() const;

  void encode(uint8_t **bufp) const;

  void decode(const uint8_t **bufp, size_t *remainp);
  
  void apply_possible_range(DB::Cell::Key& begin, DB::Cell::Key& end) const;

  void apply_possible_range_begin(DB::Cell::Key& begin) const;

  void apply_possible_range_end(DB::Cell::Key& end) const;

  const std::string to_string() const;

  void display(std::ostream& out, bool pretty=false, 
               std::string offset = "") const;

  Cell::Key  range_begin, range_end, range_offset;
  Key        key_start, key_finish;
  Value      value;
  Timestamp  ts_start, ts_finish;
  Flags      flags;

  bool       key_eq;

  Cell::Key  offset_key;
  int64_t    offset_rev;
};

}}}

#ifdef SWC_IMPL_SOURCE
#include "swcdb/db/Cells/SpecsInterval.cc"
#endif 

#endif