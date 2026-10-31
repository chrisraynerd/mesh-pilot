#pragma once
#include <stdint.h>
#include <stddef.h>
namespace protocol {
struct MessageView { bool direct; uint8_t channel; size_t textOffset; uint32_t timestamp; };
inline uint32_t read32(const uint8_t* p) { return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24); }
inline void write32(uint8_t* p,uint32_t v) { for(int i=0;i<4;i++) p[i]=v>>(8*i); }
inline bool message(const uint8_t* p,size_t n,MessageView& m) {
  if(!n) return false;
  bool direct=p[0]==7||p[0]==16;
  bool group=p[0]==8||p[0]==17;
  if(!direct&&!group) return false;
  size_t base=(p[0]==16||p[0]==17)?4:1;
  size_t meta=direct?8:3;
  if(n<base+meta+4) return false;
  uint8_t type=p[base+(direct?7:2)];
  // Plain=0; signed plain=2 includes a four-byte signature prefix.
  if(type!=0 && !(direct&&type==2)) return false;
  size_t offset=base+meta+4+(type==2?4:0);
  if(n<offset) return false;
  m={direct,group?p[base]:uint8_t(255),offset,read32(p+base+meta)};
  return true;
}
inline bool selfTest() {
  MessageView m;
  const uint8_t v3[]={17,0,0,0,2,255,0,0x78,0x56,0x34,0x12,'x'};
  const uint8_t old[]={8,3,0,0,1,0,0,0,'y'};
  const uint8_t dm[]={16,0,0,0,1,2,3,4,5,6,255,2,1,0,0,0,9,9,9,9,'z'};
  return message(v3,sizeof(v3),m)&&m.channel==2&&m.textOffset==11&&m.timestamp==0x12345678
      &&!message(v3,10,m)&&message(old,sizeof(old),m)&&m.textOffset==8
      &&message(dm,sizeof(dm),m)&&m.direct&&m.textOffset==20&&!message(dm,19,m);
}
}
