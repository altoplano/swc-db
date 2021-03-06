/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */


#ifndef swcdb_db_Columns_Schemas_h
#define swcdb_db_Columns_Schemas_h

#include "swcdb/db/Columns/Schema.h"

#include <vector>
#include <unordered_map>
#include <shared_mutex>

namespace SWC { namespace DB {

class Schemas final {
  public:

  Schemas();

  ~Schemas();
  
  void add(int &err, Schema::Ptr schema);

  void remove(int64_t cid);

  void replace(Schema::Ptr schema);

  Schema::Ptr get(int64_t cid);
  
  Schema::Ptr get(const std::string &name);

  void all(std::vector<Schema::Ptr> &entries);

  private:
  std::shared_mutex                         m_mutex;
  std::unordered_map<int64_t, Schema::Ptr>  m_map;
};



}}


#ifdef SWC_IMPL_SOURCE
#include "swcdb/db/Columns/Schemas.cc"
#endif 

#endif // swcdb_db_Columns_Schemas_h