/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swc_app_ranger_handlers_RangeLoad_h
#define swc_app_ranger_handlers_RangeLoad_h

#include "swcdb/db/Protocol/Rgr/params/RangeLoad.h"
#include "swcdb/ranger/callbacks/RangeLoaded.h"


namespace SWC { namespace Protocol { namespace Rgr { namespace Handler {


class RangeLoad : public AppHandler {
  public:

  RangeLoad(ConnHandlerPtr conn, Event::Ptr ev)
            : AppHandler(conn, ev) { }

  void run() override {

    try {

      const uint8_t *ptr = m_ev->data.base;
      size_t remain = m_ev->data.size;

      Params::RangeLoad params;
      params.decode(&ptr, &remain);

      if(params.schema != nullptr) {
        Env::Schemas::get()->replace(params.schema);
        if(!Env::RgrData::is_shuttingdown())
          SWC_LOGF(LOG_DEBUG, "updated %s", params.schema->to_string().c_str());
      }
      
      int err = Error::OK;
      Env::RgrColumns::get()->load_range(
        err,
        params.cid, params.rid, 
        std::make_shared<server::Rgr::Callback::RangeLoaded>(
          m_conn, m_ev, params.cid, params.rid)
      );
       
    }
    catch (Exception &e) {
      SWC_LOG_OUT(LOG_ERROR) << e << SWC_LOG_OUT_END;
    }
  
  }

};
  

}}}}

#endif // swc_app_ranger_handlers_RangeLoad_h