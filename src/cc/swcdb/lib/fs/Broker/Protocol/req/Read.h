/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */

#ifndef swc_lib_fs_Broker_Protocol_req_Read_h
#define swc_lib_fs_Broker_Protocol_req_Read_h

#include "Base.h"
#include "../params/Read.h"

namespace SWC { namespace FS { namespace Protocol { namespace Req {

class Read : public Base {

  public:
  
  size_t amount;
  void* buffer;
  
  Read(uint32_t timeout, SmartFd::Ptr &smartfd, void* dst, size_t len, 
      Callback::ReadCb_t cb=0)
      : smartfd(smartfd), buffer(dst), cb(cb), amount(0) {
    HT_DEBUGF("read len=%d timeout=%d %s", 
              len, timeout, smartfd->to_string().c_str());

    cbp = CommBuf::make(Params::ReadReq(smartfd->fd(), len));
    cbp->header.set(Cmd::FUNCTION_READ, timeout);
  }

  std::promise<void> promise(){
    std::promise<void>  r_promise;
    cb = [await=&r_promise]
         (int err, SmartFd::Ptr smartfd, StaticBuffer::Ptr buf){
           await->set_value();
          };
    return r_promise;
  }

  void handle(ConnHandlerPtr conn, Event::Ptr &ev) { 

    const uint8_t *ptr;
    size_t remain;

    if(!Base::is_rsp(conn, ev, Cmd::FUNCTION_READ, &ptr, &remain))
      return;

    StaticBuffer::Ptr buf = nullptr;
    if(error == Error::OK || error == Error::FS_EOF) {
      Params::ReadRsp params;
      params.decode(&ptr, &remain);
      amount = ev->data_ext.size;
      smartfd->pos(params.get_offset()+amount);

      if(amount > 0) {
        if(buffer == nullptr) {
          buf = std::make_shared<StaticBuffer>(ev->data_ext); 
        } else {
          memcpy(buffer, ev->data_ext.base, amount);
        }
      }
    }

    HT_DEBUGF("read %s amount='%d' error='%d'", 
              smartfd->to_string().c_str(), amount, error);

    cb(error, smartfd, buf);
  }

  private:
  SmartFd::Ptr        smartfd;
  Callback::ReadCb_t  cb;
};



}}}}

#endif  // swc_lib_fs_Broker_Protocol_req_Read_h