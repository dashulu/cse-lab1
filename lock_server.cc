// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
	VERIFY(pthread_mutex_init(&mlocks_mutex, NULL) == 0);
	VERIFY(pthread_cond_init(&mlocks_cv, NULL) == 0);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

/* We hold the convention that the thread who has got the lock won't call acquire twice */
lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
	pthread_mutex_lock(&mlocks_mutex);
	lock_protocol::status ret = lock_protocol::OK;
	std::map<lock_protocol::lockid_t, int>::iterator it;
	it = mlocks.find(lid);
	r = 0;
	if (it == mlocks.end())
		mlocks[lid] = clt;
	else {
		// VERIFY(mlocks[lid] != clt);
		while (mlocks[lid] != -1)
			pthread_cond_wait(&mlocks_cv, &mlocks_mutex);
		mlocks[lid] = clt;
	}

	// printf("acquire request from clt %d for lock %llu\n", clt, lid);
	pthread_mutex_unlock(&mlocks_mutex);
	return ret;
}

/* We hold the convention that only the thread who has got the lock call release */
lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
	pthread_mutex_lock(&mlocks_mutex);
	lock_protocol::status ret = lock_protocol::OK;
	std::map<lock_protocol::lockid_t, int>::iterator it;
	it = mlocks.find(lid);
	r = 0;
	mlocks[lid] = -1;
	pthread_cond_signal(&mlocks_cv);
	// if (it != mlocks.end()) {
	// 	VERIFY(mlocks[lid] == clt);
	// 	mlocks[lid] = -1;
	// 	pthread_cond_signal(&mlocks_cv);
	// }
	// printf("release request from clt %d for lock %llu\n", clt, lid);
	pthread_mutex_unlock(&mlocks_mutex);
	return ret;
}