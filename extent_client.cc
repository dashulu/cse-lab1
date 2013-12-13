// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

// a demo to show how to use RPC
extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;

  std::list<Attr>::iterator iter;
  for(iter = attr_cache.begin();iter != attr_cache.end();iter++) {
    if(iter->i_num == eid) {
      attr.type = iter->type;
      attr.size = iter->size;
      attr.mtime = iter->mtime;
      attr.atime = iter->atime;
      attr.ctime = iter->ctime;
      return ret;
    }
  }

  ret = cl->call(extent_protocol::getattr, eid, attr);
  if(ret == extent_protocol::OK) {
    Attr tmp;
    tmp.i_num = eid;
    tmp.type = attr.type;
    tmp.size = attr.size;
    tmp.atime = attr.atime;
    tmp.ctime = attr.ctime;
    tmp.mtime = attr.mtime;
    attr_cache.push_front(tmp);
  }
  return ret;
}

extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  ret = cl->call(extent_protocol::create, type, id);
  if(ret == extent_protocol::OK) {
    Attr tmp;
    tmp.i_num = id;
    tmp.size = 0;
    tmp.type = type;
/*    tmp.atime = attr.atime;
    tmp.ctime = attr.ctime;
    tmp.mtime = attr.mtime;*/
    attr_cache.push_front(tmp);
  }
  return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  std::list<cache>::iterator iter;
  for(iter = caches.begin();iter != caches.end();iter++) {
    if(iter->i_num == eid) {
      buf = iter->content;
      return ret;
    }
  }

  ret = cl->call(extent_protocol::get, eid, buf);

  if(ret == extent_protocol::OK) {
    cache tmp;
    tmp.content = buf;
    tmp.dirty = false;
    tmp.i_num = eid;
    caches.push_front(tmp);
  }
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  int r;
  std::list<cache>::iterator iter;
  for(iter = caches.begin();iter != caches.end();iter++) {
    if(iter->i_num == eid) {
      iter->content = buf;
      iter->dirty = true;

      std::list<Attr>::iterator iter1;
      for(iter1 = attr_cache.begin();iter1 != attr_cache.end();iter1++) {
        if(iter1->i_num == eid) {
          iter1->size = buf.size();
          break;
        }
      }

      return ret;
    }
  }

  cache tmp;
  tmp.content = buf;
  tmp.dirty = true;
  tmp.i_num = eid;
  caches.push_front(tmp);

//  ret = cl->call(extent_protocol::put, eid, buf, r);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  int r;

  std::list<cache>::iterator iter;
  for(iter = caches.begin();iter != caches.end();iter++) {
    if(iter->i_num == eid) {
      caches.erase(iter);
      break;
    }
  }

  std::list<Attr>::iterator iter1;
  for(iter1 = attr_cache.begin();iter1 != attr_cache.end();iter1++) {
    if(iter1->i_num == eid) {
      attr_cache.erase(iter1);
      break;
    }
  }

  ret = cl->call(extent_protocol::remove, eid, r);
  return ret;
}


