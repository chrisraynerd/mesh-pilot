#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace discovery {
constexpr uint32_t WINDOW_MS=30000, COOLDOWN_MS=60000;
struct Result { uint8_t key[32]; int8_t outboundSnrQ, inboundSnrQ, rssi; uint32_t heardAt; };
// Companion control push: metadata[4], flags, request SNR, tag[4], full key[32].
inline void request(uint8_t (&out)[11],uint32_t tag) {
  memset(out,0,sizeof(out));out[0]=55;out[1]=0x80;out[2]=1<<2;
  for(int i=0;i<4;i++)out[3+i]=uint8_t(tag>>(8*i));
}
inline bool parse(const uint8_t* p,size_t n,uint32_t tag,Result& out) {
  if(n!=42||p[0]!=0x8e||p[3]!=0||p[4]!=0x92||!tag)return false;
  uint32_t actual=uint32_t(p[6])|(uint32_t(p[7])<<8)|(uint32_t(p[8])<<16)|(uint32_t(p[9])<<24);
  if(actual!=tag)return false;
  uint8_t any=0;for(int i=10;i<42;i++)any|=p[i];if(!any)return false;
  memcpy(out.key,p+10,32);out.outboundSnrQ=int8_t(p[5]);out.inboundSnrQ=int8_t(p[1]);out.rssi=int8_t(p[2]);return true;
}
struct Scan {
  Result results[32]{};
  uint8_t count=0;bool active=false,full=false;uint32_t tag=0,started=0;
  void begin(uint32_t token,uint32_t now){count=0;full=false;tag=token;started=now;active=token!=0;}
  bool expired(uint32_t now)const{return uint32_t(now-started)>=WINDOW_MS;}
  bool receive(const uint8_t* p,size_t n,uint32_t now,const uint8_t* self=nullptr){
    Result r{};if(!active||expired(now)||!parse(p,n,tag,r)||(self&&!memcmp(self,r.key,32)))return false;
    r.heardAt=now;
    for(int i=0;i<count;i++)if(!memcmp(results[i].key,r.key,32)){results[i]=r;return true;}
    if(count==32){full=true;return false;}results[count++]=r;return true;
  }
};
inline bool selfTest(){
  uint8_t q[11];request(q,0x12345678);
  if(q[0]!=55||q[1]!=0x80||q[2]!=4||q[3]!=0x78||q[6]!=0x12||q[10])return false;
  uint8_t p[42]={0x8e,uint8_t(-8),uint8_t(-99),0,0x92,uint8_t(-12),0x78,0x56,0x34,0x12};p[10]=1;
  Scan s;s.begin(0x12345678,0xfffffff0);
  if(!s.receive(p,42,5)||s.count!=1||s.results[0].outboundSnrQ!=-12||s.results[0].inboundSnrQ!=-8||s.results[0].rssi!=-99)return false;
  if(!s.receive(p,42,8)||s.count!=1||s.results[0].heardAt!=8)return false;
  p[6]++;if(s.receive(p,42,10))return false;p[6]--;
  p[3]=1;if(s.receive(p,42,10))return false;p[3]=0;
  p[4]=0x91;if(s.receive(p,42,10))return false;p[4]=0x92;
  if(s.receive(p,14,10)||s.receive(p,42,30000)||s.receive(p,42,10,p+10))return false;
  for(int i=2;i<=33;i++){p[10]=i;s.receive(p,42,12);}
  if(s.count!=32||!s.full)return false;
  s.active=false;if(s.receive(p,42,13))return false;
  s.begin(99,100);return s.count==0&&!s.full&&!s.receive(p,42,101);
}
}
