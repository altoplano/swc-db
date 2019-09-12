/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swc_app_manager_handlers_RsMngId_h
#define swc_app_manager_handlers_RsMngId_h

#include "swcdb/lib/db/Protocol/params/MngrRsMngId.h"


namespace SWC { namespace server { namespace Mngr {

namespace Handler {


class RsMngId : public AppHandler {
  public:

    RsMngId(ConnHandlerPtr conn, EventPtr ev)
            : AppHandler(conn, ev){}

  void run() override {

    try {

      const uint8_t *ptr = m_ev->payload;
      size_t remain = m_ev->payload_len;

      Protocol::Params::MngrRsMngId req_params;
      req_params.decode(&ptr, &remain);

      // ResponseCallbackPtr cb = 
      //  std::make_shared<ResponseCallback>(m_conn, m_ev);
      
      // std::cout << "RsMngId-run rs " << req_params.to_string() << "\n";
      
      if(!Env::MngrRole::get()->is_active(1)){
        HT_DEBUGF("MNGR NOT ACTIVE, flag=%d rs_id=%d %s",
                  req_params.flag, req_params.rs_id, 
                  req_params.to_string().c_str());
      
        Protocol::Params::MngrRsMngId rsp_params(
          0, Protocol::Params::MngrRsMngId::Flag::MNGR_NOT_ACTIVE);
        
        CommHeader header;
        header.initialize_from_request_header(m_ev->header);
        CommBufPtr cbp = std::make_shared<CommBuf>(
          header, rsp_params.encoded_length());
        rsp_params.encode(cbp->get_data_ptr_address());

        m_conn->send_response(cbp);
        return;
      }

      RangeServersPtr rangeservers = Env::RangeServers::get();
      switch(req_params.flag){

        case Protocol::Params::MngrRsMngId::Flag::RS_REQ: {
          uint64_t rs_id = rangeservers->rs_set_id(req_params.endpoints);

          HT_DEBUGF("RS_REQ, rs_id=%d %s",
                    req_params.rs_id, req_params.to_string().c_str());

          Protocol::Params::MngrRsMngId rsp_params(
            rs_id, Protocol::Params::MngrRsMngId::Flag::MNGR_ASSIGNED);
          CommHeader header;
          header.initialize_from_request_header(m_ev->header);
          CommBufPtr cbp = std::make_shared<CommBuf>(
            header, rsp_params.encoded_length());
          rsp_params.encode(cbp->get_data_ptr_address());

          m_conn->send_response(cbp);
          break;
        }

        case Protocol::Params::MngrRsMngId::Flag::RS_ACK: {
          if(rangeservers->rs_ack_id(req_params.rs_id, req_params.endpoints)){
            HT_DEBUGF("RS_ACK, rs_id=%d %s",
                      req_params.rs_id, req_params.to_string().c_str());
            m_conn->response_ok(m_ev);

          } else {
            HT_DEBUGF("RS_ACK(MNGR_REREQ) rs_id=%d %s",
                      req_params.rs_id, req_params.to_string().c_str());

            Protocol::Params::MngrRsMngId rsp_params(
              0, Protocol::Params::MngrRsMngId::Flag::MNGR_REREQ);
            CommHeader header;
            header.initialize_from_request_header(m_ev->header);
            CommBufPtr cbp = std::make_shared<CommBuf>(
              header, rsp_params.encoded_length());
            rsp_params.encode(cbp->get_data_ptr_address());

            m_conn->send_response(cbp);
          }
          break;
        }

        case Protocol::Params::MngrRsMngId::Flag::RS_DISAGREE: {
          uint64_t rs_id = rangeservers->rs_had_id(req_params.rs_id, 
                                                   req_params.endpoints);
          HT_DEBUGF("RS_DISAGREE, rs_had_id=%d > rs_id=%d %s", 
                    req_params.rs_id, rs_id, req_params.to_string().c_str());

          if (rs_id != 0){
            Protocol::Params::MngrRsMngId rsp_params(
              rs_id, Protocol::Params::MngrRsMngId::Flag::MNGR_REASSIGN);
      
            CommHeader header;
            header.initialize_from_request_header(m_ev->header);
            CommBufPtr cbp = std::make_shared<CommBuf>(
              header, rsp_params.encoded_length());
            rsp_params.encode(cbp->get_data_ptr_address());

            m_conn->send_response(cbp);
          } else {
            m_conn->response_ok(m_ev);
          }

          break;
        }

        case Protocol::Params::MngrRsMngId::Flag::RS_SHUTTINGDOWN: {
          rangeservers->rs_shutdown(req_params.rs_id, req_params.endpoints);

          HT_DEBUGF("RS_SHUTTINGDOWN, rs_id=%d %s",
                    req_params.rs_id, req_params.to_string().c_str());
          
          Protocol::Params::MngrRsMngId rsp_params(
            req_params.rs_id, Protocol::Params::MngrRsMngId::Flag::RS_SHUTTINGDOWN);
      
          CommHeader header;
          header.initialize_from_request_header(m_ev->header);
          CommBufPtr cbp = std::make_shared<CommBuf>(
            header, rsp_params.encoded_length());
          rsp_params.encode(cbp->get_data_ptr_address());

          m_conn->send_response(cbp);
          break;
        }
        
        default:
          break;
      }

    } catch (Exception &e) {
      HT_ERROR_OUT << e << HT_END;
    }
    
  }

};
  

}}}}

#endif // swc_app_manager_handlers_RsMngId_h