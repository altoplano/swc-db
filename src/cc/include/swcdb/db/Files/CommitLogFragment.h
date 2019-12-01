/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swcdb_db_Files_CommitLogFragment_h
#define swcdb_db_Files_CommitLogFragment_h

#include "swcdb/db/Files/CellStoreBlock.h"

namespace SWC { namespace Files { namespace CommitLog {

  
class Fragment {

  /* file-format: 
        header:      i8(version), i32(header_ext-len), i32(checksum)
        header_ext:  interval, i8(encoder), i32(enc-len), i32(len), i32(cells), 
                     i32(data-checksum), i32(checksum)
        data:        [cell]
  */

  public:

  typedef Fragment* Ptr;
  
  static const uint8_t     HEADER_SIZE = 9;
  static const uint8_t     VERSION = 1;
  static const uint8_t     HEADER_EXT_FIXED_SIZE = 21;
  
  enum State {
    NONE,
    LOADING,
    LOADED,
    WRITING,
    ERROR,
  };

  static const std::string to_string(State state) {
    switch(state) {
      case State::NONE:
        return std::string("NONE");
      case State::LOADING:
        return std::string("LOADING");
      case State::LOADED:
        return std::string("LOADED");
      case State::ERROR:
        return std::string("ERROR");
      case State::WRITING:
        return std::string("WRITING");
      default:
        return std::string("UKNONWN");
    }
  }

  inline static Ptr make(const std::string& filepath, 
                         State state=State::NONE) {
    return new Fragment(filepath, state);
  }

  const int64_t         ts;
  DB::Cells::Interval   interval;

  Fragment(const std::string& filepath, State state=State::NONE)
          : ts(Time::now_ns()),
            m_smartfd(
              FS::SmartFd::make_ptr(
                filepath, FS::OpenFlags::OPEN_FLAG_OVERWRITE)
            ), 
            m_state(state), 
            m_size_enc(0), m_size(0), m_cells_count(0), m_cells_offset(0), 
            m_data_checksum(0), m_processing(0), m_cells_remain(0) {
  }
  
  Ptr ptr() {
    return this;
  }

  virtual ~Fragment() {
  }

  void write(int& err, int32_t replication, Types::Encoding encoder, 
             DynamicBuffer& cells, uint32_t cell_count) {
    m_version = VERSION;
    
    uint32_t header_extlen = interval.encoded_length()+HEADER_EXT_FIXED_SIZE;
    m_cells_remain = m_cells_count = cell_count;
    m_size = cells.fill();
    m_cells_offset = HEADER_SIZE+header_extlen;

    DynamicBuffer output;
    m_size_enc = 0;
    err = Error::OK;
    Encoder::encode(err, encoder, cells.base, m_size, 
                    &m_size_enc, output, m_cells_offset);
    if(err)
      return;

    if(m_size_enc) {
      m_encoder = encoder;
    } else {
      m_size_enc = m_size;
      m_encoder = Types::Encoding::PLAIN;
    }
                    
    uint8_t * bufp = output.base;
    Serialization::encode_i8(&bufp, m_version);
    Serialization::encode_i32(&bufp, header_extlen);
    checksum_i32(output.base, bufp, &bufp);

    uint8_t * header_extptr = bufp;
    interval.encode(&bufp);
    Serialization::encode_i8(&bufp, (uint8_t)m_encoder);
    Serialization::encode_i32(&bufp, m_size_enc);
    Serialization::encode_i32(&bufp, m_size);
    Serialization::encode_i32(&bufp, m_cells_count);
    
    checksum_i32(output.base+m_cells_offset, output.base+output.fill(), 
                 &bufp, m_data_checksum);
    checksum_i32(header_extptr, bufp, &bufp);

    StaticBuffer buff_write(output);

    do_again:
    Env::FsInterface::interface()->write(
      err,
      m_smartfd, 
      replication, m_cells_offset+m_size_enc, 
      buff_write
    );

    int tmperr = Error::OK;
    if(!err && Env::FsInterface::fs()->length(
        tmperr, m_smartfd->filepath()) != buff_write.size)
      err = Error::FS_EOF;

    if(err) {
      if(err == Error::FS_PATH_NOT_FOUND 
        //|| err == Error::FS_PERMISSION_DENIED
          || err == Error::SERVER_SHUTTING_DOWN) {
    
        // err == ? 
        // server already shutting down or major fs issue (PATH NOT FOUND)
        // write temp(local) file for recovery
        return;
      }
      // if not overwriteflag:
      //   Env::FsInterface::fs()->remove(tmperr, m_smartfd->filepath()) 
      goto do_again;
    }

    bool keep;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      keep = !m_queue.empty() || m_processing > 0;
      m_state = err ? State::ERROR : (keep ? State::LOADED : State::NONE);
      if(!err && keep)
        m_buffer.set(cells);
      else
        m_buffer.free();
    }
    if(keep)
      run_queued(err);
    else if(Env::Resources.need_ram(m_size))
      release();
  }

