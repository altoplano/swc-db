/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */


#include "swcdb/db/client/sql/Reader.h"
#include "swcdb/db/client/sql/SQL.h"


namespace SWC { namespace client { namespace SQL {


Reader::Reader(const std::string& sql, std::string& message)
              : sql(sql), message(message),
                ptr(sql.data()), remain(sql.length()), 
                err(Error::OK) {
}

Reader::~Reader() {}

const bool Reader::is_char(const char* stop) const {
  if(stop) do {
    if(*stop++ == *ptr)
      return true;
  } while(*stop);
  return false;
}

const bool Reader::found_char(const char c) {
  if(*ptr == c) {
    ptr++;
    remain--;
    return true;
  }
  return false;
}

const bool Reader::found_space() {
  return found_char(' ') || found_char('\t') 
      || found_char('\n') || found_char('\r');
}

const bool Reader::found_quote_single(bool& quote) {
  if(found_char('\'')) {
    quote = !quote;
    return true;
  }
  return false;
}

const bool Reader::found_quote_double(bool& quote) {
  if(found_char('"')) {
    quote = !quote;
    return true;
  }
  return false;
}

const bool Reader::found_token(const char* token, uint8_t token_len) {
  if(remain >= token_len && strncasecmp(ptr, token, token_len) == 0) {
    ptr += token_len;
    remain -= token_len;
    return true;
  }
  return false;
}

const bool Reader::found_comparator(Condition::Comp& comp) {
  while(remain) {
    if(found_space())
      continue;
    if((comp = Condition::from(&ptr, &remain)) != Condition::NONE)
      return true;
    break;
  }
  return false;
}


void Reader::expect_eq() {
  while(remain && !err && found_space());
  bool eq = false;
  expect_token("=", 1, eq); // ? (not space is eq)
}

void Reader::expect_comma(bool& comma) {
  while(remain && !err && found_space());
  expect_token(",", 1, comma);
}
  
void Reader::expect_comparator(Condition::Comp& comp) {
  if(found_comparator(comp) && comp != Condition::NONE)
    return;
  error_msg(Error::SQL_PARSE_ERROR, "missing 'comparator'");
}

void Reader::expect_digit() {
  if(remain >= 1) {
    if(std::isdigit((unsigned char)*ptr)) {
      ptr++;
      remain--;
      return;
    }
  }
  error_msg(Error::SQL_PARSE_ERROR, "missing 'digit'");
}

void Reader::expected_boolean(bool& value) {
  if(found_char('1') || found_token(TOKEN_BOOL_TRUE, LEN_BOOL_TRUE))
    value = true;
  else if(found_char('0') || found_token(TOKEN_BOOL_FALSE, LEN_BOOL_FALSE))
    value = false;
  else {
    ptr++;
    remain--;
    error_msg(Error::SQL_PARSE_ERROR, "missing 'bool'");
  }
}

void Reader::expect_token(const char* token, uint8_t token_len, bool& found) {
  if(remain >= token_len) {
    if(found_space())
      return;

    if(strncasecmp(ptr, token, token_len) == 0) {
      ptr += token_len;
      remain -= token_len;
      found = true;
      return;
    }
  }
  error_msg(Error::SQL_PARSE_ERROR, "missing '"+std::string(token)+"'");
}

DB::Schema::Ptr Reader::get_schema(const std::string& col) {
  DB::Schema::Ptr schema = 0;
  if(std::find_if(col.begin(), col.end(), 
      [](unsigned char c){ return !std::isdigit(c); } ) != col.end()){
    schema = Env::Clients::get()->schemas->get(err, col);
  } else {
    errno = 0;
    try { 
      schema = Env::Clients::get()->schemas->get(err, std::stoll(col));
    } catch(const std::exception& ex) {
      err = errno;
    }
  }
  if(err)
    error_msg(err, "problem getting column '"+col+"' schema");
  return schema;
}

void Reader::read(std::string& buf, const char* stop, bool keep_escape) {
  uint32_t escape = 0;
  bool quote_1 = false;
  bool quote_2 = false;
  bool is_quoted = false;
  bool was_quoted = false;

  while(remain && !err) {
    if(!escape && *ptr == '\\') {
      escape = remain-1;
      if(!keep_escape) {
        ptr++;
        remain--;
        continue;
      }
    } else if(escape && escape != remain)
      escape = 0;

    if(!escape) {
      if(!is_quoted && buf.empty() && found_space())
        continue;
      if(((!is_quoted || quote_1) && found_quote_single(quote_1)) || 
          ((!is_quoted || quote_2) && found_quote_double(quote_2))) {
        is_quoted = quote_1 || quote_2;
        was_quoted = true;
        continue;
      }
      if((was_quoted && !is_quoted) || 
          (!was_quoted && (found_space() || is_char(stop))))
        break;
    }
    buf += *ptr;
    ptr++;
    remain--;
  }
}


void Reader::read_uint8_t(uint8_t& value, bool& was_set) {
  int64_t v;
  read_int64_t(v, was_set);
  if (v > UINT8_MAX || v < INT8_MIN)
    error_msg(Error::SQL_PARSE_ERROR, " unsigned 8-bit integer out of range");
  else
    value = (uint8_t)v;
}

void Reader::read_uint32_t(uint32_t& value, bool& was_set) {
  int64_t v;
  read_int64_t(v, was_set);
  if (v > UINT32_MAX || v < INT32_MIN)
    error_msg(Error::SQL_PARSE_ERROR, " unsigned 32-bit integer out of range");
  else
    value = (uint32_t)v;
}

void Reader::read_int64_t(int64_t& value, bool& was_set) {
  std::string buf;
  read(buf, "),]");
  if(err) 
    return;
  try {
    value = std::stoll(buf);
    was_set = true;
  } catch(const std::exception& ex) {
    error_msg(Error::SQL_PARSE_ERROR, " signed 64-bit integer out of range");
  }
}


void Reader::read_key(DB::Cell::Key& key) {
  bool bracket_square = false;
  std::string fraction;
    
  while(remain && !err && found_space());
  expect_token("[", 1, bracket_square);

  while(remain && !err) {
    if(found_space())
      continue;

    read(fraction, ",]");
    key.add(fraction);
    fraction.clear();

    while(remain && !err && found_space());
    if(found_char(',')) 
      continue;
    break;
  }
    
  expect_token("]", 1, bracket_square);
}


void Reader::error_msg(int error, const std::string& msg) {
  err = error;
  auto at = sql.length() - remain + 5;
  message.append("error=");
  message.append(std::to_string(err));
  message.append("(");
  message.append(Error::get_text(err));
  message.append(")\n");
  
  message.append(" SQL=");
  message.append(sql);
  message.append("\n");
  message.insert(message.length(), at, ' ');
  message.append("^ at=");
  message.append(std::to_string(at));
  message.append("\n");
  message.insert(message.end(), at, ' ');
  message.append(msg);
  message.append("\n");
}




}}} // SWC::client:SQL namespace
