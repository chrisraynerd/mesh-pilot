#pragma once
#include <stdint.h>
#include <stddef.h>
namespace clock_tools {
inline bool leap(int y){return y%4==0&&(y%100!=0||y%400==0);}
inline bool parse(const char* digits,size_t length,uint32_t& timestamp) {
  if(length!=12)return false;
  int v[12];for(int i=0;i<12;i++){if(digits[i]<'0'||digits[i]>'9')return false;v[i]=digits[i]-'0';}
  int y=v[0]*1000+v[1]*100+v[2]*10+v[3],m=v[4]*10+v[5],d=v[6]*10+v[7],h=v[8]*10+v[9],minute=v[10]*10+v[11];
  if(y<2020||y>2099||m<1||m>12||h>23||minute>59)return false;
  const int months[]={31,28,31,30,31,30,31,31,30,31,30,31};
  if(d<1||d>months[m-1]+(m==2&&leap(y)))return false;
  uint32_t days=0;for(int year=1970;year<y;year++)days+=leap(year)?366:365;
  for(int month=1;month<m;month++)days+=months[month-1]+(month==2&&leap(y));
  timestamp=((days+d-1)*24+h)*3600+minute*60;return true;
}
inline bool selfTest(){uint32_t t=0;
  return parse("202610302319",12,t)&&t==1793402340u
    &&parse("202402290000",12,t)&&!parse("202602290000",12,t)
    &&!parse("202604310000",12,t)&&!parse("202610302400",12,t)
    &&!parse("202610302319",11,t)&&!parse("20261030236x",12,t);
}
}
