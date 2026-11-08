#pragma once
#include "protocol.h"
#include <string>
#include <ctype.h>
namespace bot {
struct Trigger { std::string sender; uint32_t identity=0; int hops=-1; bool hasSnr=false; int8_t snr=0; };
inline uint32_t hash(const uint8_t* p,size_t n,uint32_t h=2166136261u){for(size_t i=0;i<n;i++)h=(h^p[i])*16777619u;return h;}
inline bool parse(const uint8_t* p,size_t n,Trigger& out){
  protocol::MessageView v;if(!protocol::message(p,n,v)||v.direct)return false;
  std::string text(reinterpret_cast<const char*>(p+v.textOffset),n-v.textOffset);
  if(text.find('\0')!=std::string::npos)return false;
  size_t sep=text.find(": ");if(sep==std::string::npos||!sep||sep>64)return false;
  std::string body=text.substr(sep+2);size_t first=body.find_first_not_of(" \r\n\t"),last=body.find_last_not_of(" \r\n\t");
  if(first==std::string::npos)return false;body=body.substr(first,last-first+1);
  if(body.size()!=4)return false;
  for(size_t i=0;i<4;i++)if(tolower(static_cast<unsigned char>(body[i]))!="test"[i])return false;
  out.sender=text.substr(0,sep);size_t base=p[0]==17?4:1;uint8_t path=p[base+1];
  out.hops=path==255||(path>>6)==3?-1:int(path&63);
  out.hasSnr=p[0]==17;out.snr=out.hasSnr?int8_t(p[1]):0;
  // Identity excludes reception metadata: repeats can have different SNR/path.
  out.identity=hash(p+base,1);out.identity=hash(p+base+3,n-base-3,out.identity);return true;
}
struct Limits {
  struct Seen {uint32_t id=0,sender=0,at=0;bool used=false;} seen[24];
  uint32_t sentAt[6]={};uint8_t count=0,next=0;
  bool allow(uint32_t now,uint32_t id,uint32_t sender)const{
    for(const auto& s:seen)if(s.used&&(s.id==id||(s.sender==sender&&uint32_t(now-s.at)<300000)))return false;
    if(count&&uint32_t(now-sentAt[(count-1)%6])<60000)return false;
    int recent=0;for(int i=0;i<6&&i<count;i++)if(uint32_t(now-sentAt[i])<3600000)recent++;
    return recent<6;
  }
  void reserve(uint32_t now,uint32_t id,uint32_t sender){
    seen[next].id=id;seen[next].sender=sender;seen[next].at=now;seen[next].used=true;next=(next+1)%24;
    // Keep chronological rolling timestamps without an overflowing counter.
    if(count==6){for(int i=1;i<6;i++)sentAt[i-1]=sentAt[i];sentAt[5]=now;}
    else sentAt[count++]=now;
  }
};
inline bool selfTest(){
  uint8_t frame[]={17,248,0,0,2,0x43,0,1,0,0,0,'A',':',' ','T','e','S','t'};Trigger t;
  if(!parse(frame,sizeof(frame),t)||t.hops!=3||t.snr!=-8||t.sender!="A")return false;
  uint32_t id=t.identity;frame[1]=4;frame[5]=1;if(!parse(frame,sizeof(frame),t)||t.identity!=id)return false;
  frame[17]='!';if(parse(frame,sizeof(frame),t))return false;frame[17]='t';frame[5]=255;
  if(!parse(frame,sizeof(frame),t)||t.hops!=-1)return false;
  Limits limits;if(!limits.allow(0,1,10))return false;limits.reserve(0,1,10);
  if(limits.allow(60000,1,11)||limits.allow(59999,2,11)||limits.allow(60000,2,10))return false;
  for(int i=1;i<6;i++){if(!limits.allow(i*60000,i+1,i+10))return false;limits.reserve(i*60000,i+1,i+10);}
  if(limits.allow(360000,20,30)||!limits.allow(3600000,20,30))return false;
  Limits wrap;wrap.reserve(0xfffffff0u,1,1);return !wrap.allow(20,2,2)&&wrap.allow(60000,2,2);
}
}