  void load_header(bool close_after=true) {
    int err = Error::OK;
    load_header(err, close_after);
    if(err) {
      m_state = State::ERROR;
      SWC_LOGF(LOG_ERROR, "CommitLog::Fragment load_header err=%d(%s) %s", 
                err, Error::get_text(err), to_string().c_str());
    }
  }

  void load(const std::function<void(int)>& cb) {
    bool loaded;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_processing++;
      loaded = m_state == State::LOADED;
      if(!loaded) {
        m_queue.push(cb);

        if(m_state == State::LOADING || m_state == State::WRITING)
          return;
        m_state = State::LOADING;
      }
    }

    if(loaded)
      asio::post(*Env::IoCtx::io()->ptr(), [cb](){ cb(Error::OK); } );
    else 
      asio::post(*Env::IoCtx::io()->ptr(), [ptr=ptr()](){ ptr->load(); } );
  }
  
  void load_cells(DB::Cells::Block::Ptr cells_block) {
    bool was_splitted = false;
    if(loaded()) {
      if(m_buffer.size)
        m_cells_remain -= cells_block->load_cells(
          m_buffer.base, m_buffer.size, m_cells_count, was_splitted);
    } else {
      //err
    }
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_processing--; 
    }

    if(!was_splitted 
       && (!m_cells_remain.load() || Env::Resources.need_ram(m_size)))
      release();
  }
  
  size_t release() {
    size_t released = 0;     
    std::lock_guard<std::mutex> lock(m_mutex);

    if(m_processing || m_state != State::LOADED)
      return released; 
 
    m_state = State::NONE;
    released += m_buffer.size;   
    m_buffer.free();
    m_cells_remain = m_cells_count;
    return released;
  }

