/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */

#ifndef swc_manager_MngrEnv_h
#define swc_manager_MngrEnv_h

#include "swcdb/db/Columns/Schemas.h"
#include "swcdb/manager/db/Columns.h"

#include "swcdb/manager/MngrRole.h"
#include "swcdb/manager/Rangers.h"
#include "swcdb/manager/MngdColumns.h"


namespace SWC { namespace Env {

class Mngr final {
  public:

  static void init(const EndPoints& endpoints) {
    m_env = std::make_shared<Mngr>(endpoints);
  }

  static DB::Schemas* schemas() {
    return &m_env->m_schemas;
  }

  static Manager::Columns* columns() {
    return &m_env->m_columns;
  }

  static Manager::MngrRole* role() {
    return &m_env->m_role;
  }

  static Manager::Rangers* rangers() {
    return &m_env->m_rangers;
  }
  
  static Manager::MngdColumns* mngd_columns() {
    return &m_env->m_mngd_columns;
  }

  static void stop();


  Mngr(const EndPoints& endpoints) 
      : m_role(endpoints) { 
  }

  ~Mngr() { }

  private:

  inline static std::shared_ptr<Mngr> m_env = nullptr;
  DB::Schemas                         m_schemas;
  Manager::Columns                    m_columns;
  Manager::MngrRole                   m_role;
  Manager::Rangers                    m_rangers;
  Manager::MngdColumns                m_mngd_columns;
  
};


}} // SWC::Env namespace

#include "swcdb/manager/MngrRole.cc"
#include "swcdb/manager/Rangers.cc"
#include "swcdb/manager/MngdColumns.cc"



namespace SWC { namespace Env {

void Mngr::stop() {
  m_env->m_rangers.stop();
  m_env->m_mngd_columns.stop();
  m_env->m_role.stop();
}

}} 

#endif // swc_manager_MngrEnv_h