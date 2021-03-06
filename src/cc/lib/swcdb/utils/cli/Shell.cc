/*
 * Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
 */

#include "swcdb/utils/cli/Shell.h"
#include "swcdb/utils/cli/Shell_DbClient.h"
#include "swcdb/utils/cli/Shell_Manager.h"
#include "swcdb/utils/cli/Shell_Ranger.h"
#include "swcdb/utils/cli/Shell_FsBroker.h"

#include <queue>

#include <editline.h> // github.com/troglobit/editline
int el_hist_size = 4000;

namespace SWC { namespace Utils { namespace shell {


int run() {
  auto settings = SWC::Env::Config::settings();

  if(settings->has("ranger"))
    return Rgr().run();
    
  if(settings->has("manager"))
    return Mngr().run();
  
  if(settings->has("fsbroker"))
    return FsBroker().run();

  try {
    return DbClient().run();
  } catch (std::exception& e) {
    SWC_PRINT << e.what() << SWC_PRINT_CLOSE;
  }

  return 1;
}

Interface::Interface(const char* prompt, const char* history)
                    : prompt(prompt), history(history), err(Error::OK) {
  init();
}
  
Interface::~Interface() {
  for(auto o : options)
    delete o;
}

int Interface::run() {
  
  read_history(history);
  char* line;
  char* ptr;
  const char* prompt_state = prompt;
  char c;
    
  bool stop = false;
  bool cmd_end = false;    
  bool escape = false;
  bool comment = false;
  bool quoted_1 = false;
  bool quoted_2 = false;
  bool is_quoted = false;
  bool next_line = false;
  std::string cmd;
  std::queue<std::string> queue;

  while(!stop && (ptr = line = readline(prompt_state))) {

    prompt_state = ""; // "-> ";
    do {
      c = *ptr++;
      if(next_line = c == 0)
        c = '\n';

      if(c == '\n' && cmd_end) {
        while(!queue.empty()) {
          auto& run_cmd = queue.front();
          add_history(run_cmd.c_str());
          write_history(history);
          run_cmd.pop_back();
          if(stop = !cmd_option(run_cmd))
            break;
          queue.pop();
        }
        cmd_end = false;
        prompt_state = prompt;
        break;

      } else if(!comment && c == '\n' && cmd.empty()) {
        prompt_state = prompt;
        break;

      } else if(c == ' ' && cmd.empty()) {
        continue;

      } else if(!is_quoted && !escape) {
        if(c == '#') {
          comment = true;
          continue;
        } else if(c == '\n' && comment) {
          comment = false;
          break;
        } else if(comment)
          continue;
      }

      cmd += c;

      if(escape) {
        escape = false;
        continue;
      } else if(c == '\\') {
        escape = true;
        continue;
      }
        
      if((!is_quoted || quoted_1) && c == '\'')
        is_quoted = quoted_1 = !quoted_1;
      else if((!is_quoted || quoted_2) && c == '"')
        is_quoted = quoted_2 = !quoted_2;
      else if(c == ';' && !is_quoted) {
        cmd_end = true;
        queue.push(cmd);
        cmd.clear();
      }

    } while(!next_line);
      
	  free(line);
  }

  return 0;
}

void Interface::init() {
  options.push_back(
    new Option(
      "quit", 
      {"Quit or Exit the Console"}, 
      [ptr=this](std::string& cmd){return ptr->quit(cmd);}, 
      new re2::RE2("(?i)^(quit|exit)(\\s+|$)")
    )
  );
  options.push_back(
    new Option(
      "help", 
      {"Commands help information"}, 
      [ptr=this](std::string& cmd){return ptr->help(cmd);}, 
      new re2::RE2("(?i)^(help)(\\s+|$)")
    )
  );
}
  
bool Interface::quit(std::string& cmd) const {
  return false;
}

bool Interface::help(std::string& cmd) const {
  std::scoped_lock lock(Logger::logger.mutex);
  std::cout << "Usage Help:  \033[4m'command' [options];\033[00m\n";
  size_t offset_name = 0;
  size_t offset_desc = 0;
  for(auto opt : options) {
    if(offset_name < opt->name.length())
      offset_name = opt->name.length();
    for(const auto& desc : opt->desc) {
      if(offset_desc < desc.length())
        offset_desc = desc.length();
    }
  }
  offset_name += 4;
  offset_desc += 4;
  bool first;
  for(auto opt : options) {
    std::cout << std::left << std::setw(2) << " "
              << std::left << std::setw(offset_name) << opt->name;
    first = true;
    for(const auto& desc : opt->desc) {
      if(!first)
        std::cout << std::left << std::setw(2) << " "
                  << std::left << std::setw(offset_name) << " ";
      std::cout << std::setw(offset_desc) << desc << std::endl;
      first = false;
    }
        
  }
  return true;
}

const bool Interface::error(const std::string& message) {
  SWC_PRINT << "\033[31mERROR\033[00m: " << message 
            << SWC_PRINT_CLOSE;
  return true; /// ? err
}

const bool Interface::cmd_option(std::string& cmd) const {
  err = Error::OK;
  auto opt = std::find_if(options.begin(), options.end(), 
              [cmd](const Option* opt){ 
                return RE2::PartialMatch(cmd.c_str(), *opt->re); 
              });
  if(opt != options.end())
    return (*opt)->call(cmd);
  SWC_PRINT << "Unknown command='\033[31m" << cmd << ";\033[00m'" 
            << SWC_PRINT_CLOSE;
  return true;
}


}}} // namespace SWC::Utils::shell

extern "C" {
int swc_utils_run() {
  return SWC::Utils::shell::run();
};
void swc_utils_apply_cfg(SWC::Env::Config::Ptr env){
  SWC::Env::Config::set(env);
};
}
