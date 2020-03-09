/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#ifndef swc_manager_db_Range_h
#define swc_manager_db_Range_h

#include <random>

#include "swcdb/db/Columns/RangeBase.h"



namespace SWC { namespace Manager {


class Range : public DB::RangeBase {
  public:

  typedef std::shared_ptr<Range> Ptr;
  
  enum State {
    NOTSET,
    DELETED,
    ASSIGNED,
    CREATED,
    QUEUED
  };


  Range(const DB::ColumnCfg* cfg, const int64_t rid)
        : RangeBase(cfg, rid), 
          m_state(State::NOTSET), rgr_id(0), m_last_rgr(nullptr) { 
  }

  void init(int &err){

  }

  inline Ptr shared() {
    return std::dynamic_pointer_cast<Range>(shared_from_this());
  }

  inline static Ptr shared(const DB::RangeBase::Ptr& other){
    return std::dynamic_pointer_cast<Range>(other);
  }

  virtual ~Range(){}
  
  bool deleted(){
    std::shared_lock lock(m_mutex);
    return m_state == State::DELETED;
  }

  bool assigned(){
    std::shared_lock lock(m_mutex);
    return m_state == State::ASSIGNED;
  }

  bool queued(){
    std::shared_lock lock(m_mutex);
    return m_state == State::QUEUED;
  }

  bool need_assign(){
    std::shared_lock lock(m_mutex);
    return m_state == State::NOTSET;
  }

  void set_state(State new_state, uint64_t new_rgr_id){
    std::scoped_lock lock(m_mutex);
    m_state = new_state;
    rgr_id = new_rgr_id;
  }
  
  void set_deleted(){
    std::scoped_lock lock(m_mutex);
    m_state = State::DELETED;
  }

  uint64_t get_rgr_id(){
    std::shared_lock lock(m_mutex);
    return rgr_id;
  }

  void set_rgr_id(uint64_t new_rgr_id){
    std::scoped_lock lock(m_mutex);
    rgr_id = new_rgr_id;
  }

  Files::RgrData::Ptr get_last_rgr(int &err){
    std::scoped_lock lock(m_mutex);
    if(m_last_rgr == nullptr)
      m_last_rgr = DB::RangeBase::get_last_rgr(err);
    return m_last_rgr;
  }
  
  void clear_last_rgr(){
    std::scoped_lock lock(m_mutex);
    m_last_rgr = nullptr;
  }

  const std::string to_string() {
    std::shared_lock lock(m_mutex);
    std::string s("[");
    s.append(DB::RangeBase::to_string());
    s.append(", state=");
    s.append(std::to_string(m_state));
    s.append(", rgr=");
    s.append(std::to_string(rgr_id));
    s.append("]");
    return s;
  }

  private:

  uint64_t              rgr_id;
  State                 m_state;
  Files::RgrData::Ptr   m_last_rgr;

};



}}
#endif // swc_manager_db_Range_h