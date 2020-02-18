/**
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */

#include <vector>
#include <cassert>
#include <iostream>

#include "swcdb/core/PageArena.h"

int main() {
  size_t num_items = 1000;
  size_t num_uses = 1000;
  auto& arena = SWC::Env::PageArena;
  
    
  std::cout << "sizeof:" << "\n";
  std::cout << " Arena=" << sizeof(SWC::Mem::Arena) << "\n";
  std::cout << " arena=" << sizeof(arena) << "\n";
  std::cout << " Item=" << sizeof(SWC::Mem::Item) << "\n";
  std::cout << " ItemPtr=" << sizeof(SWC::Mem::ItemPtr) << "\n";
    
  std::vector<SWC::Mem::ItemPtr> data;
    
  for(size_t n=0; n<num_uses;++n) {
    std::cout << " set-"<< n << " begin\n";
    for(size_t i = num_items; i>0;--i) {
      std::string s(std::to_string(i));
      data.push_back(SWC::Mem::ItemPtr((const uint8_t*)s.data(), s.length()));
    }
    std::cout << " set-"<< n << " end\n";
  }

  std::cout << " num_items="<< num_items  << std::flush
            << " arena.count()= " << arena.count()  << std::flush
            << " data.size()= " << data.size() << "\n";
    
  assert(arena.count() == num_items);
  
  for(auto idx=arena.pages() ; idx;) {
    for(auto item : arena.page(--idx)) {
      std::cout << item->to_string() << "=" << item->count << "\n";
      std::cout << "num_uses=" << num_uses << " p->count=" << item->count << "\n";
      assert(item->count == num_uses);
    }
  }
  assert(arena.count() == num_items);
  std::cout << " arena.count="<< arena.count() << "\n";
  std::cout << "\n";


  for(auto idx=arena.pages() ;idx;) {
    auto c = arena.page(--idx).count();
    if(c)
      std::cout << "set on page-idx=" << idx << " count="<< arena.page(idx).count() << "\n";
  }
  
  std::cout << " release  begin\n";
  data.clear();
  std::cout << " release  end\n";
        
        
  assert(!arena.count());

  std::cout << " OK! \n";
  return 0;
}
