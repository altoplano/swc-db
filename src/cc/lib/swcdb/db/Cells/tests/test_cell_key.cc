/*
 * Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
 */


#include <memory>
#include <vector>
#include <iostream>

#include "swcdb/db/Cells/CellKey.h"
#include "swcdb/db/Cells/SpecsKey.h"

namespace DB = SWC::DB;
namespace Condition = SWC::Condition;

void test_basic(){
  char * fraction;
  uint32_t length;
  Condition::Comp comp;

  std::cout << "\ntest_basic start! \n";
  DB::Cell::Key key;
  key.add("abc");
  key.add("def", 3);
  key.add(std::string("ghi0"));
  key.add("jkl4");

  std::cout << "\n # DB::Cell::Key\n";
  
  key.get(0, &fraction, &length);
  std::cout << std::string(fraction, length) << "(" << length <<"),";
  key.get(1, &fraction, &length);
  std::cout << std::string(fraction, length) << "(" << length <<"),";
  key.get(2, &fraction, &length);
  std::cout << std::string(fraction, length) << "(" << length <<"),";
  key.get(3, &fraction, &length);
  std::cout << std::string(fraction, length) << "(" << length <<"),";

  std::cout << "\nfractions-count=" <<  key.count <<"\n";
  std::cout <<  key.to_string() <<"\n";

  std::cout << "key.equal(key) \n";
  if(!key.equal(key)) {
    std::cout << "\n Bad-!key.equal(key) \n";
    std::cout <<  key.to_string() <<"\n";
    exit(1);
  }

  std::cout << "key_cpy.equal(key) \n";
  DB::Cell::Key key_cpy(key);
  if(!key_cpy.equal(key)) {
    std::cout << "\n Bad-!key_cpy.equal(key) \n";
    std::cout <<  key.to_string() <<"\n";
    std::cout <<  key_cpy.to_string() <<"\n";
    exit(1);
  }
  
  std::cout << "key_insert \n";
  DB::Cell::Key key_insert;
  key_insert.add("abc");
  key_insert.add(std::string("ghi0"));
  key_insert.add("jkl4");
  std::cout << "key_insert.insert \n";
  key_insert.insert(1, "def");

  std::cout << "key.equal(key_insert) \n";
    std::cout <<  key.to_string() <<"\n";
  if(!key.equal(key_insert)) {
    std::cout << "\n Bad-!key.equal(key_insert) \n";
    std::cout <<  key.to_string() <<"\n";
    std::cout <<  key_insert.to_string() <<"\n";
    exit(1);
  }

  std::cout << "key_cpy.encoded_length()\n";
  size_t remain = key_cpy.encoded_length();
  uint8_t* buff = new uint8_t[remain];
  uint8_t* ptr = buff;
  std::cout << "key_cpy.encode \n";
  key_cpy.encode(&ptr);
  DB::Cell::Key key_encoded;
  const uint8_t* rptr = (const uint8_t*)buff;
  std::cout << "key_encoded.decode \n";
  key_encoded.decode(&rptr, &remain, true);

  std::cout << "key_encoded.equal(key_cpy) \n";
  if(!key_encoded.equal(key_cpy)) {
    std::cout << "\n Bad-!key_encoded.equal(key_cpy) \n";
    std::cout <<  "encoded_length=" << key_cpy.encoded_length() <<"\n";
    std::cout <<  "wrote=" << ptr-buff <<"\n";
    std::cout <<  key_cpy.to_string() <<"\n";
    std::cout <<  key_encoded.to_string() <<"\n";
    std::cout <<  "read=" << rptr-buff <<"\n";
    exit(1);
  }
  delete [] buff;
  std::cout << "delete [] buff OK \n";
  
  std::cout << "\n # DB::Specs::Key\n";
  DB::Specs::Key spec_key;
  spec_key.add("abc", Condition::LE);
  spec_key.add("def", 3, Condition::EQ);
  spec_key.add(std::string("ghi"), Condition::GE);
  spec_key.add("jkl", Condition::GT);

  spec_key.get(0, &fraction, &length, &comp);
  std::cout << std::string(fraction, length) << "(" << length <<"),"  << " " << comp << ",";
  spec_key.get(1, &fraction, &length, &comp);
  std::cout << std::string(fraction, length) << "(" << length <<"),"  << " " << comp << ",";
  spec_key.get(2, &fraction, &length, &comp);
  std::cout << std::string(fraction, length) << "(" << length <<"),"  << " " << comp << ",";
  spec_key.get(3, &fraction, &length, &comp);
  std::cout << std::string(fraction, length) << "(" << length <<"),"  << " " << comp << ",";
  std::cout << "\nfractions-count=" <<  spec_key.count <<"\n";
  std::cout <<  spec_key.to_string() <<"\n";

  std::cout << "spec_key.is_matching(key) \n";
  if(!spec_key.is_matching(key))
    exit(1);
    
  std::cout << "key2.equal(key) \n";
    std::cout <<  key.to_string() <<"\n";
  DB::Cell::Key key2(key);
  if(!key2.equal(key)) {
    std::cout << "\n Bad-Copy \n";
    std::cout <<  key.to_string() <<"\n";
    std::cout <<  key2.to_string() <<"\n";
    exit(1);
  }
    std::cout <<  key2.to_string() <<"\n";

  DB::Specs::Key spec_key2(spec_key);
  if(!spec_key2.equal(spec_key)) {
    std::cout << "\n Bad-Copy \n";
    std::cout <<  spec_key.to_string() <<"\n";
    std::cout <<  spec_key2.to_string() <<"\n";
    exit(1);
  }
  
  
  DB::Specs::Key spec_key_insert;
  spec_key_insert.add("def", 3, Condition::EQ);
  spec_key_insert.add(std::string("ghi"), Condition::GE);
  spec_key_insert.insert(0, "abc", Condition::LE);
  spec_key_insert.insert(3, "jkl", Condition::GT);
  
  if(!spec_key.equal(spec_key_insert)) {
    std::cout << "\n Bad-!spec_key.equal(spec_key_insert) \n";
    std::cout <<  spec_key.to_string() <<"\n";
    std::cout <<  spec_key_insert.to_string() <<"\n";
    exit(1);
  }

  DB::Specs::Key spec_key_remove;
  spec_key_remove.add("def", 3, Condition::EQ);
  spec_key_remove.add(std::string("ghi"), Condition::GE);
  spec_key_remove.insert(0, "abc", Condition::LE);

  DB::Specs::Key spec_key_remove_cpy;
  spec_key_remove_cpy.copy(spec_key_remove);
  spec_key_remove.insert(3, "jkl", Condition::GT); // removed
  spec_key_remove.remove(3);


  if(!spec_key_remove_cpy.equal(spec_key_remove)) {
    std::cout << "\n Bad-!spec_key_remove_cpy.equal(spec_key_remove)) \n";
    std::cout <<  spec_key_remove_cpy.to_string() <<"\n";
    std::cout <<  spec_key_remove.to_string() <<"\n";
    exit(1);
  }

  DB::Specs::Key spec_key_remove_recurs;
  spec_key_remove_recurs.copy(spec_key_remove);
  spec_key_remove_recurs.add("jkl", Condition::GT); // removed
  spec_key_remove_recurs.add("jk2", Condition::GT); // removed
  spec_key_remove_recurs.add("jk3", Condition::GT); // removed
  spec_key_remove_recurs.add("jk4", Condition::GT); // removed
  spec_key_remove_recurs.remove(3, true);

  if(!spec_key_remove_cpy.equal(spec_key_remove_recurs)) {
    std::cout << "\n Bad-!spec_key_remove_cpy.equal(spec_key_remove_recurs)) \n";
    std::cout <<  spec_key_remove_cpy.to_string() <<"\n";
    std::cout <<  spec_key_remove_recurs.to_string() <<"\n";
    exit(1);
  }

  std::cout << "SPEC KEY TO CELL KEY: \n";
  DB::Cell::Key spec_to_key;
  spec_key_remove_recurs.get(spec_to_key);
  std::cout << spec_key_remove_recurs.to_string() << "\n";
  std::cout << spec_to_key.to_string() << "\n";

  std::cout << "\ntest_basic OK! \n";
}

