#pragma once
#include <stdint.h>
#include <stddef.h>
#include "protocol.h"
namespace terminal_tools {
inline uint32_t retryDelay(uint8_t attempt) {
  return attempt>=4?60000u:(5000u<<attempt);
}
inline bool battery(const uint8_t* data,size_t length,uint16_t& mv,uint32_t& used,uint32_t& total) {
  if(length<3||data[0]!=12)return false;
  mv=uint16_t(data[1])|(uint16_t(data[2])<<8);used=total=0;
  if(length>=11){used=protocol::read32(data+3);total=protocol::read32(data+7);}
  // Zero/out-of-range readings mean the board has no useful measurement.
  if(mv<1000||mv>6000)mv=0;return true;
}
inline uint32_t checksum(const uint8_t* data,size_t length) {
  uint32_t hash=2166136261u;for(size_t i=0;i<length;i++){hash^=data[i];hash*=16777619u;}return hash;
}
inline bool due(uint32_t now,uint32_t deadline){return int32_t(now-deadline)>=0;}
inline bool selfTest() {
  uint8_t frame[]={12,0x10,0x0e,5,0,0,0,20,0,0,0};uint16_t mv;uint32_t used,total;
  if(!battery(frame,sizeof(frame),mv,used,total)||mv!=3600||used!=5||total!=20||battery(frame,2,mv,used,total))return false;
  frame[1]=frame[2]=0;if(!battery(frame,3,mv,used,total)||mv||used||total)return false;
  const uint8_t word[]={'h','e','l','l','o'};
  return checksum(word,5)==0x4f9f2cab&&retryDelay(0)==5000&&retryDelay(3)==40000&&retryDelay(4)==60000&&retryDelay(255)==60000
    &&!due(0xfffffff0,0x10)&&due(0x20,0x10);
}
}
