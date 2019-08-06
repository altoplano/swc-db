
/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */ 

#ifndef swc_lib_db_protocol_req_MngrsState_h
#define swc_lib_db_protocol_req_MngrsState_h

#include "swcdb/lib/db/Protocol/params/MngrsState.h"

namespace SWC {
namespace Protocol {
namespace Req {

class MngrsState : public DispatchHandler {
  public:

  static CommBufPtr get_buf(server::Mngr::HostStatuses states, 
                            uint64_t token, EndPoint mngr_host, 
                            uint32_t timeout) {
    Protocol::Params::MngrsState params(states, token, mngr_host);
    CommHeader header(Protocol::Command::MNGR_REQ_MNGRS_STATE, timeout);
    CommBufPtr cbp = std::make_shared<CommBuf>(header, params.encoded_length());
    params.encode(cbp->get_data_ptr_address());
    return cbp;
  }

  MngrsState(client::ClientConPtr conn, CommBufPtr cbp,
             ResponseCallbackPtr cb)
            : conn(conn), cbp(cbp), cb(cb) {
  }
  
  virtual ~MngrsState() { }
  
  bool run(uint32_t timeout=60000) override {
    return conn->send_request(cbp, shared_from_this()) == Error::OK;
  }

  void disconnected();

  void handle(ConnHandlerPtr conn, EventPtr &ev) {
    
    // HT_DEBUGF("handle: %s", ev->to_str().c_str());
    
    if(ev->type == Event::Type::DISCONNECT){
      disconnected();
      return;
    }

    if(ev->header.command == Protocol::Command::MNGR_REQ_MNGRS_STATE 
       && Protocol::response_code(ev) == Error::OK){
      if(cb != nullptr){
        std::cout << "response_ok, cb=" << (size_t)cb.get() << " rsp, err=" << ev->to_str() << "\n";
        cb->response_ok();
      }
      return;
    }

    conn->do_close();

  }

  private:
  client::ClientConPtr  conn;
  CommBufPtr            cbp;
  ResponseCallbackPtr   cb;
};

typedef std::shared_ptr<MngrsState> MngrsStatePtr;

}}}

#endif // swc_lib_db_protocol_req_MngrsState_h
