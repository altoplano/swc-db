
/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */


#include "swcdb/db/Protocol/Mngr/params/ColumnList.h"
#include "swcdb/core/Serialization.h"


namespace SWC { namespace Protocol { namespace Mngr { namespace Params {

ColumnListReq::ColumnListReq() { }

ColumnListReq::~ColumnListReq() { }

uint8_t ColumnListReq::encoding_version() const {
  return 1;
}
  
size_t ColumnListReq::encoded_length_internal() const {
  return 0;
}
  
void ColumnListReq::encode_internal(uint8_t **bufp) const {
}
  
void ColumnListReq::decode_internal(uint8_t version, const uint8_t **bufp, 
                                    size_t *remainp) {
}


ColumnListRsp::ColumnListRsp() { }

ColumnListRsp::~ColumnListRsp() { }


uint8_t ColumnListRsp::encoding_version() const {
  return 1;
}
  
size_t ColumnListRsp::encoded_length_internal() const {
  size_t sz = Serialization::encoded_length_vi64(schemas.size());
  for (auto schema : schemas)
    sz += schema->encoded_length();
  return sz;
}
  
void ColumnListRsp::encode_internal(uint8_t **bufp) const {
  Serialization::encode_vi64(bufp, schemas.size());
  for(auto schema : schemas)
    schema->encode(bufp);
}
  
void ColumnListRsp::decode_internal(uint8_t version, const uint8_t **bufp, 
                                    size_t *remainp) {
  size_t sz = Serialization::decode_vi64(bufp, remainp);
  schemas.clear();
  schemas.resize(sz);
  for(auto i=0;i<sz;++i) 
    schemas[i].reset(new DB::Schema(bufp, remainp));
}


}}}}
