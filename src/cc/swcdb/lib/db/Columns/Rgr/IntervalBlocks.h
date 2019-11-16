/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swcdb_lib_db_Columns_Rgr_IntervalBlocks_h
#define swcdb_lib_db_Columns_Rgr_IntervalBlocks_h

#include "swcdb/lib/db/Cells/ReqScan.h"
#include "swcdb/lib/db/Files/CellStoreReaders.h"
#include "swcdb/lib/db/Files/CommitLog.h"

namespace SWC { namespace server { namespace Rgr {

class IntervalBlocks {
  public:
  typedef IntervalBlocks* Ptr;


  DB::RangeBase::Ptr                range;
  Files::CommitLog::Fragments::Ptr  commitlog;
  Files::CellStore::Readers::Ptr    cellstores;


  class Block : public DB::Cells::Block {
    public:
    typedef Block* Ptr;

    enum State {
      NONE,
      LOADING,
      LOADED,
      REMOVED
    };

    IntervalBlocks::Ptr      blocks;
    
    inline static Ptr make(const DB::Cells::Interval& interval, 
                           const DB::Schema::Ptr s, 
                           IntervalBlocks::Ptr blocks, 
                           State state=State::NONE) {
      return new Block(interval, s, blocks, state);
    }

    Block(const DB::Cells::Interval& interval, const DB::Schema::Ptr s, 
          IntervalBlocks::Ptr blocks, State state=State::NONE)
          : DB::Cells::Block(interval, s), blocks(blocks), m_state(state), 
            m_processing(0) {
    }

    void splitter() override {
      blocks->_split(ptr(), false);
    }
    
    Ptr ptr() {
      return this;
    }

    virtual ~Block() {
      //std::cout << " IntervalBlocks::~Block\n";
      wait_processing();
    }

    bool loaded() {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_state == State::LOADED;
    }

    bool add_logged(const DB::Cells::Cell& cell) { 
      std::lock_guard<std::mutex> lock(m_mutex);

      if(!m_interval.is_in_end(cell.key))
        return false;

      m_cells.add(cell);

      if(cell.on_fraction)
        // return added for fraction under current end
        return m_interval.key_end.compare(
          cell.key, cell.on_fraction) != Condition::GT;
      
      if(!m_interval.is_in_begin(cell.key)) {
        m_interval.key_begin.copy(cell.key); 
        m_interval.expand(cell.timestamp);
      }
      return true;
    }
    
    bool scan(DB::Cells::ReqScan::Ptr req) {
      //std::cout << " Block::scan "<< to_string()<<" \n";
      bool loaded;
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(m_state == State::REMOVED)
          return false;

        m_processing++;
        if(!(loaded = m_state == State::LOADED)) {
          m_queue.push(new Callback(req, ptr()));
          if(m_state != State::NONE)
            return true;
          m_state = State::LOADING;
        }
      }

      if(loaded) {
        _scan(req);
        return false;
      }

      blocks->cellstores->load_cells(ptr());
      return true;
    }

    void loaded_cellstores(int err) override {
      if(err) {
        run_queue(err);
        return;
      }
      blocks->commitlog->load_cells(ptr());
    }

    void loaded_logs(int err) override {
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = State::LOADED;
      }
      run_queue(err);
    }

    void run_queue(int& err) {
      Callback* cb;
    
      for(;;) {
        {
          std::lock_guard<std::mutex> lock(m_mutex);
          if(m_queue.empty())
            return;
          cb = m_queue.front();
        }

        cb->call(err);
        delete cb;

        {
          std::lock_guard<std::mutex> lock(m_mutex);
          m_queue.pop();
        }
      }
    }

    Ptr split(size_t keep=100000, bool loaded=true) {
      std::lock_guard<std::mutex> lock(m_mutex);
      return _split(keep, loaded);
    }
    
    Ptr _split(size_t keep=100000, bool loaded=true) {
      Ptr blk = Block::make(
        DB::Cells::Interval(), 
        Env::Schemas::get()->get(blocks->range->cid), 
        blocks,
        loaded ? State::LOADED : State::NONE
      );

      m_cells.move_from_key_offset(keep, blk->m_cells);
      
      bool any_begin = m_interval.key_begin.empty();
      bool any_end = m_interval.key_end.empty();
      m_interval.free();

      m_cells.expand(m_interval);
      blk->m_cells.expand(blk->m_interval);

      if(any_begin)
        m_interval.key_begin.free();
      if(any_end)
        blk->m_interval.key_end.free();
      
      if(!loaded)
        blk->m_cells.free();

      return blk;
    }

