// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "marshall.h"

extent::extent()
{
  id = 0;
}

extent::extent(extent_protocol::extentid_t eid)
{
  id = eid;
}

extent_client::extent_client()
{
  pthread_mutex_init(&m, NULL);
  printf("extent size: %d\n", extentset.size());
}

extent_client::~extent_client()
{
  pthread_mutex_destroy(&m);
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status r = extent_protocol::OK;
  printf("get %llu\n", eid);
  pthread_mutex_lock(&m);
  if (extentset.count(eid)) {
    extent &extent = extentset[eid];
    buf = extent.data;
  }
  else {
    // Should this ever happen?
    r = extent_protocol::NOENT;
  }
  pthread_mutex_unlock(&m);
  return r;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status r = extent_protocol::OK;
  printf("getattr %llu\n", eid);
  pthread_mutex_lock(&m);
  if (extentset.count(eid)) {
    extent &extent = extentset[eid];
    attr = extent.attr;
  }
  else {
    // Should this ever happen?
    r = extent_protocol::NOENT;
  }
  pthread_mutex_unlock(&m);
  return r;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status r = extent_protocol::OK;
  printf("put %llu\n", eid);
  pthread_mutex_lock(&m);
  if (!extentset.count(eid)) {
    extentset[eid] = extent(eid);
  }
  extent &extent = extentset[eid];
  extent.data = std::string(buf);
  time((time_t *) &extent.attr.atime);
  time((time_t *) &extent.attr.ctime);
  time((time_t *) &extent.attr.mtime);

  if (eid & 0x80000000) { // is file?
    extent.attr.size = buf.size();
  }
  else {
    extent.attr.size = 4096; // Directory size
  }

  pthread_mutex_unlock(&m);
  return r;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status r = extent_protocol::OK;
  pthread_mutex_lock(&m);
  extentset.erase(eid);
  pthread_mutex_unlock(&m);
  return r;
}

void 
extent_client::flush(extent_protocol::extentid_t eid)
{
  pthread_mutex_lock(&m);
  extentset.erase(eid);
  pthread_mutex_unlock(&m);
}

void 
extent_client::populate(extent_protocol::extentid_t eid, std::string data)
{
  printf("==> populate %llu data size: %d\n", eid, extentset.count(eid));
  unmarshall rep(data);
  pthread_mutex_lock(&m);
  assert(extentset.count(eid) == 0);
  bool ent;
  rep >> ent;
  if (ent) {
    extentset[eid] = extent(eid);
    extent &extent = extentset[eid];
    rep >> extent.data;
    rep >> extent.attr.atime;
    rep >> extent.attr.mtime;
    rep >> extent.attr.mtime;
    rep >> extent.attr.size;
    printf("==> populate %llu -> %s\n", eid, extent.data.c_str());
  }
  pthread_mutex_unlock(&m);
}

void 
extent_client::fetch(extent_protocol::extentid_t eid, std::string &data)
{
  printf("==> fetch %llu\n", eid);
  marshall rep;
  pthread_mutex_lock(&m);
  if (extentset.count(eid) != 0) {
    rep << true;
    extent &extent = extentset[eid];
    rep << extent.data;
    rep << extent.attr.atime;
    rep << extent.attr.mtime;
    rep << extent.attr.ctime;
    rep << extent.attr.size;
    printf("==> fetch %llu -> %s\n", eid, extent.data.c_str());
  }
  else {
    rep << false;
  }
  pthread_mutex_unlock(&m);
  data = rep.str();
  printf("==> fetch %llu -> %d\n", eid, data.size());
}

void
extent_client::evict(extent_protocol::extentid_t eid)
{
  if (extentset.count(eid) != 0) {
    printf("==> evict %llu\n", eid);
    extentset.erase(eid);
  }
}
void 
extent_client::dorelease(lock_protocol::lockid_t lid)
{
  this->flush(lid);
}

void 
extent_client::dopopulate(lock_protocol::lockid_t lid, std::string data)
{
  this->populate(lid, data);
}

void 
extent_client::dofetch(lock_protocol::lockid_t lid, std::string &data)
{
  this->fetch(lid, data);
}

void 
extent_client::doevict(lock_protocol::lockid_t lid)
{
  this->evict(lid);
}