  bool loaded() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == State::LOADED;
  }

  bool errored() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == State::ERROR;
  }

  uint32_t cells_count() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cells_count;
  }

  size_t size_bytes(bool only_loaded=false) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if(only_loaded && m_state != State::LOADED)
      return 0;
    return m_size;
  }

  bool processing() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_processing;
  }

  void wait_processing() {
    while(processing() > 0)  {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  void remove(int &err) {
    wait_processing();
    std::lock_guard<std::mutex> lock(m_mutex);
    Env::FsInterface::fs()->remove(err, m_smartfd->filepath()); 
  }

  const std::string to_string() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string s("Fragment(version=");
    s.append(std::to_string(m_version));

    s.append(" state=");
    s.append(to_string(m_state));

    s.append(" count=");
    s.append(std::to_string(m_cells_count));
    s.append(" offset=");
    s.append(std::to_string(m_cells_offset));

    s.append(" encoder=");
    s.append(Types::to_string(m_encoder));

    s.append(" enc/size=");
    s.append(std::to_string(m_size_enc));
    s.append("/");
    s.append(std::to_string(m_size));

    s.append(" ");
    s.append(interval.to_string());

    s.append(" ");
    s.append(m_smartfd->to_string());

    s.append(" queue=");
    s.append(std::to_string(m_queue.size()));

    s.append(" processing=");
    s.append(std::to_string(m_processing));
    
    s.append(")");
    return s;
  }

  private:
  

  void load_header(int& err, bool close_after=true) {
    
    for(;;) {
      err = Error::OK;
    
      if(!Env::FsInterface::fs()->exists(err, m_smartfd->filepath())) {
        if(err != Error::OK && err != Error::SERVER_SHUTTING_DOWN)
          continue;
        return;
      }
      
      if(!Env::FsInterface::interface()->open(err, m_smartfd) && err)
        return;
      if(err)
        continue;
      
      StaticBuffer buf;
      if(Env::FsInterface::fs()->pread(
          err, m_smartfd, 0, &buf, HEADER_SIZE) != HEADER_SIZE){
        if(err != Error::FS_EOF){
          Env::FsInterface::fs()->close(err, m_smartfd);
          continue;
        }
        return;
      }
      const uint8_t *ptr = buf.base;

      size_t remain = HEADER_SIZE;
      m_version = Serialization::decode_i8(&ptr, &remain);
      uint32_t header_extlen = Serialization::decode_i32(&ptr, &remain);
      if(!checksum_i32_chk(
        Serialization::decode_i32(&ptr, &remain), buf.base, HEADER_SIZE-4)){  
        err = Error::CHECKSUM_MISMATCH;
        return;
      }
      buf.free();
      
      if(Env::FsInterface::fs()->pread(err, m_smartfd, HEADER_SIZE, 
                                       &buf, header_extlen) != header_extlen) {
        if(err != Error::FS_EOF){
          Env::FsInterface::fs()->close(err, m_smartfd);
          continue;
        }
        return;
      }
      ptr = buf.base;

      remain = header_extlen;
      interval.decode(&ptr, &remain, true);
      m_encoder = (Types::Encoding)Serialization::decode_i8(&ptr, &remain);
      m_size_enc = Serialization::decode_i32(&ptr, &remain);
      m_size = Serialization::decode_i32(&ptr, &remain);
      m_cells_remain=m_cells_count = Serialization::decode_i32(&ptr, &remain);
      m_data_checksum = Serialization::decode_i32(&ptr, &remain);

      if(!checksum_i32_chk(
        Serialization::decode_i32(&ptr, &remain), buf.base, header_extlen-4)){  
        err = Error::CHECKSUM_MISMATCH;
        return;
      }
      m_cells_offset = HEADER_SIZE+header_extlen;
      break;
    }

    if(close_after && m_smartfd->valid()) {
      int tmperr = Error::OK;
      Env::FsInterface::fs()->close(tmperr, m_smartfd);
    }
  }

  void load() {
    int err;
    uint8_t tries = 0;
    for(;;) {
      err = Error::OK;
    
      if(!m_smartfd->valid() 
         && !Env::FsInterface::interface()->open(err, m_smartfd) && err)
        break;
      if(err)
        continue;
      
      m_buffer.free();
      if(Env::FsInterface::fs()->pread(
            err, m_smartfd, m_cells_offset, &m_buffer, m_size_enc) 
          != m_size_enc) {
        err = Error::FS_IO_ERROR;
      }
      if(m_smartfd->valid()) {
        int tmperr = Error::OK;
        Env::FsInterface::fs()->close(tmperr, m_smartfd);
      }
      if(err) {
        if(err == Error::FS_EOF)
          break;
        continue;
      }
      
      if(!checksum_i32_chk(m_data_checksum, m_buffer.base, m_size_enc)){  
        if(++tries == 3) {
          SWC_LOGF(LOG_WARN, "CHECKSUM_MISMATCH try=%d %s", 
                    tries, m_smartfd->to_string().c_str());
          err = Error::CHECKSUM_MISMATCH;
          break;
        }
        continue;
      }

      if(m_encoder != Types::Encoding::PLAIN) {
        StaticBuffer decoded_buf(m_size);
        Encoder::decode(
          err, m_encoder, m_buffer.base, m_size_enc, decoded_buf.base, m_size);
        if(err) {
          int tmperr = Error::OK;
          Env::FsInterface::fs()->close(tmperr, m_smartfd);
          break;
        }
        m_buffer.set(decoded_buf);
      }
      break;
    }
    
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_state = err ? State::ERROR : State::LOADED;
    }
    if(err)
      SWC_LOGF(LOG_ERROR, "CommitLog::Fragment load err=%d(%s) %s", 
                err, Error::get_text(err), to_string().c_str());

    run_queued(err);
  }


  void run_queued(int err) {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if(m_q_runs || m_queue.empty())
        return;
      m_q_runs = true;
    }
    
    asio::post(
      *Env::IoCtx::io()->ptr(), 
      [err, ptr=ptr()](){ ptr->_run_queued(err); }
    );
  }

  void _run_queued(int err) {
    std::function<void(int)> cb;
    for(;;) {
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        cb = m_queue.front();
      }

      cb(err);
      
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.pop();
        if(m_queue.empty()) {
          m_q_runs = false;
          return;
        }
      }
    }
  }
  
  std::mutex        m_mutex;
  State             m_state;
  FS::SmartFd::Ptr  m_smartfd;
  uint8_t           m_version;
  Types::Encoding   m_encoder;
  size_t            m_size_enc;
  size_t            m_size;
  StaticBuffer      m_buffer;
  uint32_t          m_cells_count;
  uint32_t          m_cells_offset;
  uint32_t          m_data_checksum;
  size_t            m_processing;

  std::atomic<uint32_t> m_cells_remain;

  bool                                 m_q_runs = false;
  std::queue<std::function<void(int)>> m_queue;
  

};

}

}}

#endif