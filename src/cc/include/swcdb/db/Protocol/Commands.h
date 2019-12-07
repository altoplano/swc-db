
/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */

#ifndef swc_lib_db_Protocol_Commands_h
#define swc_lib_db_Protocol_Commands_h


namespace SWC { namespace Protocol {


namespace Common {
  // Common Protocol Commands
  enum Command {
    DO_ECHO              = 0xff
  };
  
}


namespace Rgr {
  // Ranger Protocol Commands
  enum Command {
    NOT_IMPLEMENTED      = 0x00,
    
    ASSIGN_ID_NEEDED     = 0x01,
    COLUMN_DELETE        = 0x02,
    SCHEMA_UPDATE        = 0x03,
    RANGE_IS_LOADED      = 0x04,
    RANGE_LOAD           = 0x05,
    RANGE_UNLOAD         = 0x06,
    RANGE_LOCATE         = 0x07,
    RANGE_QUERY_UPDATE   = 0x08,
    RANGE_QUERY_SELECT   = 0x09
  };

}


namespace Mngr {
  // Manager Protocol Commands
  enum Command {
    NOT_IMPLEMENTED      = 0x00,
    
    MNGR_STATE           = 0x01,
    MNGR_ACTIVE          = 0x02,
    COLUMN_MNG           = 0x03,
    COLUMN_UPDATE        = 0x04,
    COLUMN_GET           = 0x05,
    RGR_MNG_ID           = 0x06,
    RGR_UPDATE           = 0x07,
    RGR_GET              = 0x08,
    MAX_CMD              = 0x09
  };

}


}}

#endif // swc_lib_db_Protocol_Commands_h