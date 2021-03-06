/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */

#ifndef swc_fs_Broker_Protocol_req_Readdir_h
#define swc_fs_Broker_Protocol_req_Readdir_h

#include "swcdb/fs/Broker/Protocol/req/Base.h"
#include "swcdb/fs/Broker/Protocol/params/Readdir.h"

namespace SWC { namespace FS { namespace Protocol { namespace Req {

class Readdir : public Base {

  public:

  DirentList listing;

  Readdir(uint32_t timeout, const std::string &name, Callback::ReaddirCb_t cb=0) 
         : name(name), cb(cb) {
    SWC_LOGF(LOG_DEBUG, "readdir path='%s'", name.c_str());

    cbp = CommBuf::make(Params::ReaddirReq(name));
    cbp->header.set(Cmd::FUNCTION_READDIR, timeout);
  }

  std::promise<void> promise(){
    std::promise<void>  r_promise;
    cb = [await=&r_promise](int err, DirentList listing){await->set_value();};
    return r_promise;
  }

  void handle(ConnHandlerPtr conn, Event::Ptr& ev) override { 

    const uint8_t *ptr;
    size_t remain;

    if(!Base::is_rsp(conn, ev, Cmd::FUNCTION_READDIR, &ptr, &remain))
      return;

    if(error == Error::OK) {
      Params::ReaddirRsp params;
      params.decode(&ptr, &remain);
      params.get_listing(listing);
    }

    SWC_LOGF(LOG_DEBUG, "readdir path='%s' error='%d' sz='%d'",
               name.c_str(), error, listing.size());
    
    cb(error, listing);
  }

  private:
  const std::string      name;
  Callback::ReaddirCb_t  cb;
};



}}}}

#endif  // swc_fs_Broker_Protocol_req_Readdir_h