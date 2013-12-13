// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
	pthread_mutex_init(&lock, NULL);
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &need_release)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::map<lock_protocol::lockid_t, server_lock_record>::iterator iter;
switch_start:
  pthread_mutex_lock(&lock);
  iter = states.find(lid);
  if(iter != states.end()) {

  	switch(iter->second.state) {
  		case lock_protocol::Server_free:
  			iter->second.state = lock_protocol::Server_locked;
  			iter->second.id = id;
  			need_release = 0;
        ret = 0;
  			break;
  		case lock_protocol::Server_locked:
 /* 			if(iter->second.waiting_list.size() > 0) {
  				iter->second.waiting_list.push_back(id);
  				ret = lock_protocol::RETRY;
  			} else {*/
//          iter->second.state = lock_protocol::Server_revoke;
        if(true) {
	  			handle h(iter->second.id);
	  			rpcc *c = h.safebind();
	  			if(c) {
	  				int tt = 0;
            std::string tmp = iter->second.id;
	  				pthread_mutex_unlock(&lock);
	  				ret = c->call(rlock_protocol::revoke, lid, tt); 
	  				pthread_mutex_lock(&lock);
            std::cout<<"ret from revoke:"<<ret<<" id:"<<iter->second.id<<std::endl;
	  				if(ret == 0) {
              if(tmp == iter->second.id) {
  	  					ret = lock_protocol::OK;
  	  					iter->second.id = id;
  	  					need_release = iter->second.waiting_list.size() > 0 ? 1 : 0;
              } else {
                pthread_mutex_unlock(&lock);
                goto switch_start;
              }
	  				} else if(ret == 1) {
              if(iter->second.state == lock_protocol::Server_locked && tmp == iter->second.id) {
  	  					iter->second.waiting_list.push_back(id);
  	  					ret = lock_protocol::RETRY;
              } else if(iter->second.state != lock_protocol::Server_locked){
                ret = lock_protocol::OK;
                iter->second.id = id;
                iter->second.state = lock_protocol::Server_locked;
              } else {
                pthread_mutex_unlock(&lock);
                goto switch_start;
              }
	  				} else if(ret == 2) {
              pthread_mutex_unlock(&lock);
	  					goto switch_start;
	  				} else {
	  					ret = lock_protocol::UNKNOWNERROR;
	  				}
	  			} else {
	  				tprintf("error binding %s lid:%lu\n", iter->second.id.c_str(), lid);
	  			}
	  		}
  			break;
      case lock_protocol::Server_revoke:
        iter->second.waiting_list.push_back(id);
        ret = lock_protocol::RETRY;
        break;
  		default:
  			ret = lock_protocol::UNKNOWNERROR;
  			tprintf("error state.\n");
  			break;
  	}
  } else {
/*  	server_lock_record tmp;
  	tmp.state = lock_protocol::Server_locked;
  	tmp.id = id;*/
  	states[lid].state = lock_protocol::Server_locked;
  	states[lid].id = id;
  /*	if(states[lid].state == lock_protocol::Server_locked) {
  		tprintf(" lock lid: %lu state: %d server_locked:%d\n", lid, tmp.state, lock_protocol::Server_locked);
  	} else { 
  		tprintf("hello world\n");
  	}*/
  }
  pthread_mutex_unlock(&lock);
  switch(ret) {
  	case lock_protocol::OK:
  	//	tprintf("id:%s, lid:%ld ret OK\n", id.c_str(), lid);
      std::cout<<"server acquire id "<<id<<" lid:"<<lid<<" assign lock OK\n";
  		break;
  	case lock_protocol::RETRY:
  		std::cout<<"server acquire id "<<id<<" lid:"<<lid<<" assign lock RETRY\n";
  		break;
  	case lock_protocol::UNKNOWNERROR:
  		std::cout<<"server acquire id "<<id<<" lid:"<<lid<<" assign lock UNKNOWNERROR\n";
  		break;
  }
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::map<lock_protocol::lockid_t, server_lock_record>::iterator iter;
  int need_release = 0;
  std::cout<<"server release called: lid "<<lid<<", id:"<<id<<std::endl;
  pthread_mutex_lock(&lock);
  iter = states.find(lid);
  if(iter != states.end()) {
  	if(iter->second.waiting_list.size() == 0) {
  		iter->second.state = lock_protocol::Server_free;
  	} else {
  		tprintf("wake up other thread in server\n");
  		iter->second.state = lock_protocol::Server_locked;
  		do {
	  		iter->second.id = iter->second.waiting_list.front();
	  		iter->second.waiting_list.pop_front();
	  		need_release = iter->second.waiting_list.size() > 0 ? 1 : 0;
   //     need_release = 1;
	  		handle h(iter->second.id);
  			rpcc *c = h.safebind();
  			if(c) {
          std::cout<<"server wake up client:"<<iter->second.id<<std::endl;
  				pthread_mutex_unlock(&lock);
  				int t = 0;
  				tprintf("need_release:%d\n", need_release);
  				ret = c->call(rlock_protocol::retry, lid*4 + need_release,  t); 
  				pthread_mutex_lock(&lock);
  				if(ret == 0) {
  					ret = lock_protocol::OK;
  					break;
  				}
  			}
  		} while(iter->second.waiting_list.size() > 0);
  		if(ret != 0) {
  			iter->second.state = lock_protocol::Server_free;
  		}
  	}
  } else {
  	ret = lock_protocol::UNKNOWNERROR;
  }
  pthread_mutex_unlock(&lock);
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

