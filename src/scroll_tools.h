#pragma once
#include <stdint.h>
namespace scroll_tools {
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
  Position p;p.reset(400);if(p.offset!=272||!p.following)return false;
  p.drag(50);if(p.offset!=222||p.following)return false;
  p.newMessages=true;p.resize(455,222);if(p.offset!=222||!p.newMessages)return false;
  p.resize(433,200);if(p.offset!=200||p.following)return false;
  p.drag(1000);if(p.offset)return false;p.drag(-1000);if(p.offset!=305||!p.following||p.newMessages)return false;
  p.resize(500);if(p.offset!=372)return false;p.reset(22);p.drag(50);
  return p.offset==0&&p.following&&median(10,90,11)==11&&median(30,29,28)==29;
}
}