    void expand_next_and_release(DB::Cell::Key& key_begin) {
      std::lock_guard<std::mutex> lock(m_mutex);

      m_state = State::REMOVED;
      key_begin.copy(m_interval.key_begin);
      m_cells.free();
      m_interval.free();
    }

    void merge_and_release(Ptr blk) {
      std::lock_guard<std::mutex> lock(m_mutex);

      m_state = State::NONE;
      blk->expand_next_and_release(m_interval.key_begin);
      m_cells.free();
    }

    const size_t release() {
      size_t released = 0;
      std::lock_guard<std::mutex> lock(m_mutex);

      if(m_processing || m_state != State::LOADED)
        return released;
      m_state = State::NONE;

      //std::cout << "IntervalBlocks::Block::release\n"; 
      released += m_cells.size;
      m_cells.free();
      return released;
    }

    size_t processing() {
      std::lock_guard<std::mutex> lock(m_mutex); 
      return m_processing;
    }

    void wait_processing() {
      while(processing() > 0) {
        //std::cout << "wait_processing: " << to_string() << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }

    const std::string to_string() {
      std::lock_guard<std::mutex> lock(m_mutex);

      std::string s("Block(state=");
      s.append(std::to_string((uint8_t)m_state));
      
      s.append(m_interval.to_string());
      s.append(" ");
      s.append(m_cells.to_string());

      s.append(" queue=");
      s.append(std::to_string(m_queue.size()));

      s.append(" processing=");
      s.append(std::to_string(m_processing));
      s.append(")");
      return s;
    }

    private:

    void _scan(DB::Cells::ReqScan::Ptr req) {

      if(req->type != DB::Cells::ReqScan::Type::BLK_PRELOAD) {
        std::lock_guard<std::mutex> lock(m_mutex);

        size_t skips = 0; // Ranger::Stats
        m_cells.scan(
          *(req->spec).get(), 
          req->cells, 
          req->offset,
          [req]() { return req->reached_limits(); },
          skips, 
          req->has_selector 
          ? [req](const DB::Cells::Cell& cell) 
                { return req->selector(cell); }
          : (DB::Cells::Mutable::Selector_t)0 
        );  
        
      }

      //if(req->cells->size)
      //  req->cells->get(
      //    -1, req->spec->offset_key, req->spec->offset_rev);
      
      processing_decrement();
    }

    void processing_decrement() {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_processing--;
    }
    
    struct Callback {
      public:
      DB::Cells::ReqScan::Ptr req;
      Block::Ptr              ptr;

      Callback(DB::Cells::ReqScan::Ptr req, Block::Ptr ptr) 
               : req(req), ptr(ptr) { }

      virtual ~Callback() { }

      void call(int err) {
        if(err) {
          ptr->processing_decrement();
          req->response(err);
          return;
        }

        ptr->_scan(req); 
        
        if(req->type == DB::Cells::ReqScan::Type::BLK_PRELOAD) 
          return; 

        if(req->reached_limits()) {
          req->response(err);
        } else {
          ptr->blocks->scan(req, ptr); 
        }
      }
    };
    
    State                  m_state;
    size_t                 m_processing;
    std::queue<Callback*>  m_queue;

  }; // class Block


  // scan >> blk match >> load-cs + load+logs 

  IntervalBlocks(): commitlog(nullptr), cellstores(nullptr) {
  }
  
  void init(DB::RangeBase::Ptr for_range) {
    range       = for_range;
    commitlog   = Files::CommitLog::Fragments::make(range);
    cellstores  = Files::CellStore::Readers::make(range);
  }

  Ptr ptr() {
    return this;
  }

  virtual ~IntervalBlocks(){
    //std::cout << " ~IntervalBlocks\n";
    free();
  }
  
  void load(int& err) {
    commitlog->load(err);
  }

  void unload() {
    stop(Error::RS_NOT_READY);

    std::lock_guard<std::mutex> lock(m_mutex);

    if(commitlog != nullptr) 
      commitlog->commit_new_fragment(true);

    free();
  }
  
  void remove(int& err) {
    stop(Error::COLUMN_MARKED_REMOVED);

    std::lock_guard<std::mutex> lock(m_mutex);

    if(commitlog != nullptr) 
      commitlog->remove(err);
    if(cellstores != nullptr)
      cellstores->remove(err);  

    free();
  }

  void stop(int err) {
    std::vector<Block::Ptr>::iterator it;
    bool started=false;
    for(;;){
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(!started) {
          it = m_blocks.begin();
          started = true;
        } else 
          it++;
        if(it == m_blocks.end())
          break;
      }
      (*it)->run_queue(err);
    }
    wait_processing();
  }
  
