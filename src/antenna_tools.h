#pragma once
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <math.h>
namespace antenna {
constexpr int SAMPLES=3,SLOTS=8,SESSIONS=4;
enum class State:uint8_t { QUEUED, LISTENING, COMPLETE, REJECTED, WRITE_FAILED, INTERRUPTED, CANCELLED };
struct Sample { uint32_t epoch=0;State state=State::QUEUED;bool accepted=false,heard=false;int8_t outbound=0,inbound=0,rssi=0; };
struct Session {
  uint32_t id=0,epoch=0;uint8_t phase=0;
  bool approximate=false;int8_t power=0;
  char place[25]{},nameA[17]{},nameB[17]{},targetName[33]{},profile[65]{};
  uint8_t key[32]{},count[3]{};
  Sample samples[3][SLOTS]{};
};
inline bool valid(const Sample& s){return s.state==State::COMPLETE&&s.accepted;}
inline bool recover(Session& s){bool changed=false;for(int p=0;p<3;p++)for(int i=0;i<s.count[p];i++){auto& r=s.samples[p][i];if(r.state==State::QUEUED||r.state==State::LISTENING){r.state=State::INTERRUPTED;changed=true;}}return changed;}
struct Summary { int attempts=0,replies=0,excluded=0;float outbound=0,inbound=0,rssi=0; };
inline float median(int8_t* values,int n){if(!n)return 0;std::sort(values,values+n);return n%2?values[n/2]:(int(values[n/2-1])+int(values[n/2]))/2.0f;}
inline Summary summary(const Session& s,int phase){
  Summary out;int8_t tx[SLOTS]{},rx[SLOTS]{},rssi[SLOTS]{};
  for(int i=0;i<s.count[phase];i++){const auto& r=s.samples[phase][i];if(!valid(r)){if(r.state!=State::QUEUED&&r.state!=State::LISTENING)out.excluded++;continue;}out.attempts++;if(r.heard){tx[out.replies]=r.outbound;rx[out.replies]=r.inbound;rssi[out.replies]=r.rssi;out.replies++;}}
  out.outbound=median(tx,out.replies)/4.0f;out.inbound=median(rx,out.replies)/4.0f;out.rssi=median(rssi,out.replies);return out;
}
// This conservative small-trial rule describes the observed link, not antenna
// gain, a statistical confidence level, or an intrinsic antenna quality score.
enum class Verdict { INCOMPLETE, TOO_FEW, CONDITIONS_CHANGED, A_STRONGER, B_STRONGER, MIXED, SIMILAR };
inline Verdict verdict(const Session& s){
  auto a=summary(s,0),b=summary(s,1),again=summary(s,2);
  if(a.attempts<SAMPLES||b.attempts<SAMPLES||again.attempts<SAMPLES)return Verdict::INCOMPLETE;
  if(a.replies<2||b.replies<2||again.replies<2)return Verdict::TOO_FEW;
  if(a.replies!=again.replies||fabsf(a.outbound-again.outbound)>2||fabsf(a.inbound-again.inbound)>2||fabsf(a.rssi-again.rssi)>3)return Verdict::CONDITIONS_CHANGED;
  float tx=b.outbound-(a.outbound+again.outbound)/2,rx=b.inbound-(a.inbound+again.inbound)/2,signal=b.rssi-(a.rssi+again.rssi)/2;
  if(tx>=2&&rx>=2&&signal>=0&&b.replies>=a.replies)return Verdict::B_STRONGER;
  if(tx<=-2&&rx<=-2&&signal<=0&&a.replies>=b.replies)return Verdict::A_STRONGER;
  if((tx>=2&&rx<=-2)||(tx<=-2&&rx>=2))return Verdict::MIXED;
  return Verdict::SIMILAR;
}
inline bool capture(Session& s,int slot,const uint8_t* key,int8_t tx,int8_t rx,int8_t rssi){
  if(s.phase>2||slot<0||slot>=s.count[s.phase]||memcmp(key,s.key,32))return false;auto& r=s.samples[s.phase][slot];
  if(r.state!=State::LISTENING||r.heard)return false;r.heard=true;r.outbound=tx;r.inbound=rx;r.rssi=rssi;return true;
}
constexpr size_t META=254,RECORD=10,MAX_FILE=12+SESSIONS*(META+3*SLOTS*RECORD);
inline void put32(uint8_t* p,uint32_t v){for(int i=0;i<4;i++)p[i]=uint8_t(v>>(8*i));}
inline uint32_t get32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline void encode(const Session& s,uint8_t (&p)[META]){
  memset(p,0,sizeof(p));put32(p,s.id);put32(p+4,s.epoch);p[8]=s.phase;p[9]=s.approximate;p[10]=uint8_t(s.power);
  memcpy(p+11,s.count,3);memcpy(p+14,s.key,32);memcpy(p+46,s.place,25);memcpy(p+71,s.nameA,17);memcpy(p+88,s.nameB,17);memcpy(p+105,s.targetName,33);memcpy(p+138,s.profile,65);
  // Reserved tail leaves room for future read-only radio configuration fields.
}
inline bool decode(const uint8_t* p,size_t n,Session& s){
  if(n!=META||!get32(p)||p[8]>2||p[9]>1||p[11]>SLOTS||p[12]>SLOTS||p[13]>SLOTS||!p[46]||p[70]||!p[71]||p[87]||!p[88]||p[104]||p[137]||!p[138]||p[202])return false;
  uint8_t any=0;for(int i=14;i<46;i++)any|=p[i];if(!any)return false;for(size_t i=203;i<META;i++)if(p[i])return false;
  s={};s.id=get32(p);s.epoch=get32(p+4);s.phase=p[8];s.approximate=p[9];s.power=int8_t(p[10]);memcpy(s.count,p+11,3);memcpy(s.key,p+14,32);
  memcpy(s.place,p+46,25);memcpy(s.nameA,p+71,17);memcpy(s.nameB,p+88,17);memcpy(s.targetName,p+105,33);memcpy(s.profile,p+138,65);return true;
}
inline void encode(const Sample& s,uint8_t (&p)[RECORD]){put32(p,s.epoch);p[4]=uint8_t(s.state);p[5]=s.accepted;p[6]=s.heard;p[7]=uint8_t(s.outbound);p[8]=uint8_t(s.inbound);p[9]=uint8_t(s.rssi);}
inline bool decode(const uint8_t* p,size_t n,Sample& s){
  if(n!=RECORD||p[4]>uint8_t(State::CANCELLED)||p[5]>1||p[6]>1)return false;
  s.epoch=get32(p);s.state=State(p[4]);s.accepted=p[5];s.heard=p[6];s.outbound=int8_t(p[7]);s.inbound=int8_t(p[8]);s.rssi=int8_t(p[9]);return true;
}
inline bool selfTest(){
  Session s;s.id=1;s.key[0]=9;strcpy(s.place,"Hill");strcpy(s.nameA,"Stock");strcpy(s.nameB,"Whip");strcpy(s.profile,"868 / SF8");s.power=20;
  uint8_t meta[META];encode(s,meta);Session loaded;if(!decode(meta,sizeof(meta),loaded)||loaded.power!=20||strcmp(loaded.nameB,"Whip"))return false;
  meta[8]=3;if(decode(meta,sizeof(meta),loaded))return false;encode(s,meta);meta[11]=SLOTS+1;if(decode(meta,sizeof(meta),loaded))return false;
  for(int phase=0;phase<3;phase++){s.count[phase]=3;for(int i=0;i<3;i++){auto& r=s.samples[phase][i];r.state=State::COMPLETE;r.accepted=true;r.heard=true;r.outbound=phase==1?8:-8;r.inbound=phase==1?4:-12;r.rssi=phase==1?-95:-99;}}
  if(verdict(s)!=Verdict::B_STRONGER)return false;
  for(int i=0;i<3;i++){s.samples[1][i].outbound=-24;s.samples[1][i].inbound=-28;s.samples[1][i].rssi=-103;}if(verdict(s)!=Verdict::A_STRONGER)return false;
  for(int i=0;i<3;i++){s.samples[1][i].outbound=8;s.samples[1][i].rssi=-99;}if(verdict(s)!=Verdict::MIXED)return false;
  for(int i=0;i<3;i++){s.samples[1][i].outbound=-8;s.samples[1][i].inbound=-12;}if(verdict(s)!=Verdict::SIMILAR)return false;
  s.samples[2][0].rssi=-120;s.samples[2][1].rssi=-120;if(verdict(s)!=Verdict::CONDITIONS_CHANGED)return false;
  for(int phase=0;phase<3;phase++)for(int i=0;i<3;i++)s.samples[phase][i].heard=false;if(verdict(s)!=Verdict::TOO_FEW)return false;
  s.samples[0][0].state=State::INTERRUPTED;if(summary(s,0).attempts!=2||verdict(s)!=Verdict::INCOMPLETE)return false;
  s.phase=0;s.samples[0][0].state=State::LISTENING;uint8_t other[32]={8};
  if(capture(s,0,other,1,2,-99)||!capture(s,0,s.key,-9,-13,-104)||capture(s,0,s.key,100,100,0))return false;
  uint8_t record[RECORD];encode(s.samples[0][0],record);Sample sample;if(!decode(record,sizeof(record),sample)||sample.outbound!=-9||sample.rssi!=-104)return false;
  if(!recover(s)||s.samples[0][0].state!=State::INTERRUPTED||summary(s,0).attempts!=2)return false;
  int8_t values[]={-100,-90};return median(values,2)==-95;
}
}
