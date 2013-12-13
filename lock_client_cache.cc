// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
             class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  pthread_mutex_init(&global_lock, NULL);
  pthread_mutex_init(&lock_needs_free_lock, NULL);
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  int value;
  std::map<lock_protocol::lockid_t, lock_record >::iterator iter;
//  printf("lid: %ld id:%s\n", lid, id.c_str());
  std::cout<<"client acquire id:"<<id<<" lid:"<<lid<<"\n";
switch_start:
  pthread_mutex_lock(&global_lock);
  iter = states.find(lid);
  if(iter != states.end()) {
    switch(iter->second.state) {
      case lock_protocol::None:
        std::cout<<"none\n";
        iter->second.state = lock_protocol::Acquiring;
        pthread_mutex_unlock(&global_lock);
        ret = cl->call(lock_protocol::acquire, lid, id, value);
        pthread_mutex_lock(&global_lock);
//        tprintf("id:%s ret:%d\n", id.c_str(), ret);
        if(ret == 1) {
          iter->second.num++;
          pthread_cond_wait(&iter->second.cv, &global_lock);
          std::cout<<"wake up from None. id:" << id << std::endl;
          iter->second.state = lock_protocol::Locked;
          iter->second.num--;
          ret = lock_protocol::OK;
        } else if(ret > 1){
          std::cout<<"error when RPC acquire.id:" << id << std::endl;
          pthread_mutex_unlock(&global_lock);
          return lock_protocol::UNKNOWNERROR;
        } else {
          if(!lock_needs_free[lid])
            lock_needs_free[lid] = (value == 1);
        //  lock_needs_free[lid] = true;
          ret = lock_protocol::OK;
        }
        iter->second.state = lock_protocol::Locked;
        break;
      case lock_protocol::Free:
        std::cout<<"Free\n";
        ret = lock_protocol::OK;
        iter->second.state = lock_protocol::Locked;
        break;
      case lock_protocol::Locked:
      case lock_protocol::Acquiring:
 //       tprintf("id:%s lid:%d iter->second++\n", id.c_str(), lid);
        iter->second.num++;
        pthread_cond_wait(&iter->second.cv, &global_lock);
        std::cout<<" wake up from acquiring:\n";
        iter->second.num--;
        ret = lock_protocol::OK;
        break;
      case lock_protocol::Releasing:
      //  iter->second.num++;
      //  pthread_cond_wait(&iter->second.cv, &global_lock);
      //  std::cout<<"wake up from releasing\n";
      //  iter->second.num--;
        pthread_mutex_unlock(&global_lock);
        goto switch_start;
      default:
        pthread_mutex_unlock(&global_lock);
        return lock_protocol::UNKNOWNERROR;
    }
  } else {
//    lock_record tmp;
    states[lid].num = 0;
    states[lid].state = lock_protocol::None;
    int r = pthread_cond_init(&states[lid].cv, NULL);
    if(r == EINVAL) {
      printf("einval\n");
      return lock_protocol::UNKNOWNERROR;
    } else if(r == ENOMEM) {
      printf("enomem\n");
      return lock_protocol::UNKNOWNERROR;
    }
//    states[lid] = tmp;
    pthread_mutex_unlock(&global_lock);
    goto switch_start;
  }
  pthread_mutex_unlock(&global_lock);
  return ret;
}

int
lock_client_cache:: stat(lock_protocol::lockid_t)
{
  return 0;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  std::map<lock_protocol::lockid_t, lock_record >::iterator iter;
  lock_protocol::status ret = lock_protocol::OK;

  std::cout<<"release lid:"<<lid<<" id:"<<id<<std::endl;
  pthread_mutex_lock(&global_lock);
  iter = states.find(lid);
  if(iter != states.end()) {
    if(iter->second.state != lock_protocol::Locked) {
      tprintf("client release error: state is not locked.\n");
      ret = lock_protocol::UNKNOWNERROR;
    } else {
      if(iter->second.num > 0) {
        tprintf("wake up other thread 0.\n");
        pthread_cond_signal(&iter->second.cv);
        tprintf("wait up other thread.\n");
      } else {
        tprintf("wait list is zero.\n");
        if(lock_needs_free[lid]) {
          tprintf("need free\n");
          int value = 0;
          iter->second.state = lock_protocol::Releasing;
          pthread_mutex_unlock(&global_lock);
          ret = cl->call(lock_protocol::release, lid, id, value);
          pthread_mutex_lock(&global_lock);
          iter->second.state = lock_protocol::None;
          lock_needs_free[lid] = false;
          pthread_cond_signal(&iter->second.cv);
          ret = lock_protocol::OK;
        } else {
          tprintf("no need for free.\n");
          iter->second.state = lock_protocol::Free;
        }
      }
    }
  } else {
    tprintf("client release error : not find lock in the client.\n");
    ret = lock_protocol::UNKNOWNERROR;
  }
  
  pthread_mutex_unlock(&global_lock);
  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                   int &)
{
  int ret = rlock_protocol::OK;
  std::map<lock_protocol::lockid_t, lock_record >::iterator iter;
  pthread_mutex_lock(&global_lock);
  iter = states.find(lid);

  if(iter == states.end()) {
    ret = 3;
  } else {
    switch(iter->second.state) {
      case lock_protocol::None:
        ret = 2;
        lock_needs_free[lid] = true;
        break;
      case lock_protocol::Free:
        ret = 0;
        iter->second.state = lock_protocol::None;
        lock_needs_free[lid] = false;
        break;
      case lock_protocol::Locked:
      case lock_protocol::Acquiring:
        ret = 1;
        lock_needs_free[lid] = true;
        break;
      case lock_protocol::Releasing:
        lock_needs_free[lid] = true;
        ret = 1;
        break;
      default:
        ret = 3;
        break;
    }
  }
  std::cout<<"call for revoke: need free:"<<lock_needs_free[lid]<<" lid:"<<lid<<std::endl;
  std::cout<<"lock state: "<<iter->second.state<<" lid:"<<lid;
  tprintf("here.\n");

  pthread_mutex_unlock(&global_lock);
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                int &)
{
  int ret = rlock_protocol::OK;
  std::map<lock_protocol::lockid_t, lock_record >::iterator iter;

  int need_release = lid % 4;
  lid /= 4;
  std::cout<<"retry function is called id:"<<id<<" lid:"<<lid<<" need release :"<< need_release<<"\n";

 // printf("lid:%ld,  need_release:%d, \n", lid, need_release);
  pthread_mutex_lock(&global_lock);
  iter = states.find(lid);
  if(iter != states.end()) {
    iter->second.state = lock_protocol::Locked;
//    pthread_cond_signal(&iter->second.cv);
//    lock_needs_free[lid] = true;
    if(need_release > 0) {
      lock_needs_free[lid] = true;
      while(iter->second.num <= 0) {
        pthread_mutex_unlock(&global_lock);
        pthread_mutex_lock(&global_lock);
      }
      pthread_cond_signal(&iter->second.cv);
    } else {
      while(iter->second.num <= 0) {
        pthread_mutex_unlock(&global_lock);
        pthread_mutex_lock(&global_lock);
      }
      pthread_cond_signal(&iter->second.cv);
    }
  } else {
    std::cout<<"error here.\n";
    ret = 1;
  }
  pthread_mutex_unlock(&global_lock);
  return ret;
}



