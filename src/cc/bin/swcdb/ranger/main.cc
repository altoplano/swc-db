/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */


#include "swcdb/ranger/Settings.h"
#include "swcdb/core/comm/SerializedServer.h"
#include "swcdb/ranger/AppContext.h"


namespace SWC {

int run() {
  SWC_TRY_OR_LOG("", 
  
  auto app_ctx = Ranger::AppContext::make();

  auto srv = std::make_shared<server::SerializedServer>(
    "RANGER", 
    Env::Config::settings()->get_i32("swc.rgr.reactors"), 
    Env::Config::settings()->get_i32("swc.rgr.workers"), 
    "swc.rgr.port",
    app_ctx
  );
  app_ctx->set_srv(srv);
  srv->run();

  return 0);
  return 1;
}

}

int main(int argc, char** argv) {
  SWC::Env::Config::init(argc, argv);
  SWC::Env::Config::settings()->init_process();
  return SWC::run();
}
