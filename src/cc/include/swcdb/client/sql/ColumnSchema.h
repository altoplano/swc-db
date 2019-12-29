/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swcdb_client_sql_ColumnSchema_h
#define swcdb_client_sql_ColumnSchema_h



namespace SWC { namespace client { namespace SQL {

class ColumnSchema : public Reader {

  public:
  enum Func {
    CREATE,
    MODIFY,
    DELETE
  };

  ColumnSchema(const std::string& sql, DB::Schema::Ptr& schema,
              std::string& message)
              : Reader(sql, message), schema(schema) {
  }

  const int parse(Func func) {
    bool token_cmd = false;
    bool token_typ = false;
    bool bracket = false;

    while(remain && !err) {
 
      if(found_space())
        continue;

      if(!token_cmd && 
         (func == CREATE && (found_token("add", 3)    || found_token("create", 6))) || 
         (func == MODIFY && (found_token("modify", 6) || found_token("update", 6))) || 
         (func == DELETE && (found_token("delete", 6) || found_token("remove", 6)))
        ) {   
        token_cmd = true;
        continue;
      } 
      if(!token_cmd) {
        expect_token("CREATE|MODIFY|DELETE", 20, token_cmd);
        break;
      }
      if(!token_typ && (found_token("column", 6) || found_token("schema", 6))) {   
        token_typ = true;
        continue;
      }
      if(!token_typ) {
        expect_token("column", 7, token_typ);
        break;
      }

      if(!bracket) {
        expect_token("(", 1 , bracket);
        continue;
      }

      read_schema_options(func);
      if(err)
        break;

      expect_token(")", 1 , bracket);
      break;
    }
    return err;
  }

  ~ColumnSchema() {}


  void read_schema_options(Func func) {

    bool any = true;
    bool was_set = false;

    int64_t cid = DB::Schema::NO_CID;
	  std::string   col_name;
    // defaults
    uint32_t col_type=(uint8_t)Types::Column::PLAIN;
    uint32_t cell_versions=1;
    uint32_t cell_ttl=0;
    uint8_t blk_replication=0;
    uint32_t blk_encoding=(uint8_t)Types::Encoding::DEFAULT;
    uint32_t blk_size=0;
    uint32_t blk_cells=0;
    uint32_t cs_size=0;
    uint8_t cs_max=0;
    uint8_t compact_percent=0;
    int64_t revision=0;

    const char* stop = ", ";

    while(any && remain && !err) {
      if(found_space())
        continue;    

      if(any = found_token("cid", 3)) {
        expect_eq();
        if(err)
          return;
        read_int64_t(cid, was_set);
        continue;
      }
      if(any = found_token("name", 4)) {
        expect_eq();
        if(err)
          return;
        read(col_name, stop);
        continue;
      }
      if(any = found_token("type", 4)) {
        expect_eq();
        if(err)
          return;
        read_uint32_t(col_type, was_set);
        continue;
      }
      if(any = found_token("revision", 8)) {
        expect_eq();
        if(err)
          return;
        read_int64_t(revision, was_set);
        continue;
      }
      break;
    }

    if(col_name.empty() && func == CREATE ) {
      error_msg(Error::COLUMN_SCHEMA_NAME_EMPTY, "create action require column name");
      return;
    }

    if(cid == DB::Schema::NO_CID)
      schema = DB::Schema::make(
        col_name, (Types::Column)col_type,
        cell_versions, cell_ttl,
        blk_replication, (Types::Encoding)blk_encoding, blk_size, blk_cells,
        cs_size, cs_max, compact_percent, 
        revision
      );
    else
      schema = DB::Schema::make(
        cid,
        col_name, (Types::Column)col_type,
        cell_versions, cell_ttl,
        blk_replication, (Types::Encoding)blk_encoding, blk_size, blk_cells,
        cs_size, cs_max, compact_percent, 
        revision
      );
  }

  private:
    DB::Schema::Ptr& schema;
};


/*
use options:
add|create|modify|delete (schema definitions);
*/

}}} // SWC::client:SQL namespace

#endif //swcdb_client_sql_ColumnSchema_h