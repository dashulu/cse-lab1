// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
	int r = pthread_cond_init(&cv, NULL);
	pthread_mutex_init(&mp, NULL);
	if(r == EINVAL) {
		printf("einval\n");
		exit(1);
	} else if(r == ENOMEM) {
		printf("enomem\n");
		exit(1);
	}
}

lock_protocol::status
lock_server::stat(unsigned int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %u\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status 
lock_server::acquire(unsigned int clt, lock_protocol::lockid_t lid, int &r) 
{
	lock_protocol::status ret = lock_protocol::OK;
	pthread_mutex_lock(&mp);
	std::map<lock_protocol::lockid_t, std::pair<unsigned int, bool> >::iterator iter;
	while(1) {

		if((iter = locks.find(lid)) == locks.end()) {
			std::pair<unsigned int, bool> foo (clt, true);
			std::pair<lock_protocol::lockid_t, std::pair<unsigned int, bool> > foo1 (lid, foo);
			locks.insert(foo1);
			printf("acquire request from clt %u, lid:%u state: new lock\n", clt, lid);
			break;
		} else if(iter->second.second) {
			printf("acquire request from clt %u, lid:%u state: lock\n", clt, lid);

			pthread_cond_wait(&cv, &mp);
		} else {
			printf("acquire request from clt %u, lid:%u state: can lock\n", clt, lid);
			iter->second.second = true;
			iter->second.first = clt;
			break;
		}
	}
	pthread_mutex_unlock(&mp);
	printf("acquire request from clt %u end\n", clt);
	nacquire++;
	return ret;
}

lock_protocol::status 
lock_server::release(unsigned int clt, lock_protocol::lockid_t lid, int &r) 
{
	lock_protocol::status ret = lock_protocol::OK;
	printf("release request from clt %u\n", clt);
	pthread_mutex_lock(&mp);
	std::map<lock_protocol::lockid_t, std::pair<unsigned int, bool> >::iterator iter;
	while(1) {
		if((iter = locks.find(lid)) == locks.end()) {
			r = lock_protocol::lock_not_found;
			printf("release request from clt %u, lid:%u state: not found\n", clt, lid);
			break;
		} else if(iter->second.second) {
		/*	if(iter->second.first != clt) {
				r = lock_protocol::lock_false_release;
				printf("release request from clt %u, lid:%u state: not belong\n", clt, lid);
				//ret = RETRY;
			} else {
		*/		r = lock_protocol::ok;
				iter->second.second = false;
				printf("release request from clt %u, lid:%u state: release\n", clt, lid);
				pthread_cond_broadcast(&cv);
		//	}
			break;
		} else {
			r = lock_protocol::lock_is_free;
			printf("release request from clt %u, lid:%u state: is free \n", clt, lid);
			break;
		}
	}
	pthread_mutex_unlock(&mp);
	return ret;
}


