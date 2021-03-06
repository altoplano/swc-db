
/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */


#include "swcdb/db/Protocol/Mngr/req/ColumnGet.h"
#include "swcdb/db/Protocol/Mngr/req/MngrActive.h"
#include "swcdb/db/client/Clients.h"
#include "swcdb/db/Protocol/Commands.h"


namespace SWC { namespace Protocol { namespace Mngr { namespace Req {

 
 void ColumnGet::schema(const std::string& name, const ColumnGet::Cb_t cb, 
                        const uint32_t timeout) {
  request(Flag::SCHEMA_BY_NAME, name, cb, timeout);
}

 void ColumnGet::schema(int64_t cid, const ColumnGet::Cb_t cb, 
                        const uint32_t timeout) {
  request(Flag::SCHEMA_BY_ID, cid, cb, timeout);
}

 void ColumnGet::cid(const std::string& name, const ColumnGet::Cb_t cb, 
                     const uint32_t timeout) {
  request(Flag::ID_BY_NAME, name, cb, timeout);
}

 void ColumnGet::request(ColumnGet::Flag flag, const std::string& name, 
                         const ColumnGet::Cb_t cb, 
                         const uint32_t timeout){
  std::make_shared<ColumnGet>(Params::ColumnGetReq(flag, name), cb, timeout)
    ->run();
}

 void ColumnGet::request(ColumnGet::Flag flag, int64_t cid, const ColumnGet::Cb_t cb, 
                    const uint32_t timeout){
  std::make_shared<ColumnGet>(Params::ColumnGetReq(flag, cid), cb, timeout)
    ->run();
}


ColumnGet::ColumnGet(const Params::ColumnGetReq& params, 
                     const ColumnGet::Cb_t cb, 
                     const uint32_t timeout) 
                    : client::ConnQueue::ReqBase(false), cb(cb) {
  cbp = CommBuf::make(params);
  cbp->header.set(COLUMN_GET, timeout);
}

ColumnGet::~ColumnGet() { }

void ColumnGet::handle_no_conn() {
  clear_endpoints();
  run();
}

bool ColumnGet::run(uint32_t timeout) {
  if(endpoints.empty()){
    // columns-get (can be any mngr)
    Env::Clients::get()->mngrs_groups->select(1, endpoints); 
    if(endpoints.empty()){
      std::make_shared<MngrActive>(1, shared_from_this())->run();
      return false;
    }
  } 
  Env::Clients::get()->mngr->get(endpoints)->put(req());
  return true;
}

void ColumnGet::handle(ConnHandlerPtr conn, Event::Ptr& ev) {

  if(ev->type == Event::Type::DISCONNECT) {
    handle_no_conn();
    return;
  }

  Params::ColumnGetRsp rsp_params;
  int err = ev->error != Error::OK? ev->error: ev->response_code();

  if(err == Error::OK) {
    try{
      const uint8_t *ptr = ev->data.base+4;
      size_t remain = ev->data.size-4;
      rsp_params.decode(&ptr, &remain);
    } catch (Exception &e) {
      SWC_LOG_OUT(LOG_ERROR) << e << SWC_LOG_OUT_END;
      err = e.code();
    }
  }

  cb(req(), err, rsp_params);
}

void ColumnGet::clear_endpoints() {
  Env::Clients::get()->mngrs_groups->remove(endpoints);
  endpoints.clear();
}



}}}}