void load_check_key(int chks, int num_fractions, int chk_count){

  auto start_ts_1 = std::chrono::system_clock::now();
  for(int n=0; n < chks;n++) {
    DB::Cell::Key key;
    for(auto b=0;b<num_fractions;b++)
      key.add(std::to_string(b+2^60));
  }
  auto took_add = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now() - start_ts_1).count();
        
  DB::Cell::Key key;
  for(auto b=0;b<num_fractions;b++)
    key.add(std::to_string(b+2^60));    

  auto start_ts_2 = std::chrono::system_clock::now();
  for(int n=0; n < chks;n++) {
    char * fraction;
    uint32_t length;
    for(auto b=0;b<num_fractions;b++) 
      key.get(b, &fraction, &length);
  }
  auto took_get = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now() - start_ts_2).count();
  /*
  auto start_ts_get22 = std::chrono::system_clock::now();
  for(int n=0; n < chks;n++) {
    char * fraction;
    uint32_t length;
    for(auto b=0;b<num_fractions;b++) 
      key.get2(b, &fraction, &length);
  }
  auto took_get2 = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now() - start_ts_get22).count();
  */

  auto start_ts_3 = std::chrono::system_clock::now();
  for(int n=0; n < chks;n++) {
    for(int n=0; n < chk_count;n++)
      int count = key.count;
  }
  auto took_count = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now() - start_ts_3).count();

  size_t sz = sizeof(key)+sizeof(key.size)+sizeof(key.count);//*(sizeof(DB::Cell::Key::Fraction));

  std::cout << "Cell::Key, sz=" << sz
                    << " add=" << took_add 
                    << " get=" << took_get
                    << " cnt=" << took_count 
                    << " avg(add)=" << took_add/chks 
                    << " avg(get)=" << took_get/chks 
                    //<< " avg(get2)=" << took_get2/chks 
                    << " avg(cnt)=" << took_count/chks <<  "\n";
  
}

