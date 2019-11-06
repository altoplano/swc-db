/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swc_core_comm_ConnHandler_h
#define swc_core_comm_ConnHandler_h

#include <asio.hpp>
#include "swcdb/lib/core/comm/Resolver.h"

namespace SWC { 

typedef std::shared_ptr<asio::io_context> IOCtxPtr;
typedef std::shared_ptr<asio::ip::tcp::socket> SocketPtr;
typedef std::shared_ptr<asio::high_resolution_timer> TimerPtr;

// forward declarations
class ConnHandler;
typedef std::shared_ptr<ConnHandler> ConnHandlerPtr;

}
 
#include <memory>
#include "DispatchHandler.h"

#include "Event.h"
#include "CommBuf.h"
#include "Protocol.h"
#include <queue>

namespace SWC { 


inline size_t endpoints_hash(const EndPoints& endpoints){
  std::string s;
  for(auto& endpoint : endpoints){
    s.append(endpoint.address().to_string());
    s.append(":");
    s.append(std::to_string(endpoint.port()));
  }
  std::hash<std::string> hasher;
  return hasher(s);
}

inline size_t endpoint_hash(const EndPoint& endpoint){
  std::hash<std::string> hasher;
  return hasher(
    (std::string)endpoint.address().to_string()
    +":"
    +std::to_string(endpoint.port()));
}


class ConnHandler : public std::enable_shared_from_this<ConnHandler> {

  struct PendingRsp {
    uint32_t           id;
    DispatchHandlerPtr hdlr;
    TimerPtr           tm;
    bool               sequential;
    Event::Ptr         ev;
  };

  struct Outgoing {
    CommBuf::Ptr        cbuf;
    DispatchHandlerPtr  hdlr; 
    bool                sequential;
  };

  public:
  ConnHandler(AppContextPtr app_ctx, SocketPtr socket, IOCtxPtr io_ctx) 
            : app_ctx(app_ctx), m_sock(socket), io_ctx(io_ctx),
              m_next_req_id(0) {
  }

  ConnHandlerPtr ptr(){
    return shared_from_this();
  }

  virtual ~ConnHandler(){ 
    do_close();
  }
  
  const AppContextPtr   app_ctx;
  const IOCtxPtr        io_ctx;
  EndPoint        endpoint_remote;
  EndPoint        endpoint_local;

  const std::string endpoint_local_str(){
    std::string s(endpoint_local.address().to_string());
    s.append(":");
    s.append(std::to_string(endpoint_local.port()));
    return s;
  }
  
  const std::string endpoint_remote_str(){
    std::string s(endpoint_remote.address().to_string());
    s.append(":");
    s.append(std::to_string(endpoint_remote.port()));
    return s;
  }
  
  const size_t endpoint_remote_hash(){
    return endpoint_hash(endpoint_remote);
  }
  
  const size_t endpoint_local_hash(){
    return endpoint_hash(endpoint_local);
  }
  
  virtual void new_connection() {
    std::lock_guard<std::mutex> lock(m_mutex);

    endpoint_remote = m_sock->remote_endpoint();
    endpoint_local = m_sock->local_endpoint();
    HT_DEBUGF("new_connection local=%s, remote=%s, executor=%d",
              endpoint_local_str().c_str(), endpoint_remote_str().c_str(),
              (size_t)&m_sock->get_executor().context());
  }

  inline bool is_open() {
    return m_err == Error::OK && m_sock->is_open();
  }

  void close(){
    if(is_open()){
      std::lock_guard<std::mutex> lock(m_mutex);
      try{m_sock->close();}catch(...){}
    }
    m_err = Error::COMM_NOT_CONNECTED;
    disconnected();
  }

  size_t pending_read(){
    std::lock_guard<std::mutex> lock(m_mutex_reading);
    return m_pending.size();
  }

  size_t pending_write(){
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_outgoing.size();
  }

  bool due() {
    return pending_read() > 0 || pending_write() > 0;
  }


  virtual void run(Event::Ptr ev, DispatchHandlerPtr hdlr=nullptr) {
    if(hdlr != nullptr)
      hdlr->handle(ptr(), ev);
    if(ev->type == Event::Type::DISCONNECT)
      return;
    HT_WARNF("run is Virtual!, %s", ev->to_str().c_str());
  }

  void do_close(DispatchHandlerPtr hdlr=nullptr){
    if(m_err == Error::OK) {
      close();
      run(Event::make(Event::Type::DISCONNECT, m_err), hdlr);
    }
  }

