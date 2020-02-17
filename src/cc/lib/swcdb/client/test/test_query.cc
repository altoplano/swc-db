/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */
 
#include "swcdb/core/config/Settings.h"
#include "swcdb/core/comm/Settings.h"

#include "swcdb/client/Clients.h"
#include "swcdb/client/AppContext.h"
#include "swcdb/db/Protocol/Mngr/req/ColumnMng.h"
#include "swcdb/db/Protocol/Mngr/req/ColumnGet.h"
#include "swcdb/db/Protocol/Common/req/Query.h"


#include "swcdb/db/Stats/Stat.h"


namespace SWC{ namespace Config {

void Settings::init_app_options(){
  init_comm_options();
  init_client_options();
  // file_desc.add_options();
}
void Settings::init_post_cmd_args(){ }
}}

namespace Cells = SWC::DB::Cells;
namespace Query = SWC::Protocol::Common::Req::Query;

bool quite = true;
std::atomic<bool> finished = false;
int fraction_start = 0;
int fraction_finish = 26;
int num_fractions = fraction_finish-fraction_start;


void expect_empty_column(int64_t cid) {
    // Req::Query::Select
  Query::Select::Ptr select_req = std::make_shared<Query::Select>(
    [](Query::Select::Result::Ptr result)
    {
      if(quite)return;
      /*
      std::cout << "CB completion=" << result->completion.load() << "\n";
      for(auto col : result->columns) {
        std::cout << " cid=" << col.first 
                  << ": sz=" << col.second->cells.size() << "\n";
        int num=0;
        for(auto cell : col.second->cells)
          std::cout << "  " << ++num << ":" << cell->to_string() << "\n";  
      }
      */
    }
  );
  
  auto spec = SWC::DB::Specs::Interval::make_ptr();
  spec->flags.offset=0;
  spec->flags.limit=1;
  // spec->flags.options |= flags.DELETES;
  select_req->specs.columns = {SWC::DB::Specs::Column::make_ptr(cid, {spec})};
  select_req->scan();
  select_req->wait();

  size_t sz = select_req->result->get_size(cid);
  if(sz > 0) {
    SWC::DB::Cells::Vector holder;
    select_req->result->get_cells(cid, holder);
    std::cerr << "BAD, column not empty: \n" 
              << " " << spec->to_string() << "\n"
              << " expected_value=0\n"
              << "  result_value=" << sz << "\n"
              << " " << holder.cells[0]->to_string() << "\n";
    exit(1);
  }
}

void select_all(int64_t cid, int64_t expected_sz = 0, int64_t offset=0) {
  Query::Select::Ptr select_req = std::make_shared<Query::Select>();
  
  auto took =  SWC::Time::now_ns();
  auto spec = SWC::DB::Specs::Interval::make_ptr();
  spec->flags.offset=offset;
  spec->flags.limit=0;
  select_req->specs.columns = {SWC::DB::Specs::Column::make_ptr(cid, {spec})};
  select_req->scan();
  select_req->wait();
  
  took = SWC::Time::now_ns() - took;

  auto sz = select_req->result->get_size(cid);
  SWC::DB::Cells::Vector holder;
  select_req->result->get_cells(cid, holder);
    
  std::cout << "SELECT-ALL offset=" << offset 
            << " expected=" << expected_sz 
            << " cells=" << sz 
            << " TOOK="  << took 
            << " avg="<< (sz?took/sz:0) << "\n";
  
  if(sz != expected_sz) {
    size_t num=0;
    for(auto cell : holder.cells) {
      if( ++num == 1 || num == sz)
        std::cout << "  " << num << ":" << cell->to_string() << "\n";  
    }
    std::cerr << "\n BAD, on offset, select cells count: \n" 
              << " err=" << select_req->result->err 
              << " " << spec->to_string() << "\n"
              << " expected_value=" << expected_sz << "\n"
              << "   result_value=" << sz << "\n";
    std::cout << "SELECT-ALL offset=" << offset 
              << " expected=" << expected_sz 
              << " cells=" << sz 
              << " TOOK="  << took 
              << " avg="<< (sz?took/sz:0) << "\n";
    exit(1);
  }
}