void load_check_vec(int chks, int num_fractions, int chk_count){

  auto start_ts_1 = std::chrono::system_clock::now();
  for(int n=0; n < chks;n++) {
    std::vector<std::string*> key;
    for(auto b=0;b<num_fractions;b++)
      key.push_back(new std::string(std::to_string(b+2^60)));
  }
  auto took_add = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now() - start_ts_1).count();
        
  std::vector<std::string*> key;
  for(auto b=0;b<num_fractions;b++){
    key.push_back(new std::string(std::to_string(b+2^60)));
  }
  auto start_ts_2 = std::chrono::system_clock::now();
  for(int n=0; n < chks;n++) {
    char *      fraction;
    uint32_t    length; 
    for(auto b=0;b<num_fractions;b++) {
      std::string* s = key.at(b);
      length = s->length();
      fraction = s->data();
    }
  }
  auto took_get = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now() - start_ts_2).count();

  auto start_ts_3 = std::chrono::system_clock::now();
  for(int n=0; n < chks;n++) {
    for(int n=0; n < chk_count;n++)
      int count = key.size();
  }
  auto took_count = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now() - start_ts_3).count();
  
  size_t sz = sizeof(key)+8;
  for(auto& s : key)
    sz += 8+sizeof(s)+8;

  std::cout << "vector,  sz=" << sz
                    << " add=" << took_add 
                    << " get=" << took_get
                    << " cnt=" << took_count 
                    << " avg(add)=" << took_add/chks 
                    << " avg(get)=" << took_get/chks 
                    << " avg(cnt)=" << took_count/chks <<  "\n";
}

int main() {
  
  test_basic();
  //exit(0);
  std::cout << "\n";

  int chks = 1000;
  int chk_count = 1;

  for(int fractions = 0;fractions<=500;fractions+=10){
    std::cout << "fractions=" << fractions << "\n";
    load_check_key(chks, fractions, chk_count);
    load_check_vec(chks, fractions, chk_count);
  }

  exit(0);
   
}