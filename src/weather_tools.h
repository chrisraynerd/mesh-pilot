#pragma once
#include <stdint.h>
#include <string>
#include <math.h>
#include <stdio.h>
namespace weather {
inline std::string normalize(const std::string& input){std::string out;bool space=false;for(unsigned char c:input){if(c==' '||c=='\t'||c=='\r'||c=='\n'){space=!out.empty();continue;}if(space){out+=' ';space=false;}out+=char(c>='A'&&c<='Z'?c+32:c);}return out;}
inline bool location(const std::string& s){if(s.empty()||s.size()>32)return false;bool letter=false;for(char c:s){if(c>='a'&&c<='z')letter=true;else if(!((c>='0'&&c<='9')||c==' '||c==','||c=='-'||c=='\''))return false;}return letter;}
inline bool query(const std::string& body,std::string& place){auto s=normalize(body);size_t split=s.find(' ');auto word=s.substr(0,split);if(word!="wx"&&word!="weather")return false;place=split==std::string::npos?"":s.substr(split+1);return true;}
inline std::string encode(const std::string& s){std::string out;for(unsigned char c:s){if((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-')out+=c;else{char b[4];snprintf(b,4,"%%%02X",c);out+=b;}}return out;}
struct Data {char query[33]={},name[25]={};uint32_t epoch=0;float temperature=0,wind=0,direction=0;int code=0;};
inline bool valid(const Data& d){return d.epoch>=1577836800u&&d.epoch<=4102444799u&&isfinite(d.temperature)&&d.temperature>=-100&&d.temperature<=70&&isfinite(d.wind)&&d.wind>=0&&d.wind<=300&&isfinite(d.direction)&&d.direction>=0&&d.direction<=360&&d.code>=0&&d.code<=99;}
inline const char* condition(int code){if(code==0)return "clear";if(code<=2)return "part-cloudy";if(code==3)return "cloudy";if(code==45||code==48)return "fog";if(code>=51&&code<=57)return "drizzle";if(code>=61&&code<=67)return "rain";if((code>=71&&code<=77)||code==85||code==86)return "snow";if(code>=80&&code<=82)return "showers";if(code>=95)return "thunderstorms";return "conditions unknown";}
inline bool cacheAllowed(uint32_t now,uint32_t epoch,uint32_t elapsed,uint32_t maxAge){return elapsed<=maxAge&&now>=epoch&&now-epoch<=21600;}
inline std::string render(const Data& d,uint32_t ageMinutes=0,bool cached=false){
  const char* dirs[]={"N","NE","E","SE","S","SW","W","NW"};char stamp[8];snprintf(stamp,8,"%02u:%02uZ",unsigned(d.epoch%86400/3600),unsigned(d.epoch%3600/60));
  char result[121];char cache[24]="";if(cached)snprintf(cache,sizeof(cache),"CACHED %um; ",unsigned(ageMinutes));
  snprintf(result,sizeof(result),"%s%.20s %s: %.0fC %s; %s %.0fmph. Open-Meteo",cache,d.name,stamp,double(d.temperature),condition(d.code),dirs[int((d.direction+22.5f)/45)%8],double(d.wind));
  return std::string(result).substr(0,114);
}
inline bool selfTest(){std::string place;if(!query(" Wx  Ramsbottom ",place)||place!="ramsbottom"||query("wxplease",place)||!query("weather",place)||!place.empty())return false;
  if(!location("ramsbottom")||location("http://evil")||location("a&b")||location("")||encode("a b")!="a%20b")return false;
  if(!cacheAllowed(10000,9000,100,1000)||cacheAllowed(40000,9000,100,1000)||cacheAllowed(8000,9000,100,1000)||cacheAllowed(10000,9000,1001,1000))return false;
  Data d;snprintf(d.name,25,"Ramsbottom");d.epoch=1794225600;d.temperature=8;d.wind=12;d.direction=270;d.code=3;
  if(!valid(d)||render(d).find("W 12mph")==std::string::npos||render(d,120,true).find("CACHED 120m")==std::string::npos)return false;d.temperature=NAN;if(valid(d))return false;d.temperature=8;d.wind=-1;return !valid(d);
}
}