  void add_logged(const DB::Cells::Cell& cell) {
    commitlog->add(cell);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    for(size_t idx = 0; idx<m_blocks.size(); idx++) {
      if(!m_blocks[idx]->loaded())
        continue;
      split(idx);

      if(m_blocks[idx]->add_logged(cell))
        break;
    }
    
  }

  void scan(DB::Cells::ReqScan::Ptr req, Block::Ptr blk_ptr = nullptr) {
    //std::cout << "IntervalBlocks::scan " <<  to_string() 
    //          << " " << req->to_string() << "\n";

    int err = Error::OK;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if(cellstores->empty())
        err = Error::RS_NOT_LOADED_RANGE;

      else if(m_blocks.empty()) 
        init_blocks(err);
    }
    if(err) {
      req->response(err);
      return;
    }

    
    Block::Ptr blk;
    bool flw;
    Block::Ptr nxt_blk;
    for(;;) {
      {
        blk = nullptr;
        flw = false;
        nxt_blk = nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        // it = narrower on req->spec->offset_key

        for(auto it=m_blocks.begin(); it< m_blocks.end(); it++) {
          auto eval = *it;
          if(!flw && blk_ptr != nullptr) {
            if(blk_ptr == eval)
              flw = true;
            continue;
          }
          if(eval->is_next(req->spec)) {
            blk = eval;
            blk_ptr = eval;

            if((!req->spec->flags.limit || req->spec->flags.limit > 1) 
                && ++it != m_blocks.end() && !(*it)->loaded())
              nxt_blk = *it;
            break;
          }/* else {
            std::cout << " scan eval-blk-mismatch "
                      << "\n block " << eval->to_string()
                      << "\n specs " << req->spec->to_string()
                      << "\n";
          }*/
        }
        if(blk == nullptr)  
          break;
      }

      if(nxt_blk != nullptr
        && !Env::Resources.need_ram(commitlog->cfg_blk_sz->get() * 10) 
        && nxt_blk->includes(req->spec)) {
        //std::cout << " BLK_PRELOAD " << nxt_blk->to_string() << "\n";
        asio::post(*Env::IoCtx::io()->ptr(), 
          [nxt_blk](){ 
            nxt_blk->scan(
              std::make_shared<DB::Cells::ReqScan>(
                DB::Cells::ReqScan::Type::BLK_PRELOAD)
            );
          }
        );
      }

      if(Env::Resources.need_ram(commitlog->cfg_blk_sz->get())) {
        asio::post(*Env::IoCtx::io()->ptr(), 
          [blk, ptr=ptr()](){
            ptr->release_prior(blk); // release_and_merge(blk);
          }
        );
      }

      if(blk->scan(req))
        return;
      
      if(req->reached_limits())
        break;
    }

    req->response(err);
  }

  const size_t cells_count() {
    size_t sz = 0;
    std::lock_guard<std::mutex> lock(m_mutex);
    for(auto& blk : m_blocks) 
      sz += blk->size();
    return sz;
  }

  const size_t size() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blocks.size();
  }

