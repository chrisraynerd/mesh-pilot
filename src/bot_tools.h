#pragma once
#include "protocol.h"
#include <string>
#include <ctype.h>
namespace bot {
struct Trigger { std::string sender,body; uint32_t identity=0; int hops=-1; bool hasSnr=false; int8_t snr=0; };
inline uint32_t hash(const uint8_t* p,size_t n,uint32_t h=2166136261u){for(size_t i=0;i<n;i++)h=(h^p[i])*16777619u;return h;}
inline bool parseAny(const uint8_t* p,size_t n,Trigger& out){
  protocol::MessageView v;if(!protocol::message(p,n,v)||v.direct)return false;
  std::string text(reinterpret_cast<const char*>(p+v.textOffset),n-v.textOffset);
  if(text.find('\0')!=std::string::npos)return false;
  size_t sep=text.find(": ");if(sep==std::string::npos||!sep||sep>64)return false;
  std::string body=text.substr(sep+2);size_t first=body.find_first_not_of(" \r\n\t"),last=body.find_last_not_of(" \r\n\t");
  if(first==std::string::npos)return false;body=body.substr(first,last-first+1);
  for(auto& c:body)c=tolower(static_cast<unsigned char>(c));out.body=body;
  out.sender=text.substr(0,sep);size_t base=p[0]==17?4:1;uint8_t path=p[base+1];
  out.hops=path==255||(path>>6)==3?-1:int(path&63);
  out.hasSnr=p[0]==17;out.snr=out.hasSnr?int8_t(p[1]):0;
  // Identity excludes reception metadata: repeats can have different SNR/path.
  out.identity=hash(p+base,1);out.identity=hash(p+base+3,n-base-3,out.identity);return true;
}
inline bool parse(const uint8_t* p,size_t n,Trigger& out){return parseAny(p,n,out)&&out.body=="test";}
inline bool keyword(const std::string& word){if(word.empty()||word.size()>16||word=="help")return false;for(char c:word)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;return true;}
inline int command(const Trigger& t,uint8_t flags,const char* test,const char* a,const char* b){
  if((flags&1)&&t.body==test)return 0;if((flags&2)&&t.body=="help")return 1;
  if((flags&4)&&t.body==a)return 2;if((flags&8)&&t.body==b)return 3;return -1;
}
inline bool validInterval(uint8_t seconds){return seconds==0||seconds==5||seconds==10||seconds==15||seconds==30||seconds==60;}
struct Limits {
  struct Seen {uint32_t id=0;uint8_t channel=0;bool used=false;} seen[24];
  uint32_t sentAt[40]={};bool haveSent[40]={};uint32_t count=0;uint8_t next=0;
  bool duplicate(uint32_t id,uint8_t channel)const{for(const auto& s:seen)if(s.used&&s.id==id&&s.channel==channel)return true;return false;}
  uint32_t remaining(uint32_t now,uint8_t channel,uint8_t seconds)const{
    if(channel>=40||!haveSent[channel]||!seconds)return 0;uint32_t gap=uint32_t(seconds)*1000,elapsed=now-sentAt[channel];return elapsed>=gap?0:gap-elapsed;
  }
  void remember(uint32_t id,uint8_t channel){seen[next].id=id;seen[next].channel=channel;seen[next].used=true;next=(next+1)%24;}
  void dispatched(uint32_t now,uint8_t channel){if(channel<40){sentAt[channel]=now;haveSent[channel]=true;count++;}}
};
inline bool selfTest(){
  uint8_t frame[]={17,248,0,0,2,0x43,0,1,0,0,0,'A',':',' ','T','e','S','t'};Trigger t;
  if(!parse(frame,sizeof(frame),t)||t.hops!=3||t.snr!=-8||t.sender!="A")return false;
  uint32_t id=t.identity;frame[1]=4;frame[5]=1;if(!parse(frame,sizeof(frame),t)||t.identity!=id)return false;
  frame[17]='!';if(parse(frame,sizeof(frame),t))return false;frame[17]='t';frame[5]=255;
  if(!parse(frame,sizeof(frame),t)||t.hops!=-1)return false;
  t.body="meeting";if(command(t,7,"test","meeting","")!=2||command(t,3,"test","meeting","")!=-1||!keyword("meeting")||keyword("help")||keyword("two words")||keyword("[bot]"))return false;
  Limits limits;if(limits.remaining(0,2,30)||limits.duplicate(1,2))return false;
  limits.remember(1,2);limits.dispatched(0,2);
  if(!limits.duplicate(1,2)||limits.duplicate(2,2)||limits.duplicate(1,3))return false;
  if(limits.remaining(29999,2,30)!=1||limits.remaining(30000,2,30)||limits.remaining(1,2,0)||limits.remaining(1,3,30))return false;
  // Repeating the same body with a new source timestamp is a new request.
  frame[7]++;if(!parse(frame,sizeof(frame),t)||t.identity==id||limits.duplicate(t.identity,2))return false;
  for(int i=0;i<20;i++){limits.remember(i+2,2);limits.dispatched(i*5000+1,2);if(limits.remaining(i*5000+5001,2,5))return false;}
  Limits wrap;wrap.dispatched(0xfffffff0u,1);return wrap.remaining(20,1,5)==4964&&!wrap.remaining(6000,1,5)&&validInterval(30)&&validInterval(0)&&!validInterval(255);
}
}
