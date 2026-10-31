#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <vector>
#include "terminal_tools.h"

namespace field {
constexpr size_t MAX_SENDS=80, MAX_TESTS=12, MAX_REPLIES=32;
enum class SendState:uint8_t { REQUESTING, ACCEPTED, REPEAT, ACK, NO_REPEAT, NO_ACK, REJECTED, UNCONFIRMED, INTERRUPTED, TRACKING_UNAVAILABLE };
struct SendResult {
  uint32_t timestamp=0,updated=0;
  SendState state=SendState::REQUESTING;
  bool direct=false,accepted=false,approximate=false;
  uint8_t channel=0,peer[6]{};
  char target[33]{};
};
inline bool positive(SendState s){return s==SendState::ACK||s==SendState::REPEAT;}
inline void transition(SendResult& r,SendState next,uint32_t now){
  if(next==SendState::ACCEPTED)r.accepted=true;
  // A late local OK or deadline must never erase a matched RF response.
  if(positive(r.state))return;
  if(next==SendState::INTERRUPTED&&r.state!=SendState::REQUESTING&&r.state!=SendState::ACCEPTED)return;
  r.state=next;r.updated=now;
}
inline bool recover(SendResult& r){if(r.state!=SendState::REQUESTING&&r.state!=SendState::ACCEPTED)return false;r.state=SendState::INTERRUPTED;return true;}
enum class TestState:uint8_t { QUEUED, LISTENING, COMPLETE, REJECTED, WRITE_FAILED, INTERRUPTED };
struct Signal { uint8_t key[32]{};int8_t outbound=0,inbound=0,rssi=0; };
struct Test {
  uint32_t id=0,started=0,finished=0;
  TestState state=TestState::QUEUED;
  bool accepted=false,full=false,approximate=false;
  char place[25]{};
  std::vector<Signal> replies;
};
inline bool recover(Test& t){if(t.state!=TestState::QUEUED&&t.state!=TestState::LISTENING)return false;t.state=TestState::INTERRUPTED;return true;}
inline bool addReply(Test& t,const Signal& s){
  for(auto& old:t.replies)if(!memcmp(old.key,s.key,32)){old=s;return true;}
  if(t.replies.size()==MAX_REPLIES){t.full=true;return false;}t.replies.push_back(s);return true;
}

// Explicit byte layouts avoid compiler padding and never contain channel keys.
inline void put32(uint8_t* p,uint32_t n){for(int i=0;i<4;i++)p[i]=uint8_t(n>>(8*i));}
inline void encode(const SendResult& r,uint8_t (&p)[51]){
  memset(p,0,sizeof(p));put32(p,r.timestamp);put32(p+4,r.updated);p[8]=uint8_t(r.state);p[9]=uint8_t(r.direct)|(r.approximate?2:0);p[10]=r.accepted;p[11]=r.channel;memcpy(p+12,r.peer,6);memcpy(p+18,r.target,33);
}
inline bool decode(const uint8_t* p,size_t n,SendResult& r){
  if(n!=51||!protocol::read32(p)||p[8]>uint8_t(SendState::TRACKING_UNAVAILABLE)||p[9]>3||p[10]>1||p[50]||(!(p[9]&1)&&p[11]>=40))return false;
  r={};r.timestamp=protocol::read32(p);r.updated=protocol::read32(p+4);r.state=SendState(p[8]);r.direct=p[9]&1;r.approximate=p[9]&2;r.accepted=p[10];r.channel=p[11];memcpy(r.peer,p+12,6);memcpy(r.target,p+18,33);return true;
}
inline void encode(const Test& t,uint8_t (&p)[42]){
  memset(p,0,sizeof(p));put32(p,t.id);put32(p+4,t.started);put32(p+8,t.finished);p[12]=uint8_t(t.state);p[13]=t.accepted;p[14]=t.full;p[15]=t.replies.size();memcpy(p+16,t.place,25);p[41]=t.approximate;
}
inline bool decode(const uint8_t* p,size_t n,Test& t,uint8_t& count){
  if(n!=42||!protocol::read32(p)||p[12]>uint8_t(TestState::INTERRUPTED)||p[13]>1||p[14]>1||p[15]>MAX_REPLIES||!p[16]||p[40]||p[41]>1)return false;
  t={};t.id=protocol::read32(p);t.started=protocol::read32(p+4);t.finished=protocol::read32(p+8);t.state=TestState(p[12]);t.accepted=p[13];t.full=p[14];t.approximate=p[41];count=p[15];memcpy(t.place,p+16,25);return true;
}
inline uint32_t hash(const uint8_t* p,size_t n,uint32_t h=2166136261u){for(size_t i=0;i<n;i++){h^=p[i];h*=16777619u;}return h;}
inline bool selfTest(){
  SendResult r;r.timestamp=42;r.direct=true;r.channel=255;r.peer[0]=9;strcpy(r.target,"Person");
  uint8_t bytes[51];encode(r,bytes);SendResult other;
  if(!decode(bytes,51,other)||other.timestamp!=42||other.peer[0]!=9||strcmp(other.target,"Person"))return false;
  bytes[9]=4;if(decode(bytes,51,other))return false;encode(r,bytes);bytes[50]=1;if(decode(bytes,51,other))return false;
  transition(r,SendState::ACK,43);transition(r,SendState::ACCEPTED,44);transition(r,SendState::NO_ACK,45);if(r.state!=SendState::ACK||!r.accepted||recover(r))return false;
  r.state=SendState::ACCEPTED;if(!recover(r)||r.state!=SendState::INTERRUPTED)return false;
  r.state=SendState::NO_ACK;transition(r,SendState::ACK,46);if(r.state!=SendState::ACK)return false;
  Test t;t.id=3;t.started=50;strcpy(t.place,"Garden");Signal signal;signal.key[0]=1;signal.outbound=-12;signal.inbound=-8;signal.rssi=-105;addReply(t,signal);signal.rssi=-104;addReply(t,signal);
  if(t.replies.size()!=1||t.replies[0].rssi!=-104)return false;
  uint8_t meta[42],count;encode(t,meta);Test loaded;if(!decode(meta,42,loaded,count)||count!=1||strcmp(loaded.place,"Garden"))return false;
  meta[15]=33;if(decode(meta,42,loaded,count))return false;
  for(int i=2;i<=33;i++){signal.key[0]=i;addReply(t,signal);}if(t.replies.size()!=32||!t.full)return false;
  if(!recover(t)||t.state!=TestState::INTERRUPTED||recover(t))return false;
  const uint8_t word[]={'h','e','l','l','o'};return hash(word,5)==0x4f9f2cab;
}
}
