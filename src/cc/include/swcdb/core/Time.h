/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */


#ifndef swc_core_Time_h
#define swc_core_Time_h

#include "swcdb/core/Compat.h"

#include <iostream>


namespace SWC { namespace Time {


void checkings();

const int64_t now_ms();

const int64_t now_ns();

const int64_t parse_ns(int& err, const std::string& buf);

const std::string fmt_ns(int64_t ns);

std::ostream &hires_now_ns(std::ostream &out);


}}


#ifdef SWC_IMPL_SOURCE
#include "swcdb/core/Time.cc"
#endif 

#endif // swc_core_Time_h
