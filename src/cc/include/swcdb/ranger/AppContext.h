/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */

#ifndef swc_ranger_AppContext_h
#define swc_ranger_AppContext_h

#include "swcdb/core/sys/Resources.h"

#include "swcdb/core/comm/AppContext.h"
#include "swcdb/core/comm/AppHandler.h"
#include "swcdb/core/comm/ResponseCallback.h"
#include "swcdb/core/comm/DispatchHandler.h"

#include "swcdb/db/client/Clients.h"
#include "swcdb/ranger/AppContextClient.h"
#include "swcdb/ranger/RangerEnv.h"

#include "swcdb/db/Protocol/Commands.h"

#include "swcdb/common/Protocol/handlers/NotImplemented.h"
#include "swcdb/ranger/Protocol/Mngr/req/RgrMngId.h"

#include "swcdb/ranger/Protocol/handlers/AssignId.h"
#include "swcdb/ranger/Protocol/handlers/RangeLoad.h"
#include "swcdb/ranger/Protocol/handlers/RangeUnload.h"
#include "swcdb/ranger/Protocol/handlers/RangeIsLoaded.h"
#include "swcdb/ranger/Protocol/handlers/RangeLocate.h"
#include "swcdb/ranger/Protocol/handlers/RangeQueryUpdate.h"
#include "swcdb/ranger/Protocol/handlers/RangeQuerySelect.h"
#include "swcdb/ranger/Protocol/handlers/ColumnUpdate.h"
#include "swcdb/ranger/Protocol/handlers/ColumnDelete.h"
#include "swcdb/ranger/Protocol/handlers/ColumnCompact.h"
#include "swcdb/ranger/Protocol/handlers/Report.h"


namespace SWC { namespace Ranger {



class AppContext : public SWC::AppContext { 

  // in-order of Protocol::Rgr::Command
  static constexpr const AppHandler_t handlers[] = { 
    &Protocol::Common::Handler::not_implemented,
    &Protocol::Rgr::Handler::column_delete,
    &Protocol::Rgr::Handler::column_compact,
    &Protocol::Rgr::Handler::column_update,
    &Protocol::Rgr::Handler::range_is_loaded,
    &Protocol::Rgr::Handler::range_load,
    &Protocol::Rgr::Handler::range_unload,
    &Protocol::Rgr::Handler::range_locate,
    &Protocol::Rgr::Handler::range_query_update,
    &Protocol::Rgr::Handler::range_query_select,
    &Protocol::Rgr::Handler::report,
    //&Handler::debug,
    //&Handler::status,
    //&Handler::shutdown
  }; 
  
  public:

  static std::shared_ptr<AppContext> make() {
    auto settings = Env::Config::settings();
    
    settings->parse_file(
      settings->get_str("swc.rgr.cfg", ""),
      "swc.rgr.cfg.dyn"
    );

    Env::IoCtx::init(settings->get_i32("swc.rgr.handlers"));
    
    Env::FsInterface::init(FS::fs_type(settings->get_str("swc.fs")));
      
    Env::Clients::init(
      std::make_shared<client::Clients>(
        Env::IoCtx::io()->shared(),
        std::make_shared<client::Rgr::AppContext>()
      )
    );
    RangerEnv::init();

    Env::Resources.init(
      Env::IoCtx::io()->ptr(),
      settings->get<Property::V_GINT32>("swc.rgr.ram.percent"),
      settings->get<Property::V_GINT32>("swc.rgr.ram.release.rate"),
      [](size_t bytes) { RangerEnv::columns()->release(bytes); }
    );

    auto period = settings->get<Property::V_GINT32>("swc.cfg.dyn.period");
    if(period->get()) {
      Env::IoCtx::io()->set_periodic_timer(
        period,
        [](){Env::Config::settings()->check_dynamic_files();}
      );
    }

    auto app = std::make_shared<AppContext>();
    app->id_mngr = std::make_shared<Protocol::Mngr::Req::RgrMngId>(
      Env::IoCtx::io()->ptr(), 
      [app]() { (new std::thread([app]{ app->stop(); }))->detach(); }
    );
    return app;
  }

  AppContext() { }

  void init(const EndPoints& endpoints) override {
    RangerEnv::rgr_data()->endpoints = endpoints;
    
    int sig = 0;
    Env::IoCtx::io()->set_signals();
    shutting_down(std::error_code(), sig);

    RangerEnv::start();
    id_mngr->request();
  }

  void set_srv(server::SerializedServer::Ptr srv){
    m_srv = srv;
  }

  virtual ~AppContext(){}


  void handle(ConnHandlerPtr conn, Event::Ptr& ev) override {

    // SWC_LOGF(LOG_DEBUG, "handle: %s", ev->to_str().c_str());
    
    switch (ev->type) {

      case Event::Type::ESTABLISHED:
        m_srv->connection_add(conn);
        break; 
        
      case Event::Type::DISCONNECT:
        m_srv->connection_del(conn);
        break;

      case Event::Type::ERROR:
        break;

      case Event::Type::MESSAGE: {
        uint8_t cmd = ev->header.command >= Protocol::Rgr::MAX_CMD
                    ? Protocol::Rgr::NOT_IMPLEMENTED : ev->header.command;
        
        if(cmd == Protocol::Rgr::ASSIGN_ID_NEEDED) {
          Protocol::Rgr::Handler::assign_id(conn, ev, id_mngr);
        
        } else if(!RangerEnv::rgr_data()->id) {
          try{conn->send_error(Error::RS_NOT_READY, "", ev);}catch(...){}

        } else {
          asio::post(
            *Env::IoCtx::io()->ptr(), 
            [cmd, conn, ev]() { 
              handlers[cmd](conn, ev); 
            }
          );
        }
        break;
      }

      default:
        SWC_LOGF(LOG_WARN, "Unimplemented event-type (%llu)", (Llu)ev->type);
        break;
    }

  }

  void shutting_down(const std::error_code &ec, const int &sig) {

    if(sig==0) { // set signals listener
      Env::IoCtx::io()->signals()->async_wait(
        [this](const std::error_code &ec, const int &sig) {
          SWC_LOGF(LOG_INFO, "Received signal, sig=%d ec=%s", 
                   sig, ec.message().c_str());
          shutting_down(ec, sig); 
        }
      ); 
      SWC_LOGF(LOG_INFO, "Listening for Shutdown signal, set at sig=%d ec=%s", 
              sig, ec.message().c_str());
      return;
    }
    SWC_LOGF(LOG_INFO, "Shutdown signal, sig=%d ec=%s", 
             sig, ec.message().c_str());
    
    if(m_srv == nullptr) {
      SWC_LOG(LOG_INFO, "Exit");
      std::quick_exit(0);
    }
    

    RangerEnv::shuttingdown();

    m_srv->stop_accepting(); // no further requests accepted

    id_mngr->request();
  }

  void stop() override {
    
    while(RangerEnv::in_process())
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    RangerEnv::columns()->unload_all(true); //re-check
    
    Env::Resources.stop();
    
    Env::Clients::get()->rgr_service->stop();
    Env::Clients::get()->mngr_service->stop();
    
    Env::IoCtx::io()->stop();
    Env::FsInterface::interface()->stop();
    
    m_srv->shutdown();

    SWC_LOG(LOG_INFO, "Exit");
    std::quick_exit(0);
  }

  private:
  
  Protocol::Mngr::Req::RgrMngId::Ptr id_mngr = nullptr;
  server::SerializedServer::Ptr      m_srv = nullptr;
  
};

}}

#endif // swc_ranger_AppContext_h