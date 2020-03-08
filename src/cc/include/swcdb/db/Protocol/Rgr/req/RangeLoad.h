
/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */ 

#ifndef swc_lib_db_protocol_rgr_req_RangeLoad_h
#define swc_lib_db_protocol_rgr_req_RangeLoad_h

#include "swcdb/db/Protocol/Rgr/params/RangeLoad.h"

namespace SWC { namespace Protocol { namespace Rgr { namespace Req {


class RangeLoad : public client::ConnQueue::ReqBase {
  public:

  RangeLoad(Manager::Ranger::Ptr rgr, Manager::Range::Ptr range,
            DB::Schema::Ptr schema) 
            : client::ConnQueue::ReqBase(false), 
              rgr(rgr), range(range), schema_revision(schema->revision) {
    cbp = CommBuf::make(
      Params::RangeLoad(range->cfg->cid, range->rid, schema)
    );
    cbp->header.set(RANGE_LOAD, 60000);
  }
  
  virtual ~RangeLoad() { }

  void handle(ConnHandlerPtr conn, Event::Ptr& ev) override {
      
    if(was_called)
      return;
    was_called = true;

    if(!valid() || ev->type == Event::Type::DISCONNECT) {
      handle_no_conn();
      return;
    }

    if(ev->header.command == RANGE_LOAD){
      int err = ev->error != Error::OK? ev->error: ev->response_code();
      if(err != Error::OK){
        loaded(err, false, DB::Cells::Interval()); 
        return; 
      }
      
      const uint8_t *ptr = ev->data.base+4;
      size_t remain = ev->data.size-4;
      Params::RangeLoaded params;
      params.decode(&ptr, &remain);
      loaded(err, false, params.interval); 
    }
  }

  bool valid() override {
    return !range->deleted();
  }
  
  void handle_no_conn() override {
    loaded(Error::COMM_NOT_CONNECTED, true, DB::Cells::Interval());
  }

  void loaded(int err, bool failure, const DB::Cells::Interval& intval);


  private:

  Manager::Ranger::Ptr rgr;
  Manager::Range::Ptr  range;
  int64_t              schema_revision;
   
};

}}}}

#endif // swc_lib_db_protocol_rgr_req_RangeLoad_h