  int send_error(int error, const String &msg, Event::Ptr ev=nullptr) {
    if(m_err != Error::OK)
      return m_err;

    size_t max_msg_size = std::numeric_limits<int16_t>::max();
    auto cbp = Protocol::create_error_message(
      error, msg.length() < max_msg_size ? 
        msg.c_str() 
      :  msg.substr(0, max_msg_size).c_str()
    );
    if(ev != nullptr)
      cbp->header.initialize_from_request_header(ev->header);
    return send_response(cbp);
  }

  int response_ok(Event::Ptr ev=nullptr) {
    if(m_err != Error::OK)
      return m_err;
      
    auto cbp = CommBuf::make(4);
    if(ev != nullptr)
      cbp->header.initialize_from_request_header(ev->header);
    cbp->append_i32(Error::OK);
    return send_response(cbp);
  }

  inline int send_response(CommBuf::Ptr &cbuf, DispatchHandlerPtr hdlr=nullptr){
    if(m_err != Error::OK)
      return m_err;

    cbuf->header.flags &= CommHeader::FLAGS_MASK_REQUEST;

    write_or_queue(new Outgoing({
     .cbuf=cbuf,
     .hdlr=hdlr, 
     .sequential=false
    }));
    return m_err;
  }

  inline int send_response(CommBuf::Ptr &cbuf, uint32_t timeout_ms){
    if(m_err != Error::OK)
      return m_err;

    int e = Error::OK;
    cbuf->header.flags &= CommHeader::FLAGS_MASK_REQUEST;

    std::vector<asio::const_buffer> buffers;
    cbuf->get(buffers);

    std::future<size_t> f = asio::async_write(
      *m_sock.get(), buffers, asio::use_future);

    std::future_status status = f.wait_for(
      std::chrono::milliseconds(timeout_ms));

    if (status == std::future_status::ready){
      try {
        f.get();
      } catch (const std::exception& ex) {
        e = (m_err != Error::OK? m_err.load() : Error::COMM_BROKEN_CONNECTION);
      }
    } else {
      e = Error::REQUEST_TIMEOUT;
    }
    read_pending();
    return e;
  }

  /* 
  inline int send_response_sync(CommBuf::Ptr &cbuf){
    if(m_err != Error::OK)
      return m_err;

    asio::error_code error;
    uint32_t len_sent = m_sock->write_some(cbuf->get_buffers(), error);
    return error.value() ?
           (m_err != Error::OK? m_err.load() : Error::COMM_BROKEN_CONNECTION) 
           : Error::OK;
  }
  */

  inline int send_request(uint32_t timeout_ms, CommBuf::Ptr &cbuf, 
                    DispatchHandlerPtr hdlr, bool sequential=false) {
    cbuf->header.timeout_ms = timeout_ms;
    return send_request(cbuf, hdlr, sequential);
  }

  inline int send_request(CommBuf::Ptr &cbuf, DispatchHandlerPtr hdlr, 
                          bool sequential=false) {
    if(m_err != Error::OK)
      return m_err;

    cbuf->header.flags |= CommHeader::FLAGS_BIT_REQUEST;

    if(cbuf->header.id == 0)
      cbuf->header.id = next_req_id();

    write_or_queue(new Outgoing({
     .cbuf=cbuf, 
     .hdlr=hdlr, 
     .sequential=sequential
    }));
    return m_err;
  }

  inline void accept_requests() {
    m_accepting = true;
    read_pending();
  }

  inline void accept_requests(DispatchHandlerPtr hdlr, 
                              uint32_t timeout_ms=0) {
    add_pending(new PendingRsp({
      .id=0,  // initial req.acceptor
      .hdlr=hdlr,
      .tm=get_timer(timeout_ms)
    }));
    read_pending();
  }

  private:

  inline uint32_t next_req_id(){
    return ++m_next_req_id == 0 ? ++m_next_req_id : m_next_req_id.load();
  }

  TimerPtr get_timer(uint32_t t_ms, CommHeader header=0){
    if(t_ms == 0) 
      return nullptr;

    TimerPtr tm = std::make_shared<asio::high_resolution_timer>(
      m_sock->get_executor(), std::chrono::milliseconds(t_ms)); 

    tm->async_wait(
      [header, ptr=ptr()]
      (const asio::error_code ec) {
        if (ec != asio::error::operation_aborted){
          auto ev = Event::make(
            Event::Type::ERROR, Error::Code::REQUEST_TIMEOUT);
          ev->header = header;
          ptr->run_pending(header.id, ev, true);
        } 
      }
    );
    return tm;
  }
    
  inline void write_or_queue(Outgoing* data){ 
    {
      std::lock_guard<std::mutex> lock(m_mutex);  
      if(m_writing) {
        m_outgoing.push(data);
        return;
      }
      m_writing = true;
    }
    write(data);
  }
  
