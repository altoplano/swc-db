/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */


#ifndef swc_ranger_Protocol_handlers_RangeQueryUpdate_h
#define swc_ranger_Protocol_handlers_RangeQueryUpdate_h

#include "swcdb/db/Protocol/Rgr/params/RangeQueryUpdate.h"
#include "swcdb/ranger/callbacks/RangeQueryUpdate.h"


namespace SWC { namespace Protocol { namespace Rgr { namespace Handler {


void range_query_update(ConnHandlerPtr conn, Event::Ptr ev) {
  int err = Error::OK;
  Params::RangeQueryUpdateReq params;
  Ranger::RangePtr range;
  //StaticBuffer::Ptr buffer;

  try {      
    const uint8_t *ptr = ev->data.base;
    size_t remain = ev->data.size;
    params.decode(&ptr, &remain);

    range = RangerEnv::columns()->get_range(err, params.cid, params.rid);
      
    if(range == nullptr || !range->is_loaded()){
      if(err == Error::OK)
        err = Error::RS_NOT_LOADED_RANGE;
    }
    if(err == Error::OK && !ev->data_ext.size) {
      err = Error::INVALID_ARGUMENT;
    }// else {
    //  buffer = std::make_shared<StaticBuffer>(ev->data_ext);
    //}
  } catch (Exception &e) {
    SWC_LOG_OUT(LOG_ERROR) << e << SWC_LOG_OUT_END;
    err = e.code();
  }

  try{
    auto cb = std::make_shared<Ranger::Callback::RangeQueryUpdate>(
      conn, ev);
    if(err) {
      cb->response(err);
      return;
    }
      
    range->add(new Ranger::Range::ReqAdd(ev->data_ext, cb));
  }
  catch (Exception &e) {
    SWC_LOG_OUT(LOG_ERROR) << e << SWC_LOG_OUT_END;
  }
  
}
  

}}}}

#endif // swc_ranger_Protocol_handlers_RangeQueryUpdate_h