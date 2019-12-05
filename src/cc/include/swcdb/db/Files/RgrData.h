/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swcdb_db_Files_RgrData_h
#define swcdb_db_Files_RgrData_h


#include "swcdb/core/DynamicBuffer.h"
#include "swcdb/core/Time.h"
#include "swcdb/core/Checksum.h"

namespace SWC { namespace Files {


class RgrData final {
  /* file-format: 
      header: i8(version), i32(data-len), 
              i32(data-checksum), i32(header-checksum),
      data:   i64(ts), vi64(id), 
              i32(num-points), [endpoint]
  */

  public:

  typedef std::shared_ptr<RgrData> Ptr;

  static const int HEADER_SIZE=13;
  static const int HEADER_OFFSET_CHKSUM=9;
  
  static const int8_t VERSION=1;

  static Ptr get_rgr(int &err, std::string filepath){
    Ptr data = std::make_shared<RgrData>();
    try{
      data->read(err, FS::SmartFd::make_ptr(filepath, 0));
    } catch(...){
      data = std::make_shared<RgrData>();
    }
    return data;
  }

  RgrData(): version(VERSION), id(0), timestamp(0) { }

  void read(int &err, FS::SmartFd::Ptr smartfd) {
    for(;;) {
      err = Error::OK;
    
      if(!Env::FsInterface::fs()->exists(err, smartfd->filepath())){
        if(err != Error::OK && err != Error::SERVER_SHUTTING_DOWN)
          continue;
        return;
      }

      Env::FsInterface::fs()->open(err, smartfd);
      if(!smartfd->valid())
        continue;
      if(err != Error::OK){
        Env::FsInterface::fs()->close(err, smartfd);
        continue;
      }

      uint8_t buf[HEADER_SIZE];
      const uint8_t *ptr = buf;
      if(Env::FsInterface::fs()->read(err, smartfd, buf, 
                                      HEADER_SIZE) != HEADER_SIZE){
        if(err != Error::FS_EOF){
          Env::FsInterface::fs()->close(err, smartfd);
          continue;
        }
        break;
      }

      size_t remain = HEADER_SIZE;
      version = Serialization::decode_i8(&ptr, &remain);
      size_t sz = Serialization::decode_i32(&ptr, &remain);
      size_t chksum_data = Serialization::decode_i32(&ptr, &remain);
      
      if(!checksum_i32_chk(Serialization::decode_i32(&ptr, &remain), 
                           buf, HEADER_SIZE, HEADER_OFFSET_CHKSUM))
        break;


      StaticBuffer read_buf;
      if(Env::FsInterface::fs()->read(err, smartfd, &read_buf, sz) != sz){
        if(err != Error::FS_EOF){
          Env::FsInterface::fs()->close(err, smartfd);
          continue;
        }
        break;
      }
      ptr = read_buf.base;

      if(!checksum_i32_chk(chksum_data, ptr, sz))
        break;

      read(&ptr, &sz);
      break;
    }
    
    if(smartfd->valid())
      Env::FsInterface::fs()->close(err, smartfd);
  }

  void read(const uint8_t **ptr, size_t* remain) {

    timestamp = Serialization::decode_i64(ptr, remain);
    id = Serialization::decode_vi64(ptr, remain);

    uint32_t len = Serialization::decode_i32(ptr, remain);
    for(size_t i=0;i<len;i++)
      endpoints.push_back(Serialization::decode(ptr, remain));
  }
  
  // SET 
  void set_rgr(int &err, std::string filepath, int64_t ts = 0){

    DynamicBuffer input;
    write(input, ts==0 ? Time::now_ns() : ts);
    StaticBuffer send_buf(input);

    Env::FsInterface::interface()->write(
      err,
      FS::SmartFd::make_ptr(filepath, FS::OpenFlags::OPEN_FLAG_OVERWRITE), 
      -1, -1, 
      send_buf
    );
  }

  void write(SWC::DynamicBuffer &dst_buf, int64_t ts){
    
    size_t len = 12 // (ts+endpoints.size)
               + Serialization::encoded_length_vi64(id.load());
    for(auto& endpoint : endpoints)
      len += Serialization::encoded_length(endpoint);
    dst_buf.ensure(HEADER_SIZE+len);

    Serialization::encode_i8(&dst_buf.ptr, version);
    Serialization::encode_i32(&dst_buf.ptr, len);

    uint8_t* checksum_data_ptr = dst_buf.ptr;
    Serialization::encode_i32(&dst_buf.ptr, 0);
    uint8_t* checksum_header_ptr = dst_buf.ptr;
    Serialization::encode_i32(&dst_buf.ptr, 0);

    const uint8_t* start_data_ptr = dst_buf.ptr;
    Serialization::encode_i64(&dst_buf.ptr, ts);
    Serialization::encode_vi64(&dst_buf.ptr, id.load());
    
    Serialization::encode_i32(&dst_buf.ptr, endpoints.size());
    for(auto& endpoint : endpoints)
      Serialization::encode(endpoint, &dst_buf.ptr);

    checksum_i32(start_data_ptr, dst_buf.ptr, &checksum_data_ptr);
    checksum_i32(dst_buf.base, start_data_ptr, &checksum_header_ptr);

    assert(dst_buf.fill() <= dst_buf.size);
  }

  std::string to_string(){
    std::string s("RgrData(");
    s.append("endpoints=(");
    for(auto& endpoint : endpoints){
      s.append("[");
      s.append(endpoint.address().to_string());
      s.append("]:");
      s.append(std::to_string(endpoint.port()));
      s.append(",");
    }
    s.append(")");
    
    s.append(", version=");
    s.append(std::to_string(version));
    s.append(", id=");
    s.append(std::to_string(id.load()));
    s.append(", timestamp=");
    s.append(std::to_string(timestamp));
    s.append(")");
    return s;
  } 
  
  ~RgrData(){ }

  int8_t     version;
  std::atomic<int64_t>   id;
  int64_t   timestamp;
  EndPoints endpoints;

};

} // Files namespace


namespace Env {
class RgrData final {  
  public:


  static void init() {
    m_env = std::make_shared<RgrData>();
  }

  static Files::RgrData* get(){
    HT_ASSERT(m_env != nullptr);
    return m_env->m_rgr_data;
  }

  static bool is_shuttingdown(){
    return m_env->m_shuttingdown;
  }

  static void shuttingdown(){
    m_env->m_shuttingdown = true;
  }

  static int64_t in_process(){
    return m_env->m_in_process;
  }

  static void in_process(int64_t count){
    m_env->m_in_process += count;
  }

  RgrData() : m_rgr_data(new Files::RgrData()) {}

  ~RgrData(){
    if(m_rgr_data != nullptr)
      delete m_rgr_data;
  }


  private:
  inline static std::shared_ptr<RgrData>  m_env           = nullptr;
  Files::RgrData*                         m_rgr_data      = nullptr;
  std::atomic<bool>                       m_shuttingdown  = false;
  std::atomic<int64_t>                    m_in_process    = 0;
  
};
}

}
#endif