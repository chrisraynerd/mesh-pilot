#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "protocol.h"
namespace diagnostics {
struct Contact { uint8_t key[32]; uint8_t type,path,flags; uint8_t route[64],tail[12]; uint16_t length; char name[33]; uint32_t advert; uint32_t seenAt; bool seen,saved,routeStale; };
inline bool contact(const uint8_t* p,size_t n,Contact& c) {
  if(n<136||(p[0]!=3&&p[0]!=0x8a))return false;
  memcpy(c.key,p+1,32);c.type=p[33];c.path=p[35];c.flags=p[34];
  c.length=n>148?148:n;memcpy(c.route,p+36,64);memset(c.tail,0,12);if(c.length>136)memcpy(c.tail,p+136,c.length-136);
  memcpy(c.name,p+100,32);c.name[32]=0;c.advert=protocol::read32(p+132);return true;
}
inline void encodeContact(const Contact& c,uint8_t* p){
  memset(p,0,148);p[0]=3;memcpy(p+1,c.key,32);p[33]=c.type;p[34]=c.flags;p[35]=c.path;memcpy(p+36,c.route,64);memcpy(p+100,c.name,32);protocol::write32(p+132,c.advert);memcpy(p+136,c.tail,12);
}
inline bool sameReply(const uint8_t* p,size_t n,const uint8_t* key,uint8_t code) {
  return n>=8&&p[0]==code&&memcmp(p+2,key,6)==0;
}
inline bool pathBytes(uint8_t encoded,size_t& bytes) {
  size_t width=(encoded>>6)+1,count=encoded&63;bytes=width*count;
  return width<=3&&bytes<=64;
}
inline bool paths(const uint8_t* p,size_t n,uint8_t& outgoing,uint8_t& incoming) {
  if(n<10||p[0]!=0x8d)return false;
  size_t bytes;if(!pathBytes(p[8],bytes)||9+bytes>=n)return false;
  outgoing=p[8]&63;size_t offset=9+bytes;
  if(!pathBytes(p[offset],bytes)||offset+1+bytes>n)return false;
  incoming=p[offset]&63;return true;
}
inline bool selfTest() {
  uint8_t key[32]={1,2,3,4,5,6};
  uint8_t route[]={0x8d,0,1,2,3,4,5,6,0x42,1,2,3,4,0};
  uint8_t out=0,in=1;size_t bytes=0;
  uint8_t c[148]={3};memcpy(c+1,key,32);c[33]=2;c[35]=255;memcpy(c+100,"Repeater",9);c[132]=42;
  Contact parsed{};uint8_t rebuilt[148];
  for(int i=36;i<100;i++)c[i]=i;for(int i=136;i<148;i++)c[i]=i;
  bool roundTrip=contact(c,sizeof(c),parsed);encodeContact(parsed,rebuilt);roundTrip=roundTrip&&!memcmp(c,rebuilt,148);
  parsed.path=255;parsed.flags^=1;encodeContact(parsed,rebuilt);roundTrip=roundTrip&&rebuilt[35]==255&&rebuilt[34]==1&&!memcmp(rebuilt+36,c+36,64)&&!memcmp(rebuilt+136,c+136,12);
  return roundTrip&&sizeof(Contact)<=164&&contact(c,sizeof(c),parsed)&&parsed.type==2&&parsed.advert==42
    &&!contact(c,135,parsed)&&sameReply(route,sizeof(route),key,0x8d)
    &&!sameReply(route,7,key,0x8d)&&paths(route,sizeof(route),out,in)&&out==2&&in==0
    &&!paths(route,sizeof(route)-1,out,in)&&!pathBytes(0xc1,bytes)&&!pathBytes(0x7f,bytes);
}
}
