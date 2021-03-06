/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */
 
#ifndef swc_fs_Broker_Protocol_params_Sync_h
#define swc_fs_Broker_Protocol_params_Sync_h



namespace SWC { namespace FS { namespace Protocol { namespace Params {

class SyncReq : public Serializable {
  public:
  
  SyncReq() {}

  SyncReq(int32_t fd) : fd(fd) {}

  int32_t fd;

  private:

  uint8_t encoding_version() const override {
    return 1;
  }

  size_t encoded_length_internal() const override {
    return 4;
  }

  void encode_internal(uint8_t **bufp) const override {
    Serialization::encode_i32(bufp, fd);
  }

  void decode_internal(uint8_t version, const uint8_t **bufp,
	                     size_t *remainp) override {
    fd = (int32_t)Serialization::decode_i32(bufp, remainp);
  }
  
};

}}}}

#endif // swc_fs_Broker_Protocol_params_Sync_h
