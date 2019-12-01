/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swc_app_ranger_handlers_ColumnUpdate_h
#define swc_app_ranger_handlers_ColumnUpdate_h

#include "swcdb/db/Protocol/Rgr/params/ColumnUpdate.h"


namespace SWC { namespace Protocol { namespace Rgr { namespace Handler {


class ColumnUpdate : public AppHandler {
  public:

  ColumnUpdate(ConnHandlerPtr conn, Event::Ptr ev)
              : AppHandler(conn, ev) { }

  void run() override {

    try {

      const uint8_t *ptr = m_ev->data.base;
      size_t remain = m_ev->data.size;

      Params::ColumnUpdate params;
      params.decode(&ptr, &remain);

      Env::Schemas::get()->replace(params.schema);
      if(!Env::RgrData::is_shuttingdown())
        SWC_LOGF(LOG_DEBUG, "updated %s", params.schema->to_string().c_str());
      
      m_conn->response_ok(m_ev);
    }
    catch (Exception &e) {
      SWC_LOG_OUT(LOG_ERROR) << e << SWC_LOG_OUT_END;
    }
  
  }

};
  

}}}}

#endif // swc_app_ranger_handlers_ColumnUpdate_h