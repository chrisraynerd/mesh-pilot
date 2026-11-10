#pragma once
#include <stdint.h>
namespace touch_tools {
struct Point {int x,y;};
// A stable cluster wins over transient contact coordinates. Never average a
// moving stroke across neighbouring buttons into a new, unintended target.
inline bool settled(const Point* points,int count,Point& out){
  if(count<3||count>5)return false;int xs[5],ys[5];for(int i=0;i<count;i++){xs[i]=points[i].x;ys[i]=points[i].y;}
  for(int i=1;i<count;i++){int x=xs[i],y=ys[i],j=i;while(j&&xs[j-1]>x){xs[j]=xs[j-1];j--;}xs[j]=x;j=i;while(j&&ys[j-1]>y){ys[j]=ys[j-1];j--;}ys[j]=y;}
  Point median{xs[count/2],ys[count/2]};int agree=0;for(int i=0;i<count;i++)if(points[i].x>=median.x-8&&points[i].x<=median.x+8&&points[i].y>=median.y-8&&points[i].y<=median.y+8)agree++;
  if(agree<3)return false;out=median;return true;
}
inline bool selfTest(){Point out{9,9};Point noise[]={{1,1},{238,188},{241,190},{239,189},{320,230}};
  if(!settled(noise,5,out)||out.x!=239||out.y!=189)return false;
  Point drag[]={{10,30},{40,30},{70,30},{100,30},{130,30}};out={9,9};if(settled(drag,5,out)||out.x!=9||out.y!=9)return false;
  Point crossed[]={{0,100},{100,0},{100,100}};if(settled(crossed,3,out))return false;
  Point shortTap[]={{230,180},{231,181}};if(settled(shortTap,2,out))return false;
  Point stable[]={{200,180},{201,181},{199,179}};return settled(stable,3,out)&&out.x==200&&out.y==180;
}
}
