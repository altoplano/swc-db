
/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */

#ifndef swc_lib_rangeserver_callbacks_HandleRsShutdown_h
#define swc_lib_rangeserver_callbacks_HandleRsShutdown_h

#include "swcdb/lib/db/Protocol/params/HostEndPoints.h"
#include "swcdb/lib/db/Protocol/params/MngRsId.h"
#include <functional>

namespace SWC {
namespace server {
namespace RS {

class HandleRsShutdown: public Protocol::Rsp::ActiveMngrRspCb {
  public:

  HandleRsShutdown(Protocol::Req::ActiveMngrPtr mngr_active,
                   std::function<void()> cb)
                  : Protocol::Rsp::ActiveMngrRspCb(mngr_active), 
                    cb(cb) {
  }

  virtual ~HandleRsShutdown(){}

  client::ClientConPtr m_conn;
  void run(EndPoints endpoints) override {

    m_conn = EnvClients::get()->mngr_service->get_connection(endpoints);

    Files::RsDataPtr rs_data = EnvRsData::get();
    Protocol::Params::MngRsId params(
      rs_data->rs_id, Protocol::Params::MngRsId::Flag::RS_SHUTTINGDOWN, 
      rs_data->endpoints);

    CommHeader header(Protocol::Command::REQ_MNGR_MNG_RS_ID, 60000);
    CommBufPtr cbp = std::make_shared<CommBuf>(header, params.encoded_length());
    params.encode(cbp->get_data_ptr_address());

    m_conn->send_request(cbp, shared_from_this());    
  }
  
  void handle(ConnHandlerPtr conn, EventPtr &ev) override {
    
    // HT_DEBUGF("handle: %s", ev->to_str().c_str());

    conn->do_close();
    if(ev->error == Error::OK 
       && ev->header.command == Protocol::Command::REQ_MNGR_MNG_RS_ID){

      if(Protocol::response_code(ev) == Error::OK){
        std::cout << "HandleRsShutdown: RSP-OK \n";
        cb();
        return;
      }
    }

    // return if timeout?

    mngr_active->run_within(conn->m_io_ctx, 1000);
  }
  
  std::function<void()> cb;
};

}}}

#endif // swc_lib_rangeserver_callbacks_HandleRsAssign_h