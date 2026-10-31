#pragma once
#include <stdint.h>
namespace scroll_tools {
struct DetailHold {
  int x=0,y=0;uint32_t started=0,last=0;bool eligible=false;
  void begin(int px,int py,uint32_t now){x=px;y=py;started=last=now;eligible=true;}
  void observe(int px,int py,uint32_t now,bool dragging){
    if(dragging||px-x>=6||x-px>=6||py-y>=6||y-py>=6||uint32_t(now-last)>100)eligible=false;
    last=now;
  }
  bool opens(bool dragging)const{return eligible&&!dragging&&uint32_t(last-started)>=700;}
};
struct Position {
  int offset=0,height=0,viewport=128;
  bool following=true,newMessages=false;
  int maximum() const {return height>viewport?height-viewport:0;}
  void clamp(){if(offset<0)offset=0;if(offset>maximum())offset=maximum();}
  void reset(int content){height=content;following=true;offset=maximum();newMessages=false;}
  void resize(int content,int anchor=-1){height=content;if(following)offset=maximum();else if(anchor>=0)offset=anchor;clamp();}
  void drag(int fingerDelta){offset-=fingerDelta;clamp();following=offset>=maximum();if(following)newMessages=false;}
  void latest(){offset=maximum();following=true;newMessages=false;}
};
inline int median(int a,int b,int c){return a>b?(b>c?b:a>c?c:a):(a>c?a:b>c?c:b);}
inline bool selfTest(){
  DetailHold hold;hold.begin(40,80,100);hold.observe(40,80,200,false);if(hold.opens(false))return false;
  for(uint32_t t=300;t<=800;t+=100)hold.observe(41,81,t,false);if(!hold.opens(false))return false;
  hold.observe(41,85,810,true);if(hold.opens(false))return false;
  hold.begin(40,80,100);for(uint32_t t=200;t<=900;t+=100)hold.observe(46,80,t,false);if(hold.opens(false))return false;
  hold.begin(40,80,100);hold.observe(40,80,900,false);if(hold.opens(false))return false;
  hold.begin(40,80,0xffffff00);for(int i=1;i<=8;i++)hold.observe(40,80,uint32_t(0xffffff00u+i*100u),false);if(!hold.opens(false))return false;
  Position p;p.reset(400);if(p.offset!=272||!p.following)return false;
  p.drag(50);if(p.offset!=222||p.following)return false;
  p.newMessages=true;p.resize(455,222);if(p.offset!=222||!p.newMessages)return false;
  p.resize(433,200);if(p.offset!=200||p.following)return false;
  p.drag(1000);if(p.offset)return false;p.drag(-1000);if(p.offset!=305||!p.following||p.newMessages)return false;
  p.resize(500);if(p.offset!=372)return false;p.reset(22);p.drag(50);
  return p.offset==0&&p.following&&median(10,90,11)==11&&median(30,29,28)==29;
}
}
