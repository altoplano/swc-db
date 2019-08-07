/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */

#ifndef swc_app_fsbroker_AppContext_h
#define swc_app_fsbroker_AppContext_h

#include "swcdb/lib/core/comm/AppContext.h"
#include "swcdb/lib/core/comm/AppHandler.h"

#include "swcdb/lib/fs/Interface.h"
#include "swcdb/lib/fs/Broker/Protocol/Commands.h"

#include "handlers/Exists.h"


namespace SWC { namespace server { namespace FsBroker {


class AppContext : public SWC::AppContext {
  
  public:

  AppContext() {
    EnvIoCtx::init(
      EnvConfig::settings()->get<int32_t>("swc.FsBroker.handlers"));
    EnvFsInterface::init();
  }
  
  void init(EndPoints endpoints) override {}

  void set_srv(SerializedServerPtr srv){
    m_srv = srv;
  }

  virtual ~AppContext(){}

  void handle(ConnHandlerPtr conn, EventPtr ev) override {
    HT_DEBUGF("handle: %s", ev->to_str().c_str());

    switch (ev->type) {

      case Event::Type::CONNECTION_ESTABLISHED:
        return;

      case Event::Type::DISCONNECT:
        break;

      case Event::Type::ERROR:
        break;

      case Event::Type::MESSAGE: {
        
        AppHandler *handler = 0;
        switch (ev->header.command) {

          case FS::Protocol::Cmd::FUNCTION_EXISTS:
            handler = new Handler::Exists(conn, ev);
            break;

          default: {
            conn->send_error(Error::NOT_IMPLEMENTED, 
              format("event command (%llu)",(Llu)ev->header.command), ev);
          }
        }

        if(handler)
          asio::post(*EnvIoCtx::io()->ptr(), [handler](){ handler->run();  });

        break;
      }

      default:
        conn->send_error(Error::NOT_IMPLEMENTED, 
          format("event-type (%llu)",(Llu)ev->type), ev);

    }
  }
  
  void shutting_down(const std::error_code &ec, const int &sig) {

    if(sig==0){ // set signals listener
      EnvIoCtx::io()->signals()->async_wait(
        [ptr=this](const std::error_code &ec, const int &sig){
          HT_INFOF("Received signal, sig=%d ec=%s", sig, ec.message().c_str());
          ptr->shutting_down(ec, sig); 
        }
      ); 
      HT_INFOF("Listening for Shutdown signal, set at sig=%d ec=%s", 
              sig, ec.message().c_str());
      return;
    }

    HT_INFOF("Shutdown signal, sig=%d ec=%s", sig, ec.message().c_str());
    stop();
  }

  void stop(){
    HT_INFO("Stopping APP-FSBROKER");
    
    m_srv->stop_accepting(); // no further requests accepted
    
    // + close all fds
    
    EnvIoCtx::io()->stop();
    EnvFsInterface::fs()->stop();
    
    m_srv->shutdown();
  }

  private:
  SerializedServerPtr m_srv = nullptr;
};

}}}

#endif // swc_app_fsbroker_AppContext_h