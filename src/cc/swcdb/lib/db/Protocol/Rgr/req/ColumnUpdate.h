
/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */ 

#ifndef swc_lib_db_protocol_rgr_req_ColumnUpdate_h
#define swc_lib_db_protocol_rgr_req_ColumnUpdate_h

#include "../params/ColumnUpdate.h"


namespace SWC { namespace Protocol { namespace Rgr { namespace Req {

class ColumnUpdate : public Common::Req::ConnQueue::ReqBase {
  public:

  ColumnUpdate(server::Mngr::RgrStatusPtr rgr, DB::SchemaPtr schema) 
              : Common::Req::ConnQueue::ReqBase(false), 
                rgr(rgr), schema(schema) {
    Params::ColumnUpdate params(schema);
    CommHeader header(SCHEMA_UPDATE, 60000);
    cbp = std::make_shared<CommBuf>(header, params.encoded_length());
    params.encode(cbp->get_data_ptr_address());
  }
  
  virtual ~ColumnUpdate() { }

  void handle(ConnHandlerPtr conn, EventPtr &ev) override {
      
    if(was_called)
      return;
    was_called = true;

    if(ev->type == Event::Type::DISCONNECT) {
      handle_no_conn();
      return;
    }

    if(ev->header.command == SCHEMA_UPDATE){
      updated(ev->error != Error::OK? ev->error: response_code(ev), false);
      return; 
    }
  }
  
  void handle_no_conn() override {
    updated(Error::COMM_NOT_CONNECTED, true);
  }

  void updated(int err, bool failure);

  private:

  server::Mngr::RgrStatusPtr  rgr;
  DB::SchemaPtr               schema; 
   
};

}}}}

#endif // swc_lib_db_protocol_rgr_req_ColumnUpdate_h