/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swc_app_manager_handlers_RsGet_h
#define swc_app_manager_handlers_RsGet_h

#include "swcdb/lib/db/Protocol/params/MngrRsGet.h"


namespace SWC { namespace server { namespace Mngr {

namespace Handler {


class RsGet : public AppHandler {
  public:

  RsGet(ConnHandlerPtr conn, EventPtr ev) : AppHandler(conn, ev){ }

  void run() override {

    Protocol::Params::MngrRsGetRsp rsp_params;
    
    try {

      const uint8_t *ptr = m_ev->payload;
      size_t remain = m_ev->payload_len;

      Protocol::Params::MngrRsGetReq params;
      params.decode(&ptr, &remain);

      Env::RangeServers::get()->is_active(rsp_params.err, params.cid); 
      if(rsp_params.err != Error::OK)
        goto send_response;

      auto col = Env::MngrColumns::get()->get_column(rsp_params.err, params.cid, false);
      if(rsp_params.err != Error::OK)
        goto send_response;

      col->state(rsp_params.err);
      if(rsp_params.err != Error::OK)
        goto send_response;
    
      server::Mngr::RangePtr range;
      if(params.rid == 0)
        range = col->get_range(
          rsp_params.err, params.interval, rsp_params.next_key);
      else 
        range = col->get_range(rsp_params.err, params.rid);
      
      if(range == nullptr) {
        rsp_params.err = Error::RANGE_NOT_FOUND;
        goto send_response;
      }

      Env::RangeServers::get()->rs_get(
        range->get_rs_id(), rsp_params.endpoints);
      if(rsp_params.endpoints.empty()) {
        rsp_params.err = Error::RS_NOT_READY;
        goto send_response;
      }

      rsp_params.cid = range->cid;
      rsp_params.rid = range->rid;

    } catch (Exception &e) {
      HT_ERROR_OUT << e << HT_END;
      rsp_params.err = e.code();
    }
  
    send_response:
      try {
        CommHeader header;
        header.initialize_from_request_header(m_ev->header);
        CommBufPtr cbp = std::make_shared<CommBuf>(
          header, rsp_params.encoded_length());
        rsp_params.encode(cbp->get_data_ptr_address());
        m_conn->send_response(cbp);
      } catch (Exception &e) {
        HT_ERROR_OUT << e << HT_END;
      }
    
  }

};
  
// flag(if cid==1) 
//      in(cid+intervals)  out(cid + rid + rs-endpoints + ?next) 
// else 
//      in(cid+rid)        out(cid + rid + rs-endpoints)

}}}}

#endif // swc_app_manager_handlers_RsGet_h