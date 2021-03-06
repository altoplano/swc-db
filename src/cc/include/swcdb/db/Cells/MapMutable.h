/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */ 

#ifndef swcdb_db_Cells_MapMutable_h
#define swcdb_db_Cells_MapMutable_h

#include <mutex>
#include "swcdb/db/Columns/Schema.h"
#include "swcdb/db/Cells/Mutable.h"


namespace SWC { namespace DB { namespace Cells {


class ColCells final {

  public:

  typedef std::shared_ptr<ColCells> Ptr;

  const int64_t cid;

  static Ptr make(const int64_t cid, uint32_t versions, uint32_t ttl, 
                  Types::Column type);

  static Ptr make(const int64_t cid, Mutable& cells);


  ColCells(const int64_t cid, uint32_t versions, uint32_t ttl, 
           Types::Column type);

  ColCells(const int64_t cid, Mutable& cells);

  ~ColCells();

  DB::Cell::Key::Ptr get_first_key();
  
  DB::Cell::Key::Ptr get_key_next(const DB::Cell::Key& eval_key, 
                                  bool start_key=false);

  DynamicBuffer::Ptr get_buff(const DB::Cell::Key& key_start, 
                              const DB::Cell::Key& key_end, 
                              size_t buff_sz, bool& more);

  void add(const DB::Cells::Cell& cell);

  void add(const DynamicBuffer& cells);

  void add(const DynamicBuffer& cells, const DB::Cell::Key& upto_key,
                                       const DB::Cell::Key& from_key);

  const size_t size();

  const size_t size_bytes();

  const std::string to_string();

  private:
  std::mutex   m_mutex;
  Mutable      m_cells;

};



class MapMutable {
  public:
  
  typedef std::shared_ptr<MapMutable>                 Ptr;
  typedef std::unordered_map<int64_t, ColCells::Ptr>  Columns;
  
  MapMutable();

  virtual ~MapMutable();

  const bool create(Schema::Ptr schema);

  const bool create(int64_t cid, uint32_t versions, uint32_t ttl, 
                    Types::Column type);

  const bool create(const int64_t cid, Mutable& cells);

  const bool exists(int64_t cid);
  
  void add(const int64_t cid, const Cell& cell);

  ColCells::Ptr get_idx(size_t offset);

  ColCells::Ptr get_col(const int64_t cid);

  void pop(ColCells::Ptr& col);

  void pop(const int64_t cid, ColCells::Ptr& col);

  void remove(const int64_t cid);

  const size_t size();

  const size_t size(const int64_t cid);

  const size_t size_bytes();

  const size_t size_bytes(const int64_t cid);

  const std::string to_string();

  private:
  std::mutex   m_mutex;
  Columns      m_map;

};

}}}

#ifdef SWC_IMPL_SOURCE
#include "swcdb/db/Cells/MapMutable.cc"
#endif 

#endif // swcdb_db_Cells_MapMutable_h