  inline void next_outgoing() {
    Outgoing* data;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_writing = m_outgoing.size() > 0;
      if(!m_writing) 
        return;
      data = m_outgoing.front();
      m_outgoing.pop();
    }
    write(data); 
  }
  
  inline void write(Outgoing* data){

    if(data->cbuf->header.flags & CommHeader::FLAGS_BIT_REQUEST)
      add_pending(new PendingRsp({
        .id=data->cbuf->header.id, 
        .hdlr=data->hdlr, 
        .tm=get_timer(data->cbuf->header.timeout_ms, data->cbuf->header), 
        .sequential=data->sequential
      }));
    
    std::vector<asio::const_buffer> buffers;
    data->cbuf->get(buffers);

    asio::async_write(
      *m_sock.get(), 
      buffers,
      [data, ptr=ptr()]
      (const asio::error_code ec, uint32_t len) {
        delete data;
        if(ec) {
          ptr->do_close();
        } else {
          ptr->next_outgoing();
          ptr->read_pending();
        }
      }
    );
  }

  inline void add_pending(PendingRsp* q){        
    std::lock_guard<std::mutex> lock(m_mutex_reading);
    m_pending.push_back(q);
  }

  inline void read_pending(){
    {
      std::lock_guard<std::mutex> lock(m_mutex_reading);
      if(m_err != Error::OK || m_reading)
        return;
      m_reading = true;
    }

    Event::Ptr ev = Event::make(Event::Type::MESSAGE, Error::OK);
    uint8_t* data = new uint8_t[CommHeader::PREFIX_LENGTH+1];

    asio::async_read(
      *m_sock.get(), 
      asio::mutable_buffer(data, CommHeader::PREFIX_LENGTH+1),
      [ev, data, ptr=ptr()](const asio::error_code e, size_t filled) {
        return ptr->read_condition_hdlr(ev, data, e, filled);
      },
      [ev, ptr=ptr()](const asio::error_code e, size_t filled){
        ptr->received(ev, e, filled);
      }
    );
  }
  
  size_t read_condition_hdlr(Event::Ptr ev, uint8_t* data,
                             const asio::error_code e, size_t filled) {
    asio::error_code ec = e;
    size_t remain = read_condition(ev, data, ec, filled);
    if(ec) {
      remain = 0;
      do_close();
    }
    if(remain == 0) 
      delete [] data;
    return remain;
  }
  
  inline size_t read_condition(Event::Ptr ev, uint8_t* data,
                               asio::error_code &ec, size_t filled) {
                  
    if(filled < CommHeader::PREFIX_LENGTH)
      return CommHeader::PREFIX_LENGTH-filled;
  
    size_t remain;
    const uint8_t* ptr;
    uint8_t* bufp;
    size_t  read;
    
    try{
      ptr = data;
      remain = CommHeader::PREFIX_LENGTH;
      ev->header.decode_prefix(&ptr, &remain);
      ptr = data;

      uint8_t buf_header[ev->header.header_len];
      bufp = buf_header;
      for(uint8_t n=0;n<CommHeader::PREFIX_LENGTH;n++)
        *bufp++ = *ptr++;

      read = 0;
      remain = ev->header.header_len - CommHeader::PREFIX_LENGTH;
      do {
        read = m_sock->read_some(asio::mutable_buffer(bufp+=read, remain), ec);
        filled += read;
      } while(!ec && (remain -= read));  
      
      ptr = buf_header;
      remain = ev->header.header_len;
      ev->header.decode(&ptr, &remain);
    } catch(...){
      ec = asio::error::eof;
    }
    if(ec) {
      ec = asio::error::eof;
      ev->type = Event::Type::ERROR;
      ev->error = Error::REQUEST_TRUNCATED_HEADER;
      ev->header = CommHeader();
      HT_WARNF("read, REQUEST HEADER_TRUNCATED: remain=%d filled=%d", remain, filled);
      return (size_t)0;
    }
    if(!ev->header.buffers)
      return (size_t)0;

    uint32_t checksum;
    for(uint8_t n=0; n < ev->header.buffers; n++) {
      StaticBuffer& buffer = n == 0 ? ev->data :  ev->data_ext;
      if(n == 0) {
        remain = ev->header.data_size;
        checksum = ev->header.data_chksum;
      } else {
        remain = ev->header.data_ext_size;
        checksum = ev->header.data_ext_chksum;
      }
      buffer.reallocate(remain);

      bufp = buffer.base;
      read = 0;
      do {
        read = m_sock->read_some(asio::mutable_buffer(bufp+=read, remain), ec);
      } while(!ec && (remain -= read));
      if(ec) 
        break;
      if(!checksum_i32_chk(checksum, buffer.base, buffer.size)) {
        ec = asio::error::eof;
        break;
      }
    }

    if(ec) {
      ec = asio::error::eof;
      ev->error = Error::REQUEST_TRUNCATED_PAYLOAD;
      ev->data.free();
      ev->data_ext.free();
      HT_WARNF("read, REQUEST PAYLOAD_TRUNCATED: error=(%s) %s", 
                ec.message().c_str(), ev->to_str().c_str());
    }
    return (size_t)0;
  }

