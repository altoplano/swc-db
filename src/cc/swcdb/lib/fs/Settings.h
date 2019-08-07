/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */

#ifndef swc_lib_fs_Settings_h
#define swc_lib_fs_Settings_h

#include "swcdb/lib/core/config/Settings.h"

namespace SWC{ namespace Config {


inline void Settings::init_fs_options(){
  file_desc().add_options()
    ("swc.fs.path.data", str("swcdb/"), 
     "SWC-DB data-path, within the FS(specific) base-path")

    ("swc.fs", str(), "main FileSystem: local|hadoop|ceph|broker|custom")

    ("swc.fs.local.cfg", str(), "Specific cfg-file for FS-local")
    ("swc.fs.lib.local", str(), "FS-local Lib-path based on fs/FileSystem.h")

    ("swc.fs.hadoop.cfg", str(), "Specific cfg-file for FS-hadoop")
    ("swc.fs.lib.hadoop", str(), "FS-hadoop Lib-path based on fs/FileSystem.h")

    ("swc.fs.ceph.cfg", str(), "Specific cfg-file for FS-ceph")
    ("swc.fs.lib.ceph", str(), "FS-ceph Lib-path based on fs/FileSystem.h")

    ("swc.fs.custom.cfg", str(), "Specific cfg-file for FS-custom")
    ("swc.fs.lib.custom", str(), "FS-custom Lib-path based on fs/FileSystem.h")

    ("swc.fs.broker.cfg", str(), "Specific cfg-file for FS-broker")
    ("swc.fs.broker.underlying", str(), 
      "as main FileSystem, without 'broker': local|hadoop|ceph|custom")
    ("swc.fs.lib.broker", str(), "FS-broker Lib-path based on fs/FileSystem.h")
  
  ;
}

}}

#endif // swc_lib_fs_Settings_h