
/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */ 

#ifndef swc_db_protocol_mngr_req_Echo_h
#define swc_db_protocol_mngr_req_Echo_h

#include "swcdb/db/Protocol/Commands.h"

namespace SWC { namespace Protocol { namespace Mngr { namespace Req {


class Echo : public DispatchHandler {
  public:
  typedef std::function<void(bool)> EchoCb_t;

  Echo(ConnHandlerPtr conn, EchoCb_t cb, size_t buf_sz=0)
       : conn(conn), cb(cb), was_called(false) { 

    if(!buf_sz) {
      cbp = CommBuf::make();

    } else {
      StaticBuffer sndbuf(buf_sz);
      uint8_t* ptr = sndbuf.base;
      const uint8_t* end = sndbuf.base + buf_sz-4;
    
      uint8_t i=0;
      while(ptr < end) {
        if(i == 127)
          i = 0;
        else
          i++;
        *ptr++ = i;
      }
      
      cbp = CommBuf::make(sndbuf);
    }
    
    cbp->header.set(DO_ECHO, 60000);
  }
  
  virtual ~Echo() { }
  
  bool run(uint32_t timeout=60000) override {
    return conn->send_request(cbp, shared_from_this()) == Error::OK;
  }

  void handle(ConnHandlerPtr conn, Event::Ptr& ev) override {
      
    //SWC_LOGF(LOG_DEBUG, "handle: %s", ev->to_str().c_str());

    if(ev->type == Event::Type::DISCONNECT){
      if(!was_called)
        cb(false);
      return;
    }

    if(ev->header.command == DO_ECHO){
      was_called = true;
      cb(ev->error == Error::OK);
    }
  }

  private:
  ConnHandlerPtr        conn;
  EchoCb_t              cb;
  std::atomic<bool>     was_called;
  CommBuf::Ptr          cbp;
};

}}}}

#endif // swc_db_protocol_mngr_req_Echo_h