  void received(Event::Ptr ev, const asio::error_code ec, size_t filled){
    ev->arrival_time = ClockT::now();
    if(ec) {
      do_close();
      return;
    }

    bool more;
    {
      std::lock_guard<std::mutex> lock(m_mutex_reading);
      m_reading = false;
      more = m_accepting || m_pending.size() > 0;
    }
    if(more)
      read_pending();

    run_pending(ev->header.id, ev);
  }

  void disconnected(){
    {
      std::lock_guard<std::mutex> lock(m_mutex_reading);
      m_cancelled.clear();
    }
    
    for(;;) {
      PendingRsp* q;
      {
        std::lock_guard<std::mutex> lock(m_mutex_reading);
        if(m_pending.empty())
          return;
        q = *m_pending.begin();
        m_pending.erase(m_pending.begin());
      }
      
      if(q->tm != nullptr) q->tm->cancel();
      run(Event::make(Event::Type::DISCONNECT, m_err), q->hdlr);
      delete q;
    }
  }

  inline void run_pending(uint32_t id, Event::Ptr ev, bool cancelling=false){
    bool found_current = false;
    bool found_not = false;
    bool found_next;
    bool skip;
    std::vector<PendingRsp*>::iterator it_found;
    std::vector<PendingRsp*>::iterator it_end;

    PendingRsp* q_now;
    PendingRsp* q_next;
    do {
      found_next = false;
      skip = false;
      {
        std::lock_guard<std::mutex> lock(m_mutex_reading);
        /* 
        std::cout << "run_pending: m_cancelled=" << m_cancelled.size() 
                  << " m_pending=" << m_pending.size() << "\n";
        for(auto q :m_pending )
          std::cout << " id=" << q->id << "\n";
        std::cout << "looking for id=" << id << " fd="<<m_sock->native_handle()<<" ptr="<<this
        << " " << endpoint_local_str() <<"\n";
        */
        if(cancelling)
          m_cancelled.push_back(id);

        found_not = m_pending.empty();
        if(!found_not){
          it_found = std::find_if(m_pending.begin(), m_pending.end(), 
                                 [id](const PendingRsp* q)
                                     {return q->id == id;});
          it_end = m_pending.end();
          found_not = it_found == it_end;
        }
        if(!found_not){
          found_current = !(*it_found)->sequential || it_found == m_pending.begin();

          if(found_current){
            q_now = *it_found;

            if(q_now->tm != nullptr) 
              q_now->tm->cancel();

            if(q_now->sequential){
              if(++it_found != it_end){
                found_next = (*it_found)->ev != nullptr;
                if(found_next)
                  q_next = *it_found;
              }
              m_pending.erase(--it_found);
            } else
              m_pending.erase(it_found);

          } else {
            (*it_found)->ev=ev;
          }
          
        } else if(!cancelling) {
            auto it = std::find_if(m_cancelled.begin(), m_cancelled.end(), 
                                  [id](uint32_t i){return i == id;});
            skip = (it != m_cancelled.end());
            if(skip)
              m_cancelled.erase(it);
        }
      }
    
      if(found_not){
        if(id == 0)
          run(ev);
        else if(!skip)
          run_pending(0, ev);
        
      } else if(found_current){

        run(ev, q_now->hdlr);
        delete q_now;
        
        if(found_next)
          run_pending(q_next->id, q_next->ev);
      }
    } while(found_next);

  }

  const SocketPtr           m_sock;
  std::atomic<uint32_t>     m_next_req_id;

  std::mutex                m_mutex;
  std::queue<Outgoing*>     m_outgoing;
  bool                      m_writing = 0;

  std::mutex                m_mutex_reading;
  std::vector<PendingRsp*>  m_pending;
  std::vector<uint32_t>     m_cancelled;
  bool                      m_accepting = 0;
  bool                      m_reading = 0;

  std::atomic<Error::Code>  m_err = Error::OK;

};


}

#endif // swc_core_comm_ConnHandler_h