#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <stdio.h>
namespace text_render {
// Decode full Unicode rather than passing four-byte UTF-8 to TFT_eSPI's
// built-in 16-bit decoder. Bad sequences consume one byte, preserving ASCII.
inline uint32_t next(const uint8_t* p,size_t n,size_t& i) {
  uint8_t lead=p[i++];if(lead<128)return lead;
  size_t count=lead>=0xc2&&lead<=0xdf?1:lead>=0xe0&&lead<=0xef?2:lead>=0xf0&&lead<=0xf4?3:0;
  if(!count||count>n-i)return 0xfffd;
  uint32_t cp=lead&((1u<<(6-count))-1);
  for(size_t k=0;k<count;k++) {if((p[i+k]&0xc0)!=0x80)return 0xfffd;cp=(cp<<6)|(p[i+k]&63);}
  if((count==1&&cp<0x80)||(count==2&&cp<0x800)||(count==3&&cp<0x10000)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return 0xfffd;
  i+=count;return cp;
}
inline std::string ascii(const uint8_t* p,size_t n,bool newlines=false) {
  std::string out;out.reserve(n);
  for(size_t i=0;i<n;) {
    uint32_t cp=next(p,n,i);if(!cp)break;
    if(newlines&&cp==10){out+='\n';continue;}
    if(newlines&&cp==13&&i<n&&p[i]==10)continue;
    if(cp>=32&&cp<=126){out+=char(cp);continue;}
    if(cp<32||cp==127||cp==0xa0){out+=' ';continue;}
    if((cp>=0x300&&cp<=0x36f)||cp==0x200d||cp==0xfe0e||cp==0xfe0f||(cp>=0x1f3fb&&cp<=0x1f3ff))continue;
    if(cp>=0xc0&&cp<=0xc5){out+='A';continue;}
    if(cp>=0xe0&&cp<=0xe5){out+='a';continue;}
    if(cp>=0xc8&&cp<=0xcb){out+='E';continue;}
    if(cp>=0xe8&&cp<=0xeb){out+='e';continue;}
    if(cp>=0xcc&&cp<=0xcf){out+='I';continue;}
    if(cp>=0xec&&cp<=0xef){out+='i';continue;}
    if(cp>=0xd2&&cp<=0xd6){out+='O';continue;}
    if(cp>=0xf2&&cp<=0xf6){out+='o';continue;}
    if(cp>=0xd9&&cp<=0xdc){out+='U';continue;}
    if(cp>=0xf9&&cp<=0xfc){out+='u';continue;}
    const char* replacement=nullptr;
    switch(cp) {
      case 0xc6:replacement="AE";break;case 0xe6:replacement="ae";break;
      case 0xc7:replacement="C";break;case 0xe7:replacement="c";break;
      case 0xd1:replacement="N";break;case 0xf1:replacement="n";break;
      case 0xd8:replacement="O";break;case 0xf8:replacement="o";break;
      case 0xdd:replacement="Y";break;case 0xfd:case 0xff:replacement="y";break;
      case 0xdf:replacement="ss";break;
      case 0x2018:case 0x2019:replacement="'";break;
      case 0x201c:case 0x201d:replacement="\"";break;
      case 0x2013:case 0x2014:case 0x2212:replacement="-";break;
      case 0x2026:replacement="...";break;case 0x2022:replacement="*";break;
      case 0xa3:replacement="GBP";break;case 0x20ac:replacement="EUR";break;
      case 0xb0:replacement=" deg";break;case 0xd7:replacement="x";break;
      case 0x1f431:replacement="[cat]";break;
      case 0x1f590:replacement="[hand]";break;
      case 0x1f44b:replacement="[wave]";break;
      case 0x1f44d:replacement="[thumbs up]";break;
      case 0x2764:replacement="[heart]";break;
      case 0x2705:case 0x2714:replacement="[check]";break;
      case 0x26a0:replacement="[warning]";break;
      case 0x1f198:replacement="[SOS]";break;
      case 0x1f602:case 0x1f923:replacement="[laugh]";break;
      case 0x1f600:case 0x1f603:case 0x1f604:case 0x1f642:replacement="[smile]";break;
      case 0xfffd:replacement="[?]";break;
    }
    if(replacement)out+=replacement;
    else {char marker[16];snprintf(marker,sizeof(marker),"[U+%04lX]",static_cast<unsigned long>(cp));out+=marker;}
  }
  return out;
}
inline bool selfTest() {
  const uint8_t emoji[]={0xf0,0x9f,0x90,0xb1,'N','o','d','e',':',' ','h','e','l','l','o',' ',0xf0,0x9f,0x96,0x90,0xef,0xb8,0x8f};
  const uint8_t accents[]={'c','a','f',0xc3,0xa9,' ',0xe2,0x80,0x94,' ',0xc2,0xa3,'5'};
  const uint8_t bad[]={0xf0,0x9f,'O','K'};
  const uint8_t unknown[]={0xf0,0x9f,0x9a,0x80};
  const uint8_t lines[]={'A',13,10,'B',10,'C'};
  return ascii(lines,sizeof(lines))=="A  B C"&&ascii(lines,sizeof(lines),true)=="A\nB\nC"&&ascii(emoji,sizeof(emoji))=="[cat]Node: hello [hand]"
    &&ascii(accents,sizeof(accents))=="cafe - GBP5"
    &&ascii(bad,sizeof(bad))=="[?][?]OK"
    &&ascii(unknown,sizeof(unknown))=="[U+1F680]";
}
}
