#pragma once
String fieldPath(const char* extension){String path=journalPath;path.replace(".log",extension);return path;}
struct FieldWriter {
  File file;String path,temp;uint32_t checksum=2166136261u;bool ok=false;
  FieldWriter(const String& destination,const char* magic,uint8_t count):path(destination),temp(destination+".tmp"){
    if(!storageReady||!journalPath.length())return;file=LittleFS.open(temp,"w");ok=bool(file);
    uint8_t header[8]={};memcpy(header,magic,4);header[4]=count;put(header,sizeof(header));
  }
  void put(const uint8_t* bytes,size_t length){if(!ok)return;checksum=field::hash(bytes,length,checksum);ok=file.write(bytes,length)==length;}
  bool finish(){
    uint8_t tail[4];field::put32(tail,checksum);if(ok)ok=file.write(tail,4)==4;if(file){file.flush();file.close();}
    if(ok)ok=LittleFS.rename(temp,path);if(!ok&&LittleFS.exists(temp))LittleFS.remove(temp);return ok;
  }
};
struct FieldReader {
  File file;uint32_t checksum=2166136261u;uint8_t count=0;bool ok=false;
  FieldReader(const String& path,const char* magic,size_t maxCount,size_t maxSize){
    if(!storageReady||!LittleFS.exists(path))return;file=LittleFS.open(path,"r");
    ok=file&&file.size()>=12&&file.size()<=maxSize;uint8_t header[8]={};get(header,8);
    ok=ok&&!memcmp(header,magic,4)&&header[4]<=maxCount&&!header[5]&&!header[6]&&!header[7];count=header[4];
  }
  void get(uint8_t* bytes,size_t length){if(!ok)return;ok=file.read(bytes,length)==length;if(ok)checksum=field::hash(bytes,length,checksum);}
  bool finish(){uint8_t tail[4]={};if(ok)ok=file.read(tail,4)==4&&protocol::read32(tail)==checksum&&!file.available();if(file)file.close();return ok;}
};
bool writeSendResults(){
  FieldWriter w(fieldPath(".sent"),"MPS1",sendResults.size());
  for(const auto& r:sendResults){uint8_t bytes[51];field::encode(r,bytes);w.put(bytes,sizeof(bytes));}return w.finish();
}
bool readSendResults(std::vector<field::SendResult>& output){
  FieldReader reader(fieldPath(".sent"),"MPS1",field::MAX_SENDS,4092);std::vector<field::SendResult> loaded;
  for(int i=0;reader.ok&&i<reader.count;i++){
    uint8_t bytes[51]={};reader.get(bytes,sizeof(bytes));field::SendResult r;
    if(!reader.ok||!field::decode(bytes,sizeof(bytes),r)){reader.ok=false;break;}
    for(const auto& old:loaded)if(old.timestamp==r.timestamp)reader.ok=false;loaded.push_back(r);
  }
  if(!reader.finish())return false;output=std::move(loaded);return true;
}
bool writeCoverage(){
  FieldWriter w(fieldPath(".coverage"),"MPC1",coverageTests.size());
  for(const auto& t:coverageTests){
    uint8_t meta[42];field::encode(t,meta);w.put(meta,sizeof(meta));
    for(const auto& signal:t.replies){uint8_t bytes[35];memcpy(bytes,signal.key,32);bytes[32]=uint8_t(signal.outbound);bytes[33]=uint8_t(signal.inbound);bytes[34]=uint8_t(signal.rssi);w.put(bytes,sizeof(bytes));}
  }return w.finish();
}
bool readCoverage(std::vector<field::Test>& output){
  FieldReader reader(fieldPath(".coverage"),"MPC1",field::MAX_TESTS,13956);std::vector<field::Test> loaded;
  for(int i=0;reader.ok&&i<reader.count;i++){
    uint8_t meta[42]={},count=0;reader.get(meta,sizeof(meta));field::Test t;
    if(!reader.ok||!field::decode(meta,sizeof(meta),t,count)){reader.ok=false;break;}
    for(const auto& old:loaded)if(old.id==t.id)reader.ok=false;
    for(int j=0;reader.ok&&j<count;j++){
      uint8_t bytes[35]={};reader.get(bytes,sizeof(bytes));field::Signal signal;memcpy(signal.key,bytes,32);signal.outbound=int8_t(bytes[32]);signal.inbound=int8_t(bytes[33]);signal.rssi=int8_t(bytes[34]);
      uint8_t any=0;for(auto b:signal.key)any|=b;if(!any)reader.ok=false;
      for(const auto& old:t.replies)if(!memcmp(old.key,signal.key,32))reader.ok=false;t.replies.push_back(signal);
    }loaded.push_back(std::move(t));
  }
  if(!reader.finish())return false;output=std::move(loaded);return true;
}
void saveSendResults(){if(fixtureMode)return;sendResultsDirty=true;sendResultsFailed=!writeSendResults();if(!sendResultsFailed)sendResultsDirty=false;fieldSaveAt=millis();}
void saveCoverage(){if(fixtureMode)return;coverageDirty=true;coverageFailed=!writeCoverage();if(!coverageFailed)coverageDirty=false;fieldSaveAt=millis();}
void loadFieldData(){
  sendResults.clear();coverageTests.clear();coverageCapture=false;coverageActiveId=0;coverageSelectedId=0;coveragePage=coverageReplyPage=sentPage=0;
  sendResultsDirty=coverageDirty=false;sendResultsFailed=coverageFailed=false;
  if(!storageReady)return;
  if(LittleFS.exists(fieldPath(".sent"))&&!readSendResults(sendResults))sendResultsFailed=true;
  if(LittleFS.exists(fieldPath(".coverage"))&&!readCoverage(coverageTests))coverageFailed=true;
  for(auto& r:sendResults){lastOutgoingTimestamp=max(lastOutgoingTimestamp,r.timestamp);if(field::recover(r))sendResultsDirty=true;}
  for(auto& t:coverageTests)if(field::recover(t))coverageDirty=true;
  if(sendResultsDirty)saveSendResults();if(coverageDirty)saveCoverage();
  if(!coverageTests.empty())coveragePlace=coverageTests.back().place;
}
field::SendResult* findSendResult(uint32_t timestamp){for(auto& r:sendResults)if(r.timestamp==timestamp)return &r;return nullptr;}
void updateSendResult(uint32_t timestamp,field::SendState state){
  if(fixtureMode)return;auto* r=findSendResult(timestamp);if(!r)return;field::transition(*r,state,nowSeconds());saveSendResults();refreshFieldDetails();
}
void addSendResult(const SendWatch& s){
  if(fixtureMode)return;field::SendResult r;r.timestamp=s.timestamp;r.updated=nowSeconds();r.direct=s.direct;r.approximate=!clockTrusted;r.channel=s.channel;memcpy(r.peer,s.peer,6);
  memcpy(r.target,s.target.c_str(),min(size_t(32),size_t(s.target.length())));
  if(sendResults.size()>=field::MAX_SENDS)sendResults.erase(sendResults.begin());sendResults.push_back(r);saveSendResults();
}
void interruptSendResults(){
  bool changed=false;for(auto& s:sends)if(s.active&&!s.done){auto* r=findSendResult(s.timestamp);if(r&&field::recover(*r))changed=true;}
  if(changed)saveSendResults();
}
void tickFieldStorage(){if(millis()-fieldSaveAt<2000)return;if(sendResultsDirty)saveSendResults();if(coverageDirty)saveCoverage();}

bool fieldStorageSelfTest(){
  if(!storageReady||coverageCapture)return false;
  String savedPath=journalPath;auto savedResults=std::move(sendResults);auto savedTests=std::move(coverageTests);
  journalPath="/meshpilot-field-fixture.log";sendResults.clear();coverageTests.clear();bool ok=true;
  field::SendResult r;r.timestamp=123;r.updated=124;r.accepted=true;r.state=field::SendState::ACK;strcpy(r.target,"Fixture");sendResults.push_back(r);
  std::vector<field::SendResult> results;ok=writeSendResults()&&readSendResults(results)&&results.size()==1&&results[0].state==field::SendState::ACK;
  sendResults[0].state=field::SendState::NO_REPEAT;ok=writeSendResults()&&readSendResults(results)&&results[0].state==field::SendState::NO_REPEAT&&ok;
  field::Test t;t.id=7;t.started=123;strcpy(t.place,"Fixture hill");t.state=field::TestState::COMPLETE;t.accepted=true;
  field::Signal signal;signal.key[0]=1;signal.outbound=-11;signal.inbound=-7;signal.rssi=-102;t.replies.push_back(signal);coverageTests.push_back(t);
  std::vector<field::Test> tests;ok=writeCoverage()&&readCoverage(tests)&&tests.size()==1&&tests[0].replies.size()==1&&tests[0].replies[0].rssi==-102&&ok;
  coverageTests[0].replies.clear();ok=writeCoverage()&&readCoverage(tests)&&tests[0].replies.empty()&&tests[0].state==field::TestState::COMPLETE&&ok;
  // Exercise the exact on-disk bounds using the streaming production codec.
  sendResults.clear();for(size_t i=0;i<field::MAX_SENDS;i++){r.timestamp=i+1;sendResults.push_back(r);}
  ok=writeSendResults()&&readSendResults(results)&&results.size()==field::MAX_SENDS&&results.back().timestamp==field::MAX_SENDS&&ok;
  coverageTests.clear();t.replies.clear();
  for(size_t i=0;i<field::MAX_REPLIES;i++){signal.key[0]=i+1;t.replies.push_back(signal);}
  for(size_t i=0;i<field::MAX_TESTS;i++){t.id=i+1;coverageTests.push_back(t);}
  ok=writeCoverage()&&readCoverage(tests)&&tests.size()==field::MAX_TESTS&&tests.back().replies.size()==field::MAX_REPLIES&&tests.back().replies.back().key[0]==32&&ok;
  // A checksum mismatch or truncated file must not replace the caller's data.
  File corrupt=LittleFS.open(fieldPath(".sent"),"r+");if(corrupt){corrupt.seek(20);corrupt.write(uint8_t(0xff));corrupt.close();}else ok=false;
  ok=!readSendResults(results)&&results.size()==field::MAX_SENDS&&ok;
  corrupt=LittleFS.open(fieldPath(".coverage"),"w");if(corrupt){corrupt.print("MPC1");corrupt.close();}else ok=false;
  ok=!readCoverage(tests)&&tests.size()==field::MAX_TESTS&&ok;
  const String paths[]={fieldPath(".sent"),fieldPath(".sent")+".tmp",fieldPath(".coverage"),fieldPath(".coverage")+".tmp"};
  for(const auto& p:paths)if(LittleFS.exists(p))LittleFS.remove(p);
  journalPath=savedPath;sendResults=std::move(savedResults);coverageTests=std::move(savedTests);return ok;
}
