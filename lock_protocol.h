// lock protocol

#ifndef lock_protocol_h
#define lock_protocol_h

#include "rpc.h"

class lock_protocol {
 public:
  enum xxstatus { OK, RETRY, RPCERR, NOENT, IOERR, UNKNOWNERROR };
  typedef int status;
  typedef unsigned long long lockid_t;
  enum rpc_numbers {
    acquire = 0x7001,
    release,
    stat
  };
  enum lock_ret {
    ok = 0,
    lock_not_found,
    lock_false_release,
    lock_is_free
  };
  enum client_state {
    None = 0x6001,
    Free,
    Locked,
    Acquiring,
    Releasing
  };
  enum server_state {
    Server_free = 0x5001,
    Server_locked = 0x5002,
    Server_revoke
  };
};

class rlock_protocol {
public:
    enum xxstatus { OK, RPCERR };
    typedef int status;
    enum rpc_numbers {
        revoke = 0x8001,
        retry = 0x8002
    };
};

#endif 
