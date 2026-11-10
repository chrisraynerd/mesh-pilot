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
// Aliases are complete commands, never substring matches. One optional ! is accepted.
inline std::string normalized(std::string s){size_t a=s.find_first_not_of(" \r\n\t"),b=s.find_last_not_of(" \r\n\t");if(a==std::string::npos)return "";s=s.substr(a,b-a+1);for(auto& c:s)c=tolower(static_cast<unsigned char>(c));if(s[0]=='!')s.erase(0,1);return s;}
inline bool keyword(const std::string& word){if(word.empty()||word.size()>16)return false;for(char c:word)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;return true;}
inline bool aliases(const std::string& list){if(list.empty()||list.size()>16)return false;size_t a=0;while(true){size_t b=list.find(',',a);auto word=list.substr(a,b==std::string::npos?b:b-a);if(!keyword(word))return false;if(b==std::string::npos)return true;a=b+1;}}
inline bool matches(const std::string& body,const char* list){size_t a=0;std::string words=list;while(a<words.size()){size_t b=words.find(',',a);if(body==words.substr(a,b==std::string::npos?b:b-a))return true;if(b==std::string::npos)break;a=b+1;}return false;}
inline bool overlap(const char* a,const char* b){std::string words=a;size_t i=0;while(i<words.size()){size_t end=words.find(',',i);if(matches(words.substr(i,end==std::string::npos?end:end-i),b))return true;if(end==std::string::npos)break;i=end+1;}return false;}
inline const char* fixedAliases(int kind){switch(kind){case 1:return "help,commands";case 4:return "wx,weather";case 5:return "ping";case 6:return "pong";case 7:return "status";case 8:return "battery,batt";case 9:return "uptime";case 10:return "version,ver";default:return "";}}
inline int command(const Trigger& t,uint16_t flags,const char* test,const char* a,const char* b){
  auto body=normalized(t.body);
  if((flags&1)&&matches(body,test))return 0;
  // Existing custom names keep precedence over newly introduced built-in aliases.
  if((flags&4)&&matches(body,a))return 2;if((flags&8)&&matches(body,b))return 3;
  for(int i=1;i<=10;i++)if(i!=2&&i!=3&&i!=4&&(flags&(1u<<i))&&matches(body,fixedAliases(i)))return i;return -1;
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
  t.body="meeting";if(command(t,7,"test","meeting","")!=2||command(t,3,"test","meeting","")!=-1||!keyword("meeting")||!keyword("help")||keyword("two words")||keyword("[bot]"))return false;
  if(!aliases("test,testing")||aliases("test,,testing")||aliases("test,")||aliases("two words"))return false;
  for(const char* word:{"test","TEST","testing","TESTING","!Test","!testing"}){t.body=word;if(command(t,1,"test,testing","","")!=0)return false;}
  for(const char* word:{"contest","testing now","!!test","[bot] test","! test"}){t.body=word;if(command(t,2047,"test,testing","info,about","")!=-1)return false;}
  for(int kind=5;kind<=10;kind++){std::string word=fixedAliases(kind);t.body="!"+word.substr(0,word.find(','));if(command(t,1u<<kind,"test,testing","","")!=kind||command(t,0,"test,testing","","")!=-1)return false;}
  t.body="ABOUT";if(command(t,4,"test,testing","info,about","")!=2||!overlap("test,testing","testing,foo")||overlap("test","testing"))return false;
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