std::string apply_value(int f, int b, int i) {
  /*
  
  std::string zfill;
  for(int n=0;n<16384;n++)
    zfill.append("V");
  */
  std::string value = 
    "V_OF:"+std::to_string(f)+":"+std::to_string(b)+":"+std::to_string(i);
  value += "(";
  for(uint32_t chr=0; chr<=255; chr++) {
    value += (char)chr;
  }
  value += ")END";
  return value;
}

void test_1(const std::string& col_name) {
  int num_cells = 10000; // test require at least 12
  int batches = 100;
  int64_t took;

  // Req::Query::Update
  Query::Update::Ptr update_req = std::make_shared<Query::Update>(
    [](Query::Update::Result::Ptr result)
    {
      std::cout << "CB completion=" << result->completion.load() 
                << "err=" << result->err << "\n";
    }
  );
  
  int err = SWC::Error::OK;
  auto schema = SWC::Env::Clients::get()->schemas->get(err, col_name);
    if(err) {
    std::cerr << "err=" << err << "(" << SWC::Error::get_text(err) << ")\n";
    exit(1);
  }
  std::cout << schema->to_string() << "\n";
  update_req->columns->create(schema);
  

  Cells::Cell cell;
  size_t added_count = 0;
  for(int b=0; b<batches; b++) {
    took =  SWC::Time::now_ns();
    for(int i=0;i<num_cells;i++) {
    for(int f=97+fraction_start;f<=96+fraction_finish;f++) {

      std::string cell_number(std::to_string(b)+":"+std::to_string(i));
      cell.flag = Cells::INSERT;
      cell.set_time_order_desc(true);

      cell.key.free();
      for(uint8_t chr=97; chr<=f;chr++)
        cell.key.add(((char)chr)+cell_number);
      
      std::string value = apply_value(f, b, i);
      cell.set_value(value);

      update_req->columns->add(schema->cid, cell);
      added_count++;
    }}

    size_t bytes = update_req->columns->size_bytes();
    std::cout << update_req->columns->to_string() << "\n";
  

    update_req->timeout_commit = 10*num_fractions*num_cells;
    update_req->commit();
    update_req->wait();
    took = SWC::Time::now_ns() - took;
    std::cout << "UPDATE-INSERT-TOOK=" << took 
              << " cells=" << num_fractions*num_cells 
              << " bytes=" << bytes 
              << " avg="<< took/(num_fractions*num_cells) << "\n";

    select_all(
      schema->cid, num_fractions*num_cells, num_fractions*b*num_cells);
  }
  

  std::cout << "INSERT added_count="<<added_count<<"\n";
  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  //exit(1);

  // Req::Query::Select
  Query::Select::Ptr select_req = std::make_shared<Query::Select>(
    [](Query::Select::Result::Ptr result)
    {
      if(quite)return;
      /*
      std::cout << "CB completion=" << result->completion.load() << "\n";
      for(auto col : result->columns) {
        std::cout << " cid=" << col.first 
                  << ": sz=" << col.second->cells.size() << "\n";
        int num=0;
        for(auto cell : col.second->cells)
          std::cout << "  " << ++num << ":" << cell->to_string() << "\n";  
      }
      */
    }
  );

  took =  SWC::Time::now_ns();
  auto spec = SWC::DB::Specs::Interval::make_ptr();
  spec->flags.offset=num_fractions*batches*num_cells-1;
  spec->flags.limit=1;
  select_req->specs.columns = {
    SWC::DB::Specs::Column::make_ptr(schema->cid, {spec})};
  select_req->scan();
  select_req->wait();

  size_t sz = select_req->result->get_size(schema->cid);

  if(sz != spec->flags.limit) {
    std::cerr << "BAD, on offset, select cells count: \n" 
              << " " << spec->to_string() << "\n"
              << " expected_value=" << spec->flags.limit << "\n"
              << "   result_value=" << sz << "\n";
    exit(1);
  }
  SWC::DB::Cells::Vector holder;
  select_req->result->get_cells(schema->cid, holder);
  Cells::Cell* cell_res = holder.cells.front();
  
  std::string expected_value = apply_value(
    num_fractions+97, batches-1, num_cells-1);
  if(memcmp(cell_res->value, expected_value.data(), cell_res->vlen) != 0) {
    std::cerr << "BAD, selected cell's value doesn't match: \n" 
              << " expected_value=" << expected_value << "\n"
              << "   result_value=" 
              << std::string((const char*)cell_res->value, cell_res->vlen) 
              << "\n";
    exit(1);
  }
  took = SWC::Time::now_ns() - took;
  std::cout << "SELECT-TOOK=" << took  << "\n";

  for(int tmp=1;tmp <=3;tmp++) {
  select_req->result->remove(schema->cid);
  spec =  select_req->specs.columns[0]->intervals[0];
  spec->flags.offset=num_fractions*batches*num_cells-num_fractions;
  spec->flags.limit=num_fractions; //batches*num_cells-spec->flags.offset;

  took =  SWC::Time::now_ns();
  select_req->scan();
  select_req->wait();

  sz = select_req->result->get_size(schema->cid);
  holder.free();
  select_req->result->get_cells(schema->cid, holder);

  if(sz != spec->flags.limit) {
    std::cerr << "\n first: " << holder.cells.front()->to_string() << "\n";
    std::cerr <<   "  last: " << holder.cells.back()->to_string() << "\n";
    std::cerr << "\nBAD probe, select cells count: \n" 
              << " " << spec->to_string() << "\n"
              << " expected_value=" << spec->flags.limit << "\n"
              << "   result_value=" << sz << "\n";
    exit(1);
  }
  {
    Cells::Cell prev;
    int count = 0;
    for(auto c : holder.cells) {
      count++;
      if(prev.flag != Cells::NONE) {
        if(c->key.equal(prev.key)) {
          std::cerr << " BAD DUPLICATED CELL! \n";
          std::cerr << " current  " << c->to_string() << "\n";
          std::cerr << " previous " << prev.to_string() << "\n";
        }
      }
      prev.copy(*c);
      std::string expected_value = apply_value(
        97+count, batches-1, num_cells-spec->flags.limit-1+count);
      if(memcmp(c->value, expected_value.data(), c->vlen) != 0) {
        std::cerr << "BAD, selected cell's value doesn't match: \n" 
                  << " expected_value=" << expected_value << "\n"
                  << "   result_value=" 
                  << std::string((const char*)c->value, c->vlen) << "\n";
         exit(1);
      }
    }
  }

  took = SWC::Time::now_ns() - took;
  std::cout << "SELECT-TOOK=" << took  << " probe=" << tmp << "\n";
  }

  
  select_req->result->remove(schema->cid);
  spec =  select_req->specs.columns[0]->intervals[0];
  spec->free();
  spec->key_start.add("", SWC::Condition::NONE);
  std::string fraction(
    "b"+std::to_string(batches-1)+":"+std::to_string(num_cells-12));
  spec->key_start.add(fraction, SWC::Condition::EQ);
  spec->key_start.add("", SWC::Condition::NONE);
  spec->flags.offset=0;
  spec->flags.limit=1;
  took =  SWC::Time::now_ns();
  select_req->scan();
  select_req->wait();

  sz = select_req->result->get_size(schema->cid);
  holder.free();
  select_req->result->get_cells(schema->cid, holder);

  if(sz != spec->flags.limit) {
    std::cerr << "BAD, match key fraction, select cells count: \n" 
              << " expected_value=" << spec->flags.limit << "\n"
              << "   result_value=" << sz << "\n";
    exit(1);
  }
  if(holder.cells[0]->key.get(1).compare(fraction) != 0) {
    std::cerr << "BAD, select cell by key fraction: \n" 
              << "  expected_value=" << spec->key_start.get(1) << "\n"
              << " (1)result_value=" << holder.cells[0]->to_string() << "\n";
    exit(1);
  }
  took = SWC::Time::now_ns() - took;
  std::cout << "SELECT-TOOK=" << took  << "\n";
  

  for(int b=0;b<batches;b++) {
  took =  SWC::Time::now_ns();
  for(int i=0;i<num_cells;i++) {
  for(int f=97+fraction_start;f<=96+fraction_finish;f++) {

    cell.free();
    std::string cell_number(std::to_string(b)+":"+std::to_string(i));
    cell.flag = Cells::DELETE;
    cell.key.free();
    for(uint8_t chr=97; chr<=f;chr++)
      cell.key.add(((char)chr)+cell_number);
    update_req->columns->add(schema->cid, cell);
  }}

  
  size_t bytes = update_req->columns->size_bytes();
  std::cout << update_req->columns->to_string() << "\n";
  
  took =  SWC::Time::now_ns();
  update_req->timeout_commit = 10*num_fractions*num_cells;
  update_req->commit();
  update_req->wait();
  if(update_req->columns->size_bytes() != 0) {
    std::cerr << " ERROR, remain_bytes=" 
              << update_req->columns->size_bytes() << "\n";
    exit(1);
  }
  took = SWC::Time::now_ns() - took;
  std::cout << "UPDATE-DELETE-TOOK=" << took 
            << " cells=" << num_fractions*num_cells 
            << " bytes=" << bytes 
            << " remain_bytes=" << update_req->columns->size_bytes() 
            << " avg="<< took/(num_fractions*num_cells) << "\n";
  }

  std::cout << "\n";

  
  select_req->result->remove(schema->cid);
  spec =  select_req->specs.columns[0]->intervals[0];
  spec->free();
  spec->flags.offset=0;
  spec->flags.limit=0;
  took =  SWC::Time::now_ns();
  select_req->scan();
  select_req->wait();

  sz = select_req->result->get_size(schema->cid);
  if(sz != 0) {
    std::cerr << "BAD, select cells count: \n" 
              << " expected_value=0\n"
              << "   result_value=" << sz << "\n";
    exit(1);
  }
  took = SWC::Time::now_ns() - took;
  std::cout << "SELECT-TOOK=" << took  << "\n";
  
  std::cout << "\n";
}



int main(int argc, char** argv) {
  SWC::Env::Config::init(argc, argv);
  
  SWC::Env::Clients::init(
    std::make_shared<SWC::client::Clients>(
      nullptr,
      std::make_shared<SWC::client::AppContext>()
    )
  );
  
  // PLAIN one version
  auto schema = SWC::DB::Schema::make(
    0, 
    "col-test-1", 
    SWC::Types::Column::PLAIN, 
    1, // cell_versions
    0, // cell_ttl
    SWC::Types::Encoding::ZLIB,
    50000000,
    0, // blk-cells
    3, // cs_replication
    1000000000, // cs-size
    10, // cs-max
    0 // compact-%
  );

  /*
  int err;
          expect_empty_column(
            SWC::Env::Clients::get()->schemas->get(err, schema->col_name)->cid
          );
      exit(0);
  */

  // 1st DELETE & CREATE COLUMN
  
  // DELETE
  SWC::Protocol::Mngr::Req::ColumnMng::request(
    SWC::Protocol::Mngr::Req::ColumnMng::Func::DELETE,
    schema,
    [schema]
    (SWC::Protocol::Common::Req::ConnQueue::ReqBase::Ptr req_ptr, int err){
      if(err != SWC::Error::OK 
        && err != SWC::Error::COLUMN_SCHEMA_NAME_NOT_EXISTS) {
        req_ptr->request_again();
        return;
      }
       /**/
      // CREATE
      SWC::Protocol::Mngr::Req::ColumnMng::request(
        SWC::Protocol::Mngr::Req::ColumnMng::Func::CREATE,
        schema,
        [schema]
        (SWC::Protocol::Common::Req::ConnQueue::ReqBase::Ptr req_ptr, int err) {
          if(err != SWC::Error::OK 
            && err != SWC::Error::COLUMN_SCHEMA_NAME_EXISTS) {
            req_ptr->request_again();
            return;
          }
           /**/
          expect_empty_column(
            SWC::Env::Clients::get()->schemas->get(err, schema->col_name)->cid
          );

          for(int i=0;i<2;i++) {
            test_1(schema->col_name);
            std::cout << "test_1 chk=" << i << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(10000));
          }
          finished = true;
        },
        10000
      );
  /*
  */
    },
    10000
  );
  
  

  while(!finished)
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  SWC::Env::IoCtx::io()->stop();
  std::cout << " ### EXIT ###\n";
  
  exit(0);

  /*

  

  //std::cout << " ### running-cid=1 ###\n";
  //run_test(update_req, 1);
  //std::cout << " ### running-cid=2 ###\n";
  //run_test(update_req, 2);
  run_test(update_req, 11, 1, 100000, 1);
  run_test(select_req, 11, 1, 100, 1);
  exit(0);

  for(int check=1; check<=10; check++)
    run_test(update_req, 11, 2, 100000, check);

  for(int check=1; check<=10; check++)
    run_test(update_req, 11, 2, 100000, check, true);

  for(int check=1; check<=1000; check++)
    run_test(update_req, 11, 2, 1, check);

  for(int check=1; check<=1000; check++)
    run_test(update_req, 11, 2, 1, check, true);

  */
}
