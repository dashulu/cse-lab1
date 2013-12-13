#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include <list>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

class server_lock_record {
public:
	lock_protocol::server_state state;
	std::string id;
	std::list<std::string> waiting_list;
};

class lock_server_cache {
 private:
  int nacquire;
  pthread_mutex_t lock;
  std::map<lock_protocol::lockid_t, server_lock_record > states;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