  const size_t size_bytes() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return _size_bytes();
  }

  const size_t size_bytes_total(bool only_loaded=false) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return _size_bytes(only_loaded) 
          + (cellstores ? cellstores->size_bytes(only_loaded) : 0)
          + (commitlog  ? commitlog->size_bytes(only_loaded)  : 0);
  }
  
  void split() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for(size_t idx = 0; idx<m_blocks.size(); idx++) {
      if(m_blocks[idx]->loaded())
        split(idx);
    }
  }

  void _split(Block::Ptr ptr, bool loaded=true) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for(size_t idx = 0; idx<m_blocks.size(); idx++) {
      if(ptr == m_blocks[idx]) {
        _split(idx, loaded);
        return;
      }
    }
  }

  void release_prior(Block::Ptr ptr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for(size_t idx = 0; idx<m_blocks.size(); idx++) {
      if(ptr == m_blocks[idx]) {
        if(idx < 1)
          return;
        auto prior = m_blocks[idx-1];
        if(prior->processing())
          return;
        prior->release();
      }
    }
  }

  void release_and_merge(Block::Ptr ptr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    //bool state = false;
    for(size_t idx = 0; idx<m_blocks.size(); idx++) {
      if(ptr == m_blocks[idx]) {
        if(idx < 2)
          return;
        auto ptr1 = m_blocks[idx-2];
        if(ptr1->processing())
          return;
        auto ptr2 = m_blocks[idx-1];
        if(ptr2->processing())
          return;
        ptr2->merge_and_release(ptr1);
        delete ptr1;
        m_blocks.erase(m_blocks.begin()+idx-2);
        --idx;
        //state = true;
      }
    }
    /*
    if(state) {
      for(auto blk : m_blocks)
        std::cout << " " << blk->to_string() << "\n"; 
    }
    */
  }

  const size_t release(size_t bytes=0) {
    //std::cout << "IntervalBlocks::release=" << bytes << "\n"; 
    
    size_t released = cellstores->release(bytes);
    if(!bytes || released < bytes) {

      released += commitlog->release(bytes ? bytes-released : bytes);
      if(!bytes || released < bytes) {

        std::lock_guard<std::mutex> lock(m_mutex);
        for(auto blk : m_blocks) {
          released += blk->release();
          if(bytes && released >= bytes)
            break;
        }
      }
    }
    if(!bytes)
      clear();
    //else if(m_blocks.size() > 1000)
    // merge in pairs down to 1000 blks

    //std::cout << "IntervalBlocks::release=" << bytes << " released=" << released << "\n"; 
    return released;
  }

  const size_t processing() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return _processing();
  }

  void wait_processing() {
    while(processing() > 0) {
      //std::cout << "wait_processing: " << to_string() << "\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  const std::string to_string(){
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string s("IntervalBlocks(count=");
    s.append(std::to_string(m_blocks.size()));

    s.append(" blocks=[");
    for(auto blk : m_blocks) {
        s.append(blk->to_string());
       s.append(", ");
    }
    s.append("] ");

    if(commitlog != nullptr)
      s.append(commitlog->to_string());

    s.append(" ");
    if(cellstores != nullptr)
      s.append(cellstores->to_string());

    s.append(" processing=");
    s.append(std::to_string(_processing()));

    s.append(" bytes=");
    s.append(std::to_string(_size_bytes()));

    s.append(")");
    return s;
  } 

  private:
  
  const size_t _processing() {
    size_t sz = 0;
    for(auto blk : m_blocks) 
      sz += blk->processing();
    return sz;
  }

  void clear() {
    for(auto & blk : m_blocks) 
      delete blk;
    m_blocks.clear();
  }

  void free() {
    if(cellstores != nullptr) {
      delete cellstores;
      cellstores = nullptr;
    }
    if(commitlog != nullptr) {
      delete commitlog;
      commitlog = nullptr;
    }
    range = nullptr;
    
    clear();
  }

  void init_blocks(int& err) {
    std::vector<Files::CellStore::Block::Read::Ptr> blocks;
    cellstores->get_blocks(err, blocks);
    if(err) {
      clear();
      return;
    }

    DB::Schema::Ptr schema = Env::Schemas::get()->get(range->cid);
    for(auto& blk : blocks)
      m_blocks.push_back(Block::make(blk->interval, schema, ptr()));
  
    if(range->is_any_begin())
      m_blocks.front()->free_key_begin();
    if(range->is_any_end()) 
      m_blocks.back()->free_key_end();
  }

  void split(size_t idx, bool loaded=true) {
    while(m_blocks[idx]->size() > 200000) {
      auto blk = m_blocks[idx++]->split(100000, loaded);
      m_blocks.insert(m_blocks.begin()+idx, blk);
    }
  }

  void _split(size_t idx, bool loaded=true) {
    while(m_blocks[idx]->_size() >= 200000) {
      auto blk = m_blocks[idx++]->_split(100000, loaded);
      m_blocks.insert(m_blocks.begin()+idx, blk);
    }
  }

  const size_t _size_bytes(bool only_loaded=false) {
    size_t sz = 0;
    for(auto& blk : m_blocks) 
      sz += blk->size_bytes();
    return sz;
  }

  std::mutex                   m_mutex;
  std::vector<Block::Ptr>      m_blocks;

};





}}}
#endif