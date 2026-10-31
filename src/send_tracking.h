#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include "protocol.h"
#include "diagnostics.h"
namespace send_tracking {
inline size_t channelPayload(const uint8_t* key,const char* name,const char* text,uint32_t timestamp,uint8_t* output) {
  uint8_t plain[176]={},secret[32]={},digest[32];memcpy(secret,key,16);
  size_t nameLen=strlen(name);if(nameLen>32)return 0;
  size_t prefix=nameLen+2,textLen=strlen(text);if(textLen+prefix>160)textLen=160-prefix;
  protocol::write32(plain,timestamp);memcpy(plain+5,name,nameLen);plain[5+nameLen]=':';plain[6+nameLen]=' ';
  memcpy(plain+5+prefix,text,textLen);size_t length=(5+prefix+textLen+15)&~size_t(15);
  if(mbedtls_sha256_ret(key,16,digest,0))return 0;output[0]=digest[0];
  mbedtls_aes_context aes;mbedtls_aes_init(&aes);
  if(mbedtls_aes_setkey_enc(&aes,key,128)){mbedtls_aes_free(&aes);return 0;}
  for(size_t i=0;i<length;i+=16)if(mbedtls_aes_crypt_ecb(&aes,MBEDTLS_AES_ENCRYPT,plain+i,output+3+i)){mbedtls_aes_free(&aes);return 0;}
  mbedtls_aes_free(&aes);
  if(mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),secret,32,output+3,length,digest))return 0;
  memcpy(output+1,digest,2);return length+3;
}
inline uint32_t directAck(const uint8_t* publicKey,const char* text,uint32_t timestamp) {
  uint8_t data[197]={},hash[32];size_t len=strlen(text);if(len>160)return 0;
  protocol::write32(data,timestamp);memcpy(data+5,text,len);memcpy(data+5+len,publicKey,32);
  if(mbedtls_sha256_ret(data,37+len,hash,0))return 0;return protocol::read32(hash);
}
inline bool echoed(const uint8_t* raw,size_t n,const uint8_t* expected,size_t expectedLen,uint8_t* lastHash,uint8_t& width) {
  if(n<2||!expectedLen||(raw[0]>>6)!=0||((raw[0]>>2)&15)!=5)return false;
  uint8_t route=raw[0]&3;if(route!=0&&route!=1)return false;
  size_t pathField=route==0?5:1;if(pathField>=n)return false;
  size_t bytes;if(!diagnostics::pathBytes(raw[pathField],bytes)||!bytes)return false;
  size_t payload=pathField+1+bytes;
  if(payload>n||n-payload!=expectedLen||memcmp(raw+payload,expected,expectedLen))return false;
  width=(raw[pathField]>>6)+1;memcpy(lastHash,raw+payload-width,width);return true;
}
inline bool selfTest() {
  uint8_t key[16],pub[32];for(int i=0;i<32;i++){pub[i]=i;if(i<16)key[i]=i;}
  const uint8_t expected[]={0xbe,0x64,0xa8,0xc6,0x60,0x23,0x7f,0x6d,0xa5,0x00,0xf0,0x27,0xbe,0xa1,0x63,0x37,0x51,0x08,0x37};
  uint8_t payload[180],raw[32]={0x15,1,0x42},last[3],width=0;
  size_t length=channelPayload(key,"Unit","hello",1700000000u,payload);
  if(length!=sizeof(expected)||memcmp(payload,expected,length)||directAck(pub,"hello",1700000000u)!=0x9a94808eu)return false;
  memcpy(raw+3,payload,length);
  if(!echoed(raw,length+3,payload,length,last,width)||last[0]!=0x42||width!=1)return false;
  if(echoed(raw,length+2,payload,length,last,width))return false;
  raw[3]^=1;if(echoed(raw,length+3,payload,length,last,width))return false;raw[3]^=1;
  raw[1]=0;if(echoed(raw,length+3,payload,length,last,width))return false;
  raw[0]=0x14;raw[1]=0;raw[2]=0;raw[3]=0;raw[4]=0;raw[5]=0x42;raw[6]=0x42;raw[7]=0;raw[8]=0x43;raw[9]=0;
  memcpy(raw+10,payload,length);
  return echoed(raw,length+10,payload,length,last,width)&&width==2&&last[0]==0x43&&last[1]==0;
}
}
