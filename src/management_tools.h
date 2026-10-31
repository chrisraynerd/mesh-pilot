#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <mbedtls/sha256.h>
namespace management {
inline int nibble(char c) {
  if(c>='0'&&c<='9')return c-'0';
  if(c>='a'&&c<='f')return c-'a'+10;
  if(c>='A'&&c<='F')return c-'A'+10;
  return -1;
}
inline bool hex(const char* text,size_t length,uint8_t* output,size_t bytes) {
  if(length!=bytes*2)return false;
  for(size_t i=0;i<length;i++)if(nibble(text[i])<0)return false;
  for(size_t i=0;i<bytes;i++)output[i]=(nibble(text[i*2])<<4)|nibble(text[i*2+1]);
  return true;
}
inline bool nonzero(const uint8_t* key,size_t bytes) {
  for(size_t i=0;i<bytes;i++)if(key[i])return true;return false;
}
inline bool hashtag(const char* text,size_t length,uint8_t* key) {
  if(length<2||length>31||text[0]!='#')return false;
  uint8_t digest[32];if(mbedtls_sha256_ret(reinterpret_cast<const uint8_t*>(text),length,digest,0))return false;
  memcpy(key,digest,16);return true;
}
inline bool channel(uint8_t slot,uint8_t maximum,const char* name,size_t length,const uint8_t* key,uint8_t* frame) {
  if(slot>=maximum||length>31||(length&&!nonzero(key,16)))return false;
  memset(frame,0,50);frame[0]=32;frame[1]=slot;
  if(length){memcpy(frame+2,name,length);memcpy(frame+34,key,16);}return true;
}
inline bool sameChannel(const uint8_t* reply,size_t length,const uint8_t* request) {
  // Radio GET_CHANNEL writes a C string into a reused output buffer; bytes
  // after its NUL terminator are not guaranteed to be zero padding.
  if(length<50||reply[0]!=18||reply[1]!=request[1])return false;
  size_t actual=strnlen(reinterpret_cast<const char*>(reply+2),32),wanted=strnlen(reinterpret_cast<const char*>(request+2),32);
  return actual==wanted&&!memcmp(reply+2,request+2,wanted)&&!memcmp(reply+34,request+34,16);
}
inline bool selfTest() {
  uint8_t key[32]={},frame[50];
  const uint8_t expected[]={0x9c,0xd8,0xfc,0xf2,0x2a,0x47,0x33,0x3b,0x59,0x1d,0x96,0xa2,0xb8,0x48,0xb7,0x3f};
  if(!hashtag("#test",5,key)||memcmp(key,expected,16))return false;
  if(!channel(1,8,"#test",5,key,frame)||frame[0]!=32||frame[1]!=1||frame[7]||memcmp(frame+34,key,16))return false;
  uint8_t reply[50];memcpy(reply,frame,50);reply[0]=18;reply[20]=0xaa;
  if(!sameChannel(reply,50,frame)||sameChannel(reply,49,frame))return false;
  reply[49]^=1;if(sameChannel(reply,50,frame))return false;
  if(channel(8,8,"#test",5,key,frame)||hashtag("#",1,key))return false;
  if(!hex("00aAFF12",8,key,4)||key[1]!=0xaa||key[2]!=0xff||hex("00xz",4,key,2)||hex("123",3,key,2))return false;
  memset(key,0,32);
  return !channel(1,8,"Private",7,key,frame)&&channel(1,8,"",0,key,frame)&&frame[2]==0&&frame[34]==0;
}
}
