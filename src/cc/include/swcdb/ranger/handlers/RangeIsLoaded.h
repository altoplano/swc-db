/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swc_app_ranger_handlers_RangeIsLoaded_h
#define swc_app_ranger_handlers_RangeIsLoaded_h

#include "swcdb/db/Protocol/Rgr/params/RangeIsLoaded.h"


namespace SWC { namespace Protocol { namespace Rgr { namespace Handler {


class RangeIsLoaded : public AppHandler {
  public:

  RangeIsLoaded(ConnHandlerPtr conn, Event::Ptr ev)
               : AppHandler(conn, ev){ }

  void run() override {

    try {

      const uint8_t *ptr = m_ev->data.base;
      size_t remain = m_ev->data.size;

      Params::RangeIsLoaded params;
      params.decode(&ptr, &remain);

      int err = Error::OK;
      server::Rgr::Range::Ptr range =  Env::RgrColumns::get()->get_range(
        err, params.cid, params.rid, false);
      
      if(range != nullptr && range->is_loaded()){
        m_conn->response_ok(m_ev);
      } else {
        if(err == Error::OK)
          err = Error::RS_NOT_LOADED_RANGE;
        m_conn->send_error(err, "", m_ev);
      }
    }
    catch (Exception &e) {
      SWC_LOG_OUT(LOG_ERROR) << e << SWC_LOG_OUT_END;
    }
  
  }

};
  

}}}}

#endif // swc_app_ranger_handlers_RangeIsLoaded_h