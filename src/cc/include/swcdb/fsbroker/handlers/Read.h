/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swc_app_fsbroker_handlers_Read_h
#define swc_app_fsbroker_handlers_Read_h

#include "swcdb/fs/Broker/Protocol/params/Read.h"


namespace SWC { namespace server { namespace FsBroker {

namespace Handler {


class Read : public AppHandler {
  public:

  Read(ConnHandlerPtr conn, Event::Ptr ev)
       : AppHandler(conn, ev){ }

  void run() override {

    int err = Error::OK;
    size_t offset = 0;
    StaticBuffer rbuf;

    try {

      const uint8_t *ptr = m_ev->data.base;
      size_t remain = m_ev->data.size;

      FS::Protocol::Params::ReadReq params;
      params.decode(&ptr, &remain);

      auto smartfd = Env::Fds::get()->select(params.fd);
      
      if(smartfd == nullptr)
        err = EBADR;
      else {
        offset = smartfd->pos();
        rbuf.reallocate(params.amount);
        rbuf.size = Env::FsInterface::fs()->read(
          err, smartfd, rbuf.base, params.amount);
      }
    }
    catch (Exception &e) {
      SWC_LOG_OUT(LOG_ERROR) << e << SWC_LOG_OUT_END;
      err = e.code();
    }
  
    try {
      auto cbp = CommBuf::make(FS::Protocol::Params::ReadRsp(offset), rbuf, 4);
      cbp->header.initialize_from_request_header(m_ev->header);
      cbp->append_i32(err);
      m_conn->send_response(cbp);
    }
    catch (Exception &e) {
      SWC_LOG_OUT(LOG_ERROR) << e << SWC_LOG_OUT_END;
    }

  }

};
  

}}}}

#endif // swc_app_fsbroker_handlers_Read_h