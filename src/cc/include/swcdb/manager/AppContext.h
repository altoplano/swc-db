/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */

#ifndef swc_app_manager_AppContext_h
#define swc_app_manager_AppContext_h

#include "swcdb/db/Protocol/Commands.h"

#include "swcdb/core/comm/AppContext.h"
#include "swcdb/core/comm/AppHandler.h"
#include "swcdb/core/comm/ResponseCallback.h"
#include "swcdb/core/comm/DispatchHandler.h"

#include "swcdb/fs/Interface.h"
#include "swcdb/client/Clients.h"

#include "swcdb/db/Columns/Schema.h"
#include "swcdb/db/Columns/Mngr/Columns.h"

#include "swcdb/manager/MngrRole.h"
#include "swcdb/manager/Rangers.h"

#include "swcdb/db/Protocol/Common/handlers/NotImplemented.h"
#include "swcdb/db/Protocol/Common/handlers/Echo.h"
#include "swcdb/manager/handlers/MngrState.h"
#include "swcdb/manager/handlers/MngrActive.h"
#include "swcdb/manager/handlers/ColumnMng.h"
#include "swcdb/manager/handlers/ColumnUpdate.h"
#include "swcdb/manager/handlers/ColumnGet.h"
#include "swcdb/manager/handlers/RgrMngId.h"
#include "swcdb/manager/handlers/RgrUpdate.h"
#include "swcdb/manager/handlers/RgrGet.h"


namespace SWC { namespace server { namespace Mngr {


class AppContext : public SWC::AppContext {
  
  public:

  AppContext() {
    Env::Config::settings()->parse_file(
      Env::Config::settings()->get<std::string>("swc.mngr.cfg", ""),
      Env::Config::settings()->get<std::string>("swc.mngr.OnFileChange.cfg", "")
    );

    Env::IoCtx::init(
      Env::Config::settings()->get<int32_t>("swc.mngr.handlers"));

    Env::FsInterface::init(FS::fs_type(
      Env::Config::settings()->get<std::string>("swc.fs")));
      
    Env::MngrRole::init();
    
    Env::Schemas::init();
    Env::MngrColumns::init();
    Env::Clients::init(
      std::make_shared<client::Clients>(
        Env::IoCtx::io()->shared(),
        std::make_shared<client::Mngr::AppContext>()
      )
    );
    Env::Rangers::init();
  }
  
  void init(const EndPoints& endpoints) override {
    Env::MngrRole::get()->init(endpoints);
    
    int sig = 0;
    Env::IoCtx::io()->set_signals();
    shutting_down(std::error_code(), sig);
  }

  void set_srv(SerializedServer::Ptr srv){
    m_srv = srv;
  }

  virtual ~AppContext(){}


  void handle(ConnHandlerPtr conn, Event::Ptr& ev) override {
    // SWC_LOGF(LOG_DEBUG, "handle: %s", ev->to_str().c_str());

    switch (ev->type) {

      case Event::Type::ESTABLISHED:
        m_srv->connection_add(conn);
        return; 
        
      case Event::Type::DISCONNECT:
        m_srv->connection_del(conn);
        if(Env::MngrRole::get()->disconnection(
                          conn->endpoint_remote, conn->endpoint_local, true))
          return;
        return;

      case Event::Type::ERROR:
        //rangers->decommision(event->addr);
        break;

      case Event::Type::MESSAGE: {
        AppHandler *handler = 0;
        switch (ev->header.command) {

          case Protocol::Mngr::MNGR_ACTIVE:
            handler = new Protocol::Mngr::Handler::MngrActive(conn, ev);
            break;

          case Protocol::Mngr::MNGR_STATE:
            handler = new Protocol::Mngr::Handler::MngrState(conn, ev);
            break;

          case Protocol::Mngr::COLUMN_MNG:
            handler = new Protocol::Mngr::Handler::ColumnMng(conn, ev);
            break;

          case Protocol::Mngr::COLUMN_GET:
            handler = new Protocol::Mngr::Handler::ColumnGet(conn, ev);
            break;

          case Protocol::Mngr::COLUMN_UPDATE:
            handler = new Protocol::Mngr::Handler::ColumnUpdate(conn, ev);
            break;

          case Protocol::Mngr::RGR_GET:
            handler = new Protocol::Mngr::Handler::RgrGet(conn, ev);
            break;

          case Protocol::Mngr::RGR_MNG_ID:
            handler = new Protocol::Mngr::Handler::RgrMngId(conn, ev);
            break;

          case Protocol::Mngr::RGR_UPDATE:
            handler = new Protocol::Mngr::Handler::RgrUpdate(conn, ev);
            break;

          case Protocol::Common::DO_ECHO:
            handler = new Protocol::Common::Handler::Echo(conn, ev);
            break;

          default: 
            handler = new Protocol::Common::Handler::NotImplemented(conn, ev);
            break;
        }

        if(handler)
          asio::post(*Env::IoCtx::io()->ptr(), 
                    [hdlr=AppHandler::Ptr(handler)](){ hdlr->run();  });
        //std::cout << " cmd=" << ev->header.command << "\n";
        break;
      }

      default:
        SWC_LOGF(LOG_WARN, "Unimplemented event-type (%llu)", (Llu)ev->type);
        break;

    }
  }
  
  void shutting_down(const std::error_code &ec, const int &sig) {

    if(sig==0){ // set signals listener
      Env::IoCtx::io()->signals()->async_wait(
        [ptr=this](const std::error_code &ec, const int &sig){
          SWC_LOGF(LOG_INFO, "Received signal, sig=%d ec=%s", sig, ec.message().c_str());
          ptr->shutting_down(ec, sig); 
        }
      ); 
      SWC_LOGF(LOG_INFO, "Listening for Shutdown signal, set at sig=%d ec=%s", 
              sig, ec.message().c_str());
      return;
    }

    SWC_LOGF(LOG_INFO, "Shutdown signal, sig=%d ec=%s", sig, ec.message().c_str());
    
    if(m_srv == nullptr) {
      SWC_LOG(LOG_INFO, "Exit");
      std::quick_exit(0);
    }
    
    (new std::thread([ptr=shared_from_this()]{ ptr->stop(); }))->detach();
  }

  void stop() override {
    
    m_srv->stop_accepting(); // no further requests accepted
    
    Env::Rangers::get()->stop();
    Env::MngrRole::get()->stop();

    Env::Clients::get()->rgr_service->stop();
    Env::Clients::get()->mngr_service->stop();

    Env::IoCtx::io()->stop();
    Env::FsInterface::interface()->stop();
    
    m_srv->shutdown();
    
    SWC_LOG(LOG_INFO, "Exit");
    std::quick_exit(0);
  }

  private:
  SerializedServer::Ptr m_srv = nullptr;
  //ColmNameToIDMap columns;       // column-name > CID


};

}}}

#endif // swc_app_manager_AppContext_h