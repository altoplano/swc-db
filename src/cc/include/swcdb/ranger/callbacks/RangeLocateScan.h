/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */

#ifndef swc_lib_ranger_callbacks_RangeLocateScan_h
#define swc_lib_ranger_callbacks_RangeLocateScan_h

#include "swcdb/core/comm/ResponseCallback.h"
#include "swcdb/db/Cells/ReqScan.h"

namespace SWC {
namespace server {
namespace Rgr {
namespace Callback {


class RangeLocateScan : public DB::Cells::ReqScan {
  public:

  typedef std::shared_ptr<RangeLocateScan> Ptr;

  RangeLocateScan(ConnHandlerPtr conn, Event::Ptr ev, 
                  const DB::Specs::Interval& spec, 
                  DB::Cells::Mutable& cells,
                  Range::Ptr range, uint8_t flags)
                  : DB::Cells::ReqScan(conn, ev, spec, cells), 
                    range(range), flags(flags),
                    any_is(range->type != Types::Range::DATA) {                
    //std::cout << "************************************************\n";    
  }

  virtual ~RangeLocateScan() { }

  Ptr shared() {
    return std::dynamic_pointer_cast<RangeLocateScan>(shared_from_this());
  }

  const DB::Cells::Mutable::Selector_t selector() override {
    if(flags & Protocol::Rgr::Params::RangeLocateReq::COMMIT)
      return  [req=shared()] 
              (const DB::Cells::Cell& cell, bool& stop) 
              { return req->selector_commit(cell, stop); };
    return  [req=shared()] 
            (const DB::Cells::Cell& cell, bool& stop) 
            { return req->selector_query(cell, stop); };
  }
  
  const bool selector_commit(const DB::Cells::Cell& cell, bool& stop) const {
    if(any_is && spec.range_begin.compare(cell.key, any_is) != Condition::EQ)
      return false;

    size_t remain = cell.vlen;
    const uint8_t * ptr = cell.value;
    DB::Cell::Key key_end;
    key_end.decode(&ptr, &remain);

    bool match;
    if(match = key_end.size() == any_is ||
               key_end.compare(spec.range_begin) != Condition::GT)
      stop = true;
    return match;
  }

  const bool selector_query(const DB::Cells::Cell& cell, bool& stop) const {
    if(any_is && spec.range_begin.compare(cell.key, any_is) != Condition::EQ)
      return false;

    if(flags & Protocol::Rgr::Params::RangeLocateReq::NEXT_RANGE && 
       spec.range_offset.compare(cell.key) != Condition::GT)
      return false;

    if(cell.key.size() > any_is && spec.range_end.size() > any_is && 
       !spec.is_matching_end(cell.key)) {
      stop = true;
      //std::cout << "-- KEY-BEGIN NO MATCH STOP --\n";
      return false;
    }

    size_t remain = cell.vlen;
    const uint8_t * ptr = cell.value;
    DB::Cell::Key key_end;
    key_end.decode(&ptr, &remain);

    if(key_end.size() > any_is && spec.range_begin.size() > any_is && 
       !spec.is_matching_begin(key_end)) {
      //std::cout << "-- KEY-END NO MATCH --\n";
      return false;
    }
    //return true;

    int64_t rid = Serialization::decode_vi64(&ptr, &remain); // rid

    DB::Cell::Key aligned_min;
    aligned_min.decode(&ptr, &remain);
    DB::Cell::Key aligned_max;
    aligned_max.decode(&ptr, &remain);
    /*
    std::cout << "---------------------\n";
    std::cout << "range_begin: " << spec.range_begin.to_string() << "\n";
    std::cout << "  key_begin: " << cell.key.to_string() << "\n";
    std::cout << "aligned_min: " << aligned_min.to_string() << "\n";
    std::cout << "  range_end: " << spec.range_end.to_string() << "\n";
    std::cout << "    key_end: " << key_end.to_string() << "\n";
    std::cout << "aligned_max: " << aligned_max.to_string() << "\n";
    std::cout << "        rid: " << rid << "\n";
    */
    if(spec.range_begin.size() == any_is ||
       spec.range_begin.compare(
         aligned_max, Condition::LT, spec.range_begin.size(), true)) {
      if(spec.range_end.size() == any_is ||
         spec.range_end.compare(
           aligned_min, Condition::GT, spec.range_end.size(), true)) {
        //std::cout << "-- ALIGNED MATCH  --\n";
        return true;
      }
    }
    //std::cout << "-- ALIGNED NO MATCH -------\n";
    return false;
  }

  void response(int &err) override {
    if(!DB::Cells::ReqScan::ready(err))
      return;
      
    if(!err) {
      if(RangerEnv::is_shuttingdown())
        err = Error::SERVER_SHUTTING_DOWN;
      if(range->deleted())
        err = Error::COLUMN_MARKED_REMOVED;
    }
    if(err == Error::COLUMN_MARKED_REMOVED)
      cells.free();
    
    //SWC_LOG_OUT(LOG_INFO) << spec.to_string() << SWC_LOG_OUT_END;

    Protocol::Rgr::Params::RangeLocateRsp params(err);
    if(!err) {
      if(cells.size()) {

        DB::Cells::Cell cell;
        cells.get(0, cell);
  
        params.range_begin.copy(cell.key);
        
        std::string id_name(params.range_begin.get(0));
        params.cid = (int64_t)strtoll(id_name.c_str(), NULL, 0);

        const uint8_t* ptr = cell.value;
        size_t remain = cell.vlen;
        params.range_end.decode(&ptr, &remain, true);
        params.rid = Serialization::decode_vi64(&ptr, &remain);

        if(range->type == Types::Range::MASTER) {
          params.range_begin.remove(0);
          params.range_end.remove(0);
        }
        params.range_begin.remove(0);
        params.range_end.remove(0);
      /*
      } else if(spec.range_begin.size() > 1) {
        spec.range_begin.remove(spec.range_begin.size()-1, true);
        range->scan(get_req_scan());
        return;
      */
      } else {
        params.err = Error::RANGE_NOT_FOUND;
      }
    }

    SWC_LOG_OUT(LOG_DEBUG) 
      << params.to_string() << " flags=" << (int)flags 
      << SWC_LOG_OUT_END;
    
    try {
      auto cbp = CommBuf::make(params);
      cbp->header.initialize_from_request_header(m_ev->header);
      m_conn->send_response(cbp);
    }
    catch (Exception &e) {
      SWC_LOG_OUT(LOG_ERROR) << e << SWC_LOG_OUT_END;
    }
    
  }

  Range::Ptr  range;
  uint8_t     flags;
  uint32_t    any_is;

};


}
}}}
#endif // swc_lib_ranger_callbacks_RangeLocateScan_h
