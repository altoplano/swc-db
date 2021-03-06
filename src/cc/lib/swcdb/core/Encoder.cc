/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */

#include <cstring> 
#include <algorithm>

#include "swcdb/core/Error.h"
#include "swcdb/core/DynamicBuffer.h"
#include "swcdb/core/Encoder.h"

#include <snappy.h>
#include <zlib.h>
#include <zstd.h>

namespace SWC { namespace Encoder {


void decode(int& err, Types::Encoding encoder, 
            const uint8_t* src, size_t sz_enc, 
            uint8_t *dst, size_t sz) {

  switch(encoder) {
    case Types::Encoding::ZLIB: {
      z_stream strm;
      memset(&strm, 0, sizeof(z_stream));
      strm.zalloc = Z_NULL;
      strm.zfree = Z_NULL;
      strm.opaque = Z_NULL;
      strm.avail_in = 0;
      strm.next_in = Z_NULL;
      if(::inflateInit(&strm) != Z_OK) {
        err = Error::ENCODER_DECODE;
        return;
      }
      strm.avail_in = sz_enc;
      strm.next_in = (Bytef *)src;
      strm.avail_out = sz;
      strm.next_out = dst;
      if(::inflate(&strm, Z_NO_FLUSH) != Z_STREAM_END || strm.avail_out)
        err = Error::ENCODER_DECODE;
      ::inflateReset(&strm);
      return;
    }

    case Types::Encoding::SNAPPY: {
      if(!snappy::RawUncompress((const char *)src, sz_enc, (char *)dst))
        err = Error::ENCODER_DECODE;
      return;
    }

    case Types::Encoding::ZSTD: {
      if(ZSTD_decompress((void *)dst, sz, (void *)src, sz_enc) != sz)
        err = Error::ENCODER_DECODE;
      return;
    }

    default: {
      //SWC_ASSERT(encoder==Types::Encoding::PLAIN);
      break;
    }
  }
}

void encode(int& err, Types::Encoding encoder, 
            const uint8_t* src, size_t src_sz, 
            size_t* sz_enc, DynamicBuffer& output, 
            uint32_t reserve) {
  
  switch(encoder) {
    case Types::Encoding::ZLIB: {

      z_stream strm;
      memset(&strm, 0, sizeof(z_stream));
      strm.zalloc = Z_NULL;
      strm.zfree = Z_NULL;
      strm.opaque = Z_NULL;
      if(::deflateInit(&strm, Z_DEFAULT_COMPRESSION) == Z_OK) {

        uint32_t avail_out = src_sz + 6 + (((src_sz / 16000) + 1) * 5);
        output.ensure(reserve + avail_out);
        output.ptr += reserve;
        
        strm.avail_out = avail_out;
        strm.next_out = output.ptr;
        strm.avail_in = src_sz;
        strm.next_in = (Bytef*)src;
        if(::deflate(&strm, Z_FINISH) == Z_STREAM_END)
          *sz_enc = avail_out - strm.avail_out;
      }
      ::deflateReset(&strm);
      if(*sz_enc && *sz_enc < src_sz) {
        output.ptr += *sz_enc;
        return;
      }
      break;
    }

    case Types::Encoding::SNAPPY: {
      output.ensure(reserve + snappy::MaxCompressedLength(src_sz));
      output.ptr += reserve;
      snappy::RawCompress((const char *)src, src_sz, 
                          (char *)output.ptr, sz_enc);
      if(*sz_enc && *sz_enc < src_sz) {
        output.ptr += *sz_enc;
        return;
      }
      break;
    }

    case Types::Encoding::ZSTD: {
      size_t const avail_out = ZSTD_compressBound(src_sz);
      output.ensure(reserve + avail_out);
      output.ptr += reserve;
      
      *sz_enc = ZSTD_compress(
        (void *)output.ptr, avail_out,
        (void *)src, src_sz,
        ZSTD_CLEVEL_DEFAULT
      );
      if(*sz_enc && !ZSTD_isError(*sz_enc) && *sz_enc < src_sz) {
        output.ptr += *sz_enc;
        return;
      }
      break;
    }

    default: 
      break;
  }

  *sz_enc = 0; 
  output.free();

  output.ensure(reserve + src_sz);
  output.ptr += reserve;
  if(src_sz)
    output.add_unchecked(src, src_sz);
}

}}
