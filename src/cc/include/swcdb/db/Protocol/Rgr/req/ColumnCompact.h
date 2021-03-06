
/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */ 

#ifndef swc_db_protocol_rgr_req_ColumnCompact_h
#define swc_db_protocol_rgr_req_ColumnCompact_h

#include "swcdb/db/Protocol/Rgr/params/ColumnCompact.h"


namespace SWC { namespace Protocol { namespace Rgr { namespace Req {
  

class ColumnCompact : public client::ConnQueue::ReqBase {
  public:

  ColumnCompact(int64_t cid);
  
  virtual ~ColumnCompact();
  
  void handle(ConnHandlerPtr conn, Event::Ptr& ev) override;

  void handle_no_conn() override;

};

}}}}


#ifdef SWC_IMPL_SOURCE
#include "swcdb/db/Protocol/Rgr/req/ColumnCompact.cc"
#endif 

#endif // swc_db_protocol_rgr_req_ColumnCompact_h
