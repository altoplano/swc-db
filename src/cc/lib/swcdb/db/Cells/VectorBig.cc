/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */


#include "swcdb/db/Cells/VectorBig.h"


namespace SWC { namespace DB { namespace Cells { 


VectorBig::Bucket* VectorBig::make_bucket(uint16_t reserve) {
  auto bucket = new Bucket;
  if(reserve)
    bucket->reserve(reserve);
  return bucket;
}

VectorBig::Ptr VectorBig::make(const uint32_t max_revs, const uint64_t ttl_ns, 
                               const Types::Column type) {
  return std::make_shared<VectorBig>(max_revs, ttl_ns, type);
}

VectorBig::VectorBig(const uint32_t max_revs, const uint64_t ttl_ns, 
                     const Types::Column type)
                    : type(type), max_revs(max_revs), ttl(ttl_ns),
                      buckets({make_bucket(0)}), _bytes(0), _size(0) {
}

VectorBig::VectorBig(VectorBig& other)
                    : buckets(other.buckets), 
                      _bytes(other.size_bytes()), _size(other.size()), 
                      type(other.type), max_revs(other.max_revs), 
                      ttl(other.ttl) {
  other.buckets.clear();
  other.free();
}

void VectorBig::take_sorted(VectorBig& other) {
  if(!other.empty()) {
    if(empty()) {
      delete *buckets.begin();
      buckets.clear();
    }
    _bytes += other.size_bytes();
    _size += other.size();
    buckets.insert(buckets.end(), other.buckets.begin(), other.buckets.end());
    other.buckets.clear();
    other.free();
  }
}

VectorBig::~VectorBig() {
  free();
}

void VectorBig::free() {
  for(auto bucket : buckets) {
    for(auto cell : *bucket)
      delete cell;
    delete bucket;
  }
  _bytes = 0;
  _size = 0;
  buckets.clear();
  buckets.push_back(make_bucket(0));
  buckets.shrink_to_fit();
}

void VectorBig::reset(const uint32_t revs, const uint64_t ttl_ns, 
                      const Types::Column typ) {
  free();
  configure(revs, ttl_ns, typ);
}

void VectorBig::configure(const uint32_t revs, const uint64_t ttl_ns, 
                          const Types::Column typ) {
  type = typ;
  max_revs = revs;
  ttl = ttl_ns;
}

VectorBig::ConstIterator VectorBig::ConstIt(size_t offset) const {
  return ConstIterator(&buckets, offset);
}

VectorBig::Iterator VectorBig::It(size_t offset) {
  return Iterator(&buckets, offset);
}

const size_t VectorBig::size() const {
  return _size;
}

const size_t VectorBig::size_bytes() const {
  return _bytes;
}

bool VectorBig::empty() const {
  return !_size;
  //return buckets.empty() || buckets.front()->empty();
}

Cell*& VectorBig::front() {
  return buckets.front()->front();
}

Cell*& VectorBig::back() {
  return buckets.back()->back();
}

Cell*& VectorBig::front() const {
  return buckets.front()->front();
}

Cell*& VectorBig::back() const {
  return buckets.back()->back();
}

Cell*& VectorBig::operator[](size_t idx) {
  return *It(idx).item;
}

const bool VectorBig::has_one_key() const {
  return front()->key.compare(back()->key) == Condition::EQ;
}


void VectorBig::add_sorted(const Cell& cell, bool no_value) {
  Cell* adding;
  _add(adding = new Cell(cell, no_value));
  _push_back(adding);
}

void VectorBig::add_sorted_no_cpy(Cell* cell) {
  _add(cell);
  _push_back(cell);
}

const size_t VectorBig::add_sorted(const uint8_t* ptr, size_t remain) {
  size_t count = 0;
  _bytes += remain;
  while(remain) {
    _push_back(new Cell(&ptr, &remain, true));
    ++count;
  }
  _size += count;
  return count;
}


void VectorBig::add_raw(const DynamicBuffer& cells) {
  Cell cell;
  const uint8_t* ptr = cells.base;
  size_t remain = cells.fill();
  while(remain) {
    cell.read(&ptr, &remain);
    add_raw(cell);
  }
}

void VectorBig::add_raw(const DynamicBuffer& cells, 
                        const DB::Cell::Key& upto_key,
                        const DB::Cell::Key& from_key) {
  Cell cell;
  const uint8_t* ptr = cells.base;
  size_t remain = cells.fill();
  while(remain) {
    cell.read(&ptr, &remain);
    if((!upto_key.empty() && upto_key.compare(cell.key) != Condition::GT) ||
       from_key.compare(cell.key) == Condition::GT)
      add_raw(cell);
  }
}

void VectorBig::add_raw(const Cell& e_cell) {
  if(e_cell.has_expired(ttl))
    return;

  size_t offset = _narrow(e_cell.key);

  if(e_cell.removal()) {
    _add_remove(e_cell, offset);

  } else {
    if(Types::is_counter(type))
      _add_counter(e_cell, offset);
    else
      _add_plain(e_cell, offset);
  }
}


Cell* VectorBig::takeout_begin(size_t idx) {
  return takeout(idx);
}

Cell* VectorBig::takeout_end(size_t idx) {
  return takeout(size() - idx);
}

Cell* VectorBig::takeout(size_t idx) {
  auto it = It(idx);
  Cell* cell;
  _remove(cell = *it.item);
  it.remove();
  return cell;
}


void VectorBig::write_and_free(DynamicBuffer& cells, uint32_t& cell_count,
                               Interval& intval, uint32_t threshold, 
                               uint32_t max_cells) {
  if(!_size)
    return;
    
  cells.ensure(_bytes < threshold? _bytes: threshold);
  Cell* first = nullptr;
  Cell* last = nullptr;
  Cell* cell;
  size_t count = 0;
  auto it_start = It();
  for(auto it = it_start; it && (!threshold || threshold > cells.fill()) && 
                                (!max_cells || max_cells > cell_count); ++it) {
    ++count;
    if((cell=*it.item)->has_expired(ttl))
      continue;

    cell->write(cells);
    intval.expand(cell->timestamp);
    cell->key.align(intval.aligned_min, intval.aligned_max);
    (first ? last : first) = cell;
    ++cell_count;
  }

  if(first) {
    intval.expand_begin(*first);
    intval.expand_end(*(last ? last : first));
  }
  
  if(_size == count)
    free();
  else
    _remove(it_start, count);
}

bool VectorBig::write_and_free(const DB::Cell::Key& key_start, 
                               const DB::Cell::Key& key_finish,
                               DynamicBuffer& cells, uint32_t threshold) {
  bool more = _size;
  if(!more)
    return more;

  cells.ensure(_bytes < threshold? _bytes: threshold);

  auto it = It(_narrow(key_start));
  size_t count = 0;
  Iterator it_start;
  for(Cell* cell; it && (!threshold || threshold > cells.fill()); ++it) {
    cell=*it.item;

    if(!key_start.empty() && 
        key_start.compare(cell->key, 0) == Condition::LT) 
      continue;
    if(!key_finish.empty() && 
        key_finish.compare(cell->key, 0) == Condition::GT) {
      more = false;
      break;
    }

    if(!count++)
      it_start = it;
  
    if(!cell->has_expired(ttl)) 
      cell->write(cells);
  }

  if(count) {
    if(count == _size) {
      free();
    } else {
      _remove(it_start, count);
      return more && it;
    }
  }
  return false;
}


const std::string VectorBig::to_string(bool with_cells) const {
  std::string s("Cells(size=");
  s.append(std::to_string(size()));
  s.append("/");
  s.append(std::to_string(_size));
  s.append(" bytes=");
  s.append(std::to_string(_bytes));
  s.append(" type=");
  s.append(Types::to_string(type));
  s.append(" max_revs=");
  s.append(std::to_string(max_revs));
  s.append(" ttl=");
  s.append(std::to_string(ttl));
  s.append(" buckets=");
  s.append(std::to_string(buckets.size()));
  
  if(with_cells) {
    s.append(" [\n");
    for(auto bucket : buckets) {
      s.append(" sz=");
      s.append(std::to_string(bucket->size()));
      s.append(" (\n");
      for(auto cell : *bucket) {
        s.append(cell->to_string(type));
        s.append("\n");
      }
     s.append(" )\n");
    }
    s.append("]");
  }
  
  s.append(")");
  return s;
}


void VectorBig::get(int32_t idx, DB::Cell::Key& key) const {
  key.copy((*ConstIt(idx < 0 ? size()+idx : idx).item)->key);
}
 
const bool VectorBig::get(const DB::Cell::Key& key, Condition::Comp comp, 
                          DB::Cell::Key& res) const {
  Condition::Comp chk;  
  for(auto it = ConstIt(_narrow(key)); it; ++it) {
    if((chk = key.compare((*it.item)->key, 0)) == Condition::GT 
      || (comp == Condition::GE && chk == Condition::EQ)){
      res.copy((*it.item)->key);
      return true;
    }
  }
  return false;
}

void VectorBig::write(DynamicBuffer& cells) const {
  cells.ensure(_bytes);
  for(auto it = ConstIt(); it; ++it) {
    if(!(*it.item)->has_expired(ttl))
      (*it.item)->write(cells);
  }
}


void VectorBig::scan(const Specs::Interval& specs, VectorBig& cells, 
                     size_t& cell_offset, 
                     const std::function<bool()>& reached_limits, 
                     size_t& skips, const Selector_t& selector) const {
  if(!_size)
    return;
  if(max_revs == 1) 
    scan_version_single(
      specs, cells, cell_offset, reached_limits, skips, selector);
  else
    scan_version_multi(
      specs, cells, cell_offset, reached_limits, skips, selector);
}

void VectorBig::scan_version_single(const Specs::Interval& specs, 
                                    VectorBig& cells, size_t& cell_offset, 
                                    const std::function<bool()>& reached_limits,
                                    size_t& skips, const Selector_t& selector) const {
  bool stop = false;
  bool only_deletes = specs.flags.is_only_deletes();
  bool only_keys = specs.flags.is_only_keys();

  size_t offset = specs.offset_key.empty()? 0 : _narrow(specs.offset_key);
                                             // ?specs.key_start
  Cell* cell;
  for(auto it = ConstIt(offset); !stop && it; ++it) {

    if(!(cell=*it.item)->has_expired(ttl) &&
       (only_deletes ? cell->flag != INSERT : cell->flag == INSERT) &&
       selector(*cell, stop)) {
      
      if(cell_offset) {
        --cell_offset;
        ++skips;  
        continue;
      }

      cells.add_sorted(*cell, only_keys);
      if(reached_limits())
        break;
    } else 
      ++skips;
  }
}

void VectorBig::scan_version_multi(const Specs::Interval& specs, 
                                   VectorBig& cells, size_t& cell_offset, 
                                   const std::function<bool()>& reached_limits,
                                   size_t& skips, 
                                   const Selector_t& selector) const {
  bool stop = false;
  bool only_deletes = specs.flags.is_only_deletes();
  bool only_keys = specs.flags.is_only_keys();
  
  bool chk_align;
  uint32_t rev;
  size_t offset;
  if((chk_align = !specs.offset_key.empty())) {
    rev = cells.max_revs;
    offset = _narrow(specs.offset_key);// ?specs.key_start
   } else {
    rev = 0;
    offset = 0;
  }  
  Cell* cell;
  for(auto it = ConstIt(offset); !stop && it; ++it) {
    cell = *it.item;

    if((only_deletes ? cell->flag == INSERT : cell->flag != INSERT) || 
       cell->has_expired(ttl)) {
      ++skips;
      continue;
    }

    if(chk_align) switch(specs.offset_key.compare(cell->key)) {
      case Condition::LT: {
        ++skips;
        continue;
      }
      case Condition::EQ: {
        if(!rev ||
           !specs.is_matching(cell->timestamp, cell->control & TS_DESC)) {
          if(rev)
            --rev;
          //if(cell_offset && selector(*cell, stop))
          //  --cell_offset;
          ++skips;
          continue;
        }
      }
      default:
        chk_align = false;
        break;
    }

    if(!selector(*cell, stop)) {
      ++skips;
      continue;
    }
    if(!cells.empty() && 
       cells.back()->key.compare(cell->key) == Condition::EQ) {
      if(!rev) {
        ++skips;
        continue;
      }
    } else {
      rev = cells.max_revs;
    }

    if(cell_offset) {
      --cell_offset;
      ++skips;
      continue;
    }

    cells.add_sorted(*cell, only_keys);
    if(reached_limits())
      break;
    --rev;
  }
}

void VectorBig::scan_test_use(const Specs::Interval& specs, 
                              DynamicBuffer& result,
                              size_t& count, size_t& skips) const {
  uint cell_offset = specs.flags.offset;
  bool only_deletes = specs.flags.is_only_deletes();

  Cell* cell;
  for(auto it = ConstIt(); it; ++it) {

    if(!(cell = *it.item)->has_expired(ttl) && 
       (only_deletes ? cell->flag != INSERT : cell->flag == INSERT) &&
       specs.is_matching(*cell, type)) {

      if(cell_offset) {
        --cell_offset;
        ++skips;
        continue;
      }
      
      cell->write(result);
      if(++count == specs.flags.limit) 
        // specs.flags.limit_by && specs.flags.max_versions
          break;
    } else 
      ++skips;
  }
}

void VectorBig::scan(Interval& interval, VectorBig& cells) const {
  if(!_size)
    return;

  Cell* cell;
  for(auto it = ConstIt(_narrow(interval.key_begin)); it; ++it) {
    if((cell = *it.item)->has_expired(ttl) || (!interval.key_begin.empty() 
        && interval.key_begin.compare(cell->key) == Condition::LT))
      continue;
    if(!interval.key_end.empty() 
        && interval.key_end.compare(cell->key) == Condition::GT)
      break; 

    cells.add_raw(*cell);
  }
}


void VectorBig::expand(Interval& interval) const {
  expand_begin(interval);
  if(size() > 1)
    expand_end(interval);
}

void VectorBig::expand_begin(Interval& interval) const {
  interval.expand_begin(*front());
}

void VectorBig::expand_end(Interval& interval) const {
  interval.expand_end(*back());
}


void VectorBig::split(size_t from, VectorBig& cells, 
                      Interval& intval_1st, Interval& intval_2nd,
                      bool loaded) {
  Cell* from_cell = *ConstIt(from).item;
  size_t count = _size;
  bool from_set = false;
  Iterator it_start;
  Cell* cell;
  for(auto it = It(_narrow(from_cell->key)); it; ++it) {
    cell=*it.item;

    if(!from_set) {
      if(cell->key.compare(from_cell->key, 0) == Condition::GT) {
       --count;
        continue;
      }

      it_start = it;
      intval_2nd.expand_begin(*cell);
      if(!loaded)
        break;
      from_set = true;
    }
    _remove(cell);
    if(cell->has_expired(ttl))
      delete cell;
    else
      cells.add_sorted_no_cpy(cell);
  }

  _remove(it_start, count, !loaded);

  intval_2nd.set_key_end(intval_1st.key_end);      
  intval_1st.key_end.free();
  expand_end(intval_1st);
}


void VectorBig::_add_remove(const Cell& e_cell, size_t offset) {
  int64_t ts = e_cell.get_timestamp();
  int64_t rev;
  bool chk_rev = (rev = e_cell.get_revision()) != AUTO_ASSIGN;
  Condition::Comp cond;

  Cell* cell;
  for(auto it = It(offset); it; ) {

    if((cell=*it.item)->has_expired(ttl)) {
      _remove(it);
      continue;
    }

    if((cond = cell->key.compare(e_cell.key, 0)) == Condition::GT) {
      ++it;
      continue;
    }

    if(cond == Condition::LT) {
      _insert(it, e_cell);
      return;
    }

    if((chk_rev && cell->get_revision() >= rev) ||
       (cell->removal() && cell->is_removing(ts)) )
      return;
    
    if(e_cell.is_removing(cell->get_timestamp()))
      _remove(it);
  }
  
  add_sorted(e_cell);
}

void VectorBig::_add_plain(const Cell& e_cell, size_t offset) {
  int64_t ts = e_cell.get_timestamp();
  int64_t rev;
  bool chk_rev = (rev = e_cell.get_revision()) != AUTO_ASSIGN;

  uint32_t revs = 0;
  Condition::Comp cond;
  Cell* cell;
  for(auto it = It(offset); it;) {

    if((cell=*it.item)->has_expired(ttl)) {
      _remove(it);
      continue;
    }

    if((cond = cell->key.compare(e_cell.key, 0)) == Condition::GT) {
      ++it;
      continue;
    }

    if(cond == Condition::LT) {
      _insert(it, e_cell);
      return;
    }

    if(chk_rev && cell->get_revision() >= rev)
      return;

    if(cell->removal()) {
      if(cell->is_removing(ts))
        return;
      ++it;
      continue;
    }

    if(ts != AUTO_ASSIGN && cell->get_timestamp() == ts)
      return cell->copy(e_cell);
    
    ++revs;
    if(e_cell.control & TS_DESC 
        ? e_cell.timestamp < cell->timestamp
          : e_cell.timestamp > cell->timestamp) {
      if(max_revs == revs)
        return;
      ++it;
      continue;
    }
    
    if(max_revs == revs) {
      cell->copy(e_cell);
    } else {
      _insert(it, e_cell);
      ++it;
      _remove_overhead(it, e_cell.key, revs);
    }
    return;
  }

  add_sorted(e_cell);
}

void VectorBig::_add_counter(const Cell& e_cell, size_t offset) {
  Condition::Comp cond;

  int64_t ts = e_cell.get_timestamp();
  int64_t rev;
  bool chk_rev = (rev = e_cell.get_revision()) != AUTO_ASSIGN;
  
  Cell* cell;
  auto it = It(offset); 
  for(; it; ) {

    if((cell=*it.item)->has_expired(ttl)) {
      _remove(it);
      continue;
    }

    if((cond = cell->key.compare(e_cell.key, 0)) == Condition::GT) { 
      ++it;
      continue;
    }

    if(cond == Condition::LT) //without aggregate|| ts == AUTO_ASSIGN
      goto add_counter;

    if(chk_rev && cell->get_revision() >= rev)
      return;

    if(cell->removal()) {
      if(cell->is_removing(ts))
        return;
      ++it;
      continue;
    }

    uint8_t op_1;
    int64_t eq_rev_1;
    int64_t value_1 = cell->get_counter(op_1, eq_rev_1);
    if(op_1 & OP_EQUAL) {
      if(!(op_1 & HAVE_REVISION))
        eq_rev_1 = cell->get_timestamp();
      if(eq_rev_1 > ts)
        return;
    }
    
    if(e_cell.get_counter_op() & OP_EQUAL)
      cell->copy(e_cell);
    else {
      value_1 += e_cell.get_counter();
      cell->set_counter(op_1, value_1, type, eq_rev_1);
      if(cell->timestamp < e_cell.timestamp) {
        cell->timestamp = e_cell.timestamp;
        cell->control = e_cell.control;
      }
    }
    return;
  }

  add_counter:
    cell = _insert(it, e_cell);
    if(type != Types::Column::COUNTER_I64) {
      uint8_t op_1;
      int64_t eq_rev_1;
      int64_t value_1 = cell->get_counter(op_1, eq_rev_1);
      cell->set_counter(op_1, value_1, type, eq_rev_1);
    }
}


size_t VectorBig::_narrow(const DB::Cell::Key& key) const {
  if(key.empty() || _size <= narrow_sz) 
    return 0;
  size_t sz;
  size_t offset = sz = (_size >> 1);

  try_narrow:
    if((*ConstIt(offset).item)->key.compare(key, 0) == Condition::GT) {
      if(sz < narrow_sz)
        return offset;
      offset += sz >>= 1; 
      goto try_narrow;
    }
    if((sz >>= 1) == 0)
      ++sz;  

    if(offset < sz)
      return 0;
    offset -= sz;
  goto try_narrow;
}


void VectorBig::_add(Cell* cell) {
  _bytes += cell->encoded_length();
  ++_size;
}

void VectorBig::_remove(Cell* cell) {
  _bytes -= cell->encoded_length();
  --_size;
}


void VectorBig::_push_back(Cell* cell) {
  if(buckets.back()->size() >= bucket_size)
    buckets.push_back(make_bucket());
  buckets.back()->push_back(cell);
}

Cell* VectorBig::_insert(VectorBig::Iterator& it, const Cell& cell) {
  Cell* adding;
  _add(adding = new Cell(cell));
  if(!it) 
    _push_back(adding);
  else 
    it.insert(adding);
  return adding;
}

void VectorBig::_remove(VectorBig::Iterator& it) {
  _remove(*it.item); 
  delete *it.item;
  it.remove();
}

void VectorBig::_remove(VectorBig::Iterator& it, size_t number, bool wdel) { 
  if(wdel) {
    auto it_del = Iterator(it);
    for(auto c = number; c && it_del; ++it_del,--c) {
      _remove(*it_del.item);
      delete *it_del.item;
    }
  }
  it.remove(number);
}

void VectorBig::_remove_overhead(VectorBig::Iterator& it, 
                                 const DB::Cell::Key& key, uint32_t revs) {
  while(it && (*it.item)->key.compare(key, 0) == Condition::EQ) {
    if((*it.item)->flag == INSERT && ++revs > max_revs)
      _remove(it);
    else 
      ++it;
  }
}



VectorBig::ConstIterator::ConstIterator(const VectorBig::Buckets* buckets, 
                                        size_t offset) 
                                        : buckets(buckets) {
  bucket = buckets->begin();
  if(offset) {
    for(; bucket < buckets->end(); ++bucket) {
      if(offset < (*bucket)->size()) {
        item = (*bucket)->begin() + offset;
        break;
      }
      offset -= (*bucket)->size();
    }
  } else if(bucket < buckets->end()) {
    item = (*bucket)->begin();
  }
}

VectorBig::ConstIterator::ConstIterator(const VectorBig::ConstIterator& other) 
                                        : buckets(other.buckets), 
                                          bucket(other.bucket), 
                                          item(other.item) {
}

VectorBig::ConstIterator::~ConstIterator() { }

const bool VectorBig::ConstIterator::avail() const {
  return bucket < buckets->end() && item < (*bucket)->end();
}

VectorBig::ConstIterator::operator bool() const {
  return avail();
}

void VectorBig::ConstIterator::operator++() {
  if(++item == (*bucket)->end() && ++bucket < buckets->end())
    item = (*bucket)->begin();
}




VectorBig::Iterator::Iterator() : buckets(nullptr) { }

VectorBig::Iterator::Iterator(VectorBig::Buckets* buckets, size_t offset) 
                              : buckets(buckets) {
  bucket = buckets->begin();
  if(offset) {
    for(; bucket < buckets->end(); ++bucket) {
      if(offset < (*bucket)->size()) {
        item = (*bucket)->begin() + offset;
        break;
      }
      offset -= (*bucket)->size();
    }
  } else if(bucket < buckets->end()) {
    item = (*bucket)->begin();
  }
}

VectorBig::Iterator::Iterator(const VectorBig::Iterator& other) 
                              : buckets(other.buckets), 
                                bucket(other.bucket), item(other.item) {
}

VectorBig::Iterator& 
VectorBig::Iterator::operator=(const VectorBig::Iterator& other) {
  buckets = other.buckets;
  bucket = other.bucket;
  item = other.item;
  return *this;
}

VectorBig::Iterator::~Iterator() { }

VectorBig::Iterator::operator bool() const {
  return avail();
}

const bool VectorBig::Iterator::avail() const {
  return bucket < buckets->end() && item < (*bucket)->end();
}

void VectorBig::Iterator::operator++() {
  if(++item == (*bucket)->end() && ++bucket < buckets->end())
    item = (*bucket)->begin();
}

const bool VectorBig::Iterator::avail_begin() const {
  return bucket != buckets->begin() && item >= (*bucket)->begin();
}

void VectorBig::Iterator::operator--() {
  if(--item == (*bucket)->begin() && --bucket >= buckets->begin())
    item = (*bucket)->end() - 1;
}

void VectorBig::Iterator::push_back(Cell*& value) {
  if((*bucket)->size() >= bucket_max)
    buckets->push_back(make_bucket());

  (*bucket)->push_back(value);
  item = (*bucket)->end() - 1;
}

void VectorBig::Iterator::insert(Cell*& value) {
  item = (*bucket)->insert(item, value);

  if((*bucket)->size() >= bucket_max) {
    auto offset = item - (*bucket)->begin();

    auto nbucket = buckets->insert(++bucket, make_bucket());

    auto it_b = (*(bucket = nbucket-1))->begin() + bucket_split;
    auto it_e = (*bucket)->end();
    (*nbucket)->assign(it_b, it_e);
    (*bucket)->erase  (it_b, it_e);

    if(offset >= bucket_split)
      item = (*(bucket = nbucket))->begin() + offset - bucket_split;
  }
}

void VectorBig::Iterator::remove() {
  (*bucket)->erase(item);
  if((*bucket)->empty() && buckets->size() > 1) {
    delete *bucket; 
    if((bucket = buckets->erase(bucket)) != buckets->end())
      item = (*bucket)->begin();
  } else if(item == (*bucket)->end() && ++bucket < buckets->end()) {
    item = (*bucket)->begin();
  }
}

void VectorBig::Iterator::remove(size_t number) {
  while(number) {

    if(item == (*bucket)->begin() && number >= (*bucket)->size()) {
      if(buckets->size() == 1) {
        (*bucket)->clear();
        item = (*bucket)->end();
        return;
      }
      number -= (*bucket)->size();
      delete *bucket;
      bucket = buckets->erase(bucket);

    } else {
      size_t avail;
      if((avail = (*bucket)->end() - item) > number) {
        item = (*bucket)->erase(item, item + number);
        return;
      }
      number -= avail;
      (*bucket)->erase(item, item + avail);
      ++bucket;
    }
    
    if(bucket == buckets->end())
      return;
    item = (*bucket)->begin();
  }
}



}}}
