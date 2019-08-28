/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */
 
#include "swcdb/lib/core/config/Settings.h"
#include "swcdb/lib/core/comm/Settings.h"

#include "swcdb/lib/client/Clients.h"
#include "swcdb/lib/client/AppContext.h"
#include "swcdb/lib/db/Protocol/req/Echo.h"



namespace SWC{ namespace Config {

void Settings::init_app_options(){
  init_comm_options();
  file_desc().add_options()
    ("requests", i32(1), "number of requests") 
    ("batch", i32(1), "batch size of each request")
    ("threads", i32(1), "number of threads x (requests x batch)")
    ("threads_conn", i32(1), "threads a connection") 
  ;
}
void Settings::init_post_cmd_args(){ }
}}

using namespace SWC;

class Stat {
  public:
  Stat(): m_count(0), m_avg(0), m_max(0),m_min(-1) {}
  virtual ~Stat(){}

  void add(uint64_t v){
    std::lock_guard<std::mutex> lock(m_mutex);
    m_avg *= m_count;
    m_avg += v;
    m_avg /= ++m_count;
    if(v > m_max)
      m_max = v;
    if(v < m_min)
      m_min = v;
  }

  uint64_t avg(){
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_avg;
  }
  uint64_t max(){
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_max;
  }
  uint64_t min(){
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_min;
  }

  uint64_t count(){
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_count;
  }

  private:
  std::mutex m_mutex;
  uint64_t m_count;
  uint64_t m_avg;
  uint64_t m_min;
  uint64_t m_max;
};

class Checker: public std::enable_shared_from_this<Checker>{
  public:
  Checker(int num_req, int batch_sz, int threads_conn)
          : num_req(num_req), batch_sz(batch_sz), threads_conn(threads_conn),
            total(num_req*batch_sz*threads_conn) {}

  void run(client::ClientConPtr conn, int req_n = 1){
    
    for(int i=1;i<=batch_sz;i++) {
      std::make_shared<Protocol::Req::Echo>(
        conn, 
        [req_n, conn, last=i==batch_sz, ptr=shared_from_this(), start_ts=std::chrono::system_clock::now()]
        (bool state){

          if(!state)
            ptr->failures++;

          ptr->stat->add(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::system_clock::now() - start_ts).count());   
      
          if(ptr->stat->count() % 100000 == 0) {
            std::cout << conn->endpoint_local_str() << ",";
            ptr->print_stats();
            std::cout << "\n";
          }
          if(!last)
            return;

          if(req_n < ptr->num_req){
            ptr->run(conn, req_n+1);

          } else if(ptr->stat->count() == ptr->total) {
            ptr->processed.store(true);
            ptr->cv.notify_all();
          }  

        }
      )->run();
    }
  }

  void get_conn(){
    Env::Clients::get()->mngr_service->get_connection(
      Env::Clients::get()->mngrs_groups->get_endpoints(1, 1), 
      [ptr=shared_from_this()](client::ClientConPtr conn){
        if(conn == nullptr || !conn->is_open()){
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          ptr->get_conn();
          return;
        }
        
        std::vector<std::thread*> threads;
        for(int t=1;t<=ptr->threads_conn;t++)
          threads.push_back(
            new std::thread(
              [conn, ptr](){ptr->run(conn);}
          ));
        for(auto& t : threads)t->join();
      },
      std::chrono::milliseconds(1000), 
      1
    );
  }

  void print_stats(){
    std::cout << " avg=" << stat->avg()
              << " min=" << stat->min()
              << " max=" << stat->max()
              << " count=" << stat->count()
              << " failures=" << failures;
  }

  void run(int num_threads){
    total*=num_threads;

    auto start_ts = std::chrono::system_clock::now();

    std::vector<std::thread*> threads;
    for(int t=1;t<=num_threads;t++)
      threads.push_back(
        new std::thread([ptr=shared_from_this()](){ptr->get_conn();}));

    for(auto& t : threads)t->join();
    
    std::unique_lock<std::mutex> lock_wait(lock);
    cv.wait(lock_wait, [stop=&processed]{return stop->load();});

    auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::system_clock::now() - start_ts).count();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "# FINISHED,"
              << " threads=" << num_threads
              << " threads_conn=" << threads_conn
              << " requests=" << num_req
              << " batch=" << batch_sz
              << " took=" << took
              << " median=" << took/stat->count();
    print_stats();
    std::cout << "\n";
  }

  int total;
  std::atomic<bool> processed=false;
  std::atomic<int> failures=0;
  int num_req;
  int batch_sz;
  int threads_conn;
  std::mutex lock;
  std::condition_variable cv;
  
  std::shared_ptr<Stat> stat = std::make_shared<Stat>();
};


int main(int argc, char** argv) {
  Env::Config::init(argc, argv);
  
  Env::Clients::init(std::make_shared<client::Clients>(
    nullptr,
    std::make_shared<client::AppContext>()
  ));


  std::make_shared<Checker>(
    Env::Config::settings()->get<int32_t>("requests"), 
    Env::Config::settings()->get<int32_t>("batch"),
    Env::Config::settings()->get<int32_t>("threads_conn")
  )->run(
    Env::Config::settings()->get<int32_t>("threads")
  );

  Env::IoCtx::io()->stop();

  return 0;
}