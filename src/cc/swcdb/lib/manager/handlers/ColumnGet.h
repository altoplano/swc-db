/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swc_app_manager_handlers_ColumnGet_h
#define swc_app_manager_handlers_ColumnGet_h

#include "swcdb/lib/db/Protocol/Mngr/params/ColumnGet.h"
#include "swcdb/lib/db/Protocol/Mngr/req/MngrColumnGet.h"

namespace SWC { namespace Protocol { namespace Mngr { namespace Handler {


class ColumnGet : public AppHandler {
  public:

  ColumnGet(ConnHandlerPtr conn, EventPtr ev)
            : AppHandler(conn, ev){}

  DB::SchemaPtr get_schema(int &err, Params::ColumnGetReq params) {
    switch(params.flag) {
      case Params::ColumnGetReq::Flag::SCHEMA_BY_ID:
        return Env::Schemas::get()->get(params.cid);

      case Params::ColumnGetReq::Flag::SCHEMA_BY_NAME:
        return Env::Schemas::get()->get(params.name);

      case Params::ColumnGetReq::Flag::ID_BY_NAME:
        return Env::Schemas::get()->get(params.name);

      default:
        err = Error::COLUMN_UNKNOWN_GET_FLAG;
        return nullptr;
    }
  }

  void run() override {

    int err = Error::OK;
    Params::ColumnGetReq::Flag flag;

    try {

      const uint8_t *ptr = m_ev->payload;
      size_t remain = m_ev->payload_len;

      Params::ColumnGetReq req_params;
      req_params.decode(&ptr, &remain);
      flag = req_params.flag;
      
      DB::SchemaPtr schema = get_schema(err, req_params);
      
      if(schema != nullptr || err){
        response(err, flag, schema);
        return;
      }
      Env::Rangers::get()->is_active(err, 1);
      if(err) {
        response(err, flag, schema);
        return;
      }

      if(flag == Params::ColumnGetReq::Flag::ID_BY_NAME)
        req_params.flag = Params::ColumnGetReq::Flag::SCHEMA_BY_NAME;

      Env::MngrRole::get()->req_mngr_inchain(
        std::make_shared<Req::MngrColumnGet>(
          req_params,
          [ptr=this](int err, Params::ColumnGetRsp params){
            if(err == Error::OK && params.schema != nullptr){
              int tmperr;
              Env::Schemas::get()->add(tmperr, params.schema);
            }
            ptr->response(err, params.flag, params.schema);
          }
        )
      );
      return;

    } catch (Exception &e) {
      HT_ERROR_OUT << e << HT_END;
      err = e.code();
    }
    
    response(err, flag, nullptr);
  }

  void response(int err, Params::ColumnGetReq::Flag flag, 
                DB::SchemaPtr schema){

    if(err == Error::OK && schema == nullptr)
      err = Error::COLUMN_SCHEMA_NAME_NOT_EXISTS;

    try {
      CommHeader header;
      header.initialize_from_request_header(m_ev->header);
      CommBufPtr cbp;

      if(err == Error::OK) {
        Params::ColumnGetRsp rsp_params(flag, schema);
        cbp = std::make_shared<CommBuf>(header, 4+rsp_params.encoded_length());
        cbp->append_i32(err);
        rsp_params.encode(cbp->get_data_ptr_address());

      } else {
        cbp = std::make_shared<CommBuf>(header, 4);
        cbp->append_i32(err);
      }

      m_conn->send_response(cbp);
    }
    catch (Exception &e) {
      HT_ERROR_OUT << e << HT_END;
    }
  }

};
  
}}}}

#endif // swc_app_manager_handlers_ColumnGet_h