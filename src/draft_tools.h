#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <mbedtls/sha256.h>
#include "terminal_tools.h"
namespace draft_tools {
struct Draft { bool direct; uint8_t channel,peer[32],channelHash[8]; char name[33],text[121]; };
inline bool fingerprint(const uint8_t* key,uint8_t* out) {
  uint8_t hash[32];if(mbedtls_sha256_ret(key,16,hash,0))return false;memcpy(out,hash,8);return true;
}
inline size_t encode(const Draft& draft,uint8_t* out) {
  size_t length=strnlen(draft.text,121),name=strnlen(draft.name,33);
  if(!length||length>120||name>32||(!draft.direct&&draft.channel>=40))return 0;
  memset(out,0,203);memcpy(out,"MPD1",4);out[4]=draft.direct;out[5]=draft.channel;
  memcpy(out+6,draft.peer,32);memcpy(out+38,draft.channelHash,8);memcpy(out+46,draft.name,name);
  out[78]=length;memcpy(out+79,draft.text,length);protocol::write32(out+79+length,terminal_tools::checksum(out,79+length));return 83+length;
}
inline bool decode(const uint8_t* bytes,size_t length,Draft& draft) {
  if(length<84||length>203||memcmp(bytes,"MPD1",4)||bytes[4]>1||(!bytes[4]&&bytes[5]>=40)||!bytes[78]||bytes[78]>120||length!=size_t(83+bytes[78]))return false;
  if(protocol::read32(bytes+length-4)!=terminal_tools::checksum(bytes,length-4))return false;
  draft={};draft.direct=bytes[4];draft.channel=bytes[5];memcpy(draft.peer,bytes+6,32);memcpy(draft.channelHash,bytes+38,8);
  memcpy(draft.name,bytes+46,32);memcpy(draft.text,bytes+79,bytes[78]);return true;
}
inline bool selfTest() {
  Draft a{},b{};a.channel=3;strcpy(a.name,"#test");strcpy(a.text,"unsent fixture");uint8_t bytes[203];
  size_t length=encode(a,bytes);if(!length||!decode(bytes,length,b)||strcmp(b.text,a.text)||b.channel!=3||b.direct)return false;
  bytes[length-1]^=1;if(decode(bytes,length,b)||decode(bytes,length-1,b))return false;
  a.direct=true;a.peer[0]=9;length=encode(a,bytes);if(!decode(bytes,length,b)||!b.direct||b.peer[0]!=9)return false;
  memset(a.text,'x',120);a.text[120]=0;length=encode(a,bytes);if(length!=203||!decode(bytes,length,b)||strlen(b.text)!=120)return false;
  a.text[0]=0;return encode(a,bytes)==0;
}
}
