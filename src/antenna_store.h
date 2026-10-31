#pragma once
bool writeAntenna(){
  FieldWriter writer(fieldPath(".antenna"),"MPA1",antennaSessions.size());
  for(const auto& session:antennaSessions){
    uint8_t meta[antenna::META];antenna::encode(session,meta);writer.put(meta,sizeof(meta));
    for(int phase=0;phase<3;phase++)for(int i=0;i<session.count[phase];i++){uint8_t record[antenna::RECORD];antenna::encode(session.samples[phase][i],record);writer.put(record,sizeof(record));}
  }return writer.finish();
}
bool readAntenna(std::vector<antenna::Session>& output){
  FieldReader reader(fieldPath(".antenna"),"MPA1",antenna::SESSIONS,antenna::MAX_FILE);std::vector<antenna::Session> loaded;
  for(int i=0;reader.ok&&i<reader.count;i++){
    uint8_t meta[antenna::META]{};reader.get(meta,sizeof(meta));antenna::Session s;
    if(!reader.ok||!antenna::decode(meta,sizeof(meta),s)){reader.ok=false;break;}
    for(const auto& old:loaded)if(old.id==s.id)reader.ok=false;
    for(int phase=0;reader.ok&&phase<3;phase++)for(int j=0;reader.ok&&j<s.count[phase];j++){
      uint8_t record[antenna::RECORD]{};reader.get(record,sizeof(record));if(!reader.ok||!antenna::decode(record,sizeof(record),s.samples[phase][j]))reader.ok=false;
    }loaded.push_back(s);
  }
  if(!reader.finish())return false;output=std::move(loaded);return true;
}
void saveAntenna(){if(fixtureMode)return;antennaDirty=true;antennaFailed=!writeAntenna();if(!antennaFailed)antennaDirty=false;antennaSaveAt=millis();if(antennaFailed)antennaBatch=false;}
void loadAntenna(){
  uint32_t previous=antennaDataRadio==activeAddress?antennaSelectedId:0;antennaDataRadio=activeAddress;
  antennaSessions.clear();antennaSelectedId=antennaRunningId=0;antennaBatch=antennaSampleActive=false;antennaSample=-1;antennaDirty=antennaFailed=false;
  antennaDraft={};strcpy(antennaDraft.nameA,"A");strcpy(antennaDraft.nameB,"B");antennaInfo="";antennaPage=antennaPickPage=antennaSamplePage=0;
  if(storageReady&&LittleFS.exists(fieldPath(".antenna"))&&!readAntenna(antennaSessions))antennaFailed=true;
  for(auto& s:antennaSessions)if(antenna::recover(s))antennaDirty=true;
  if(antennaDirty)saveAntenna();
  for(const auto& s:antennaSessions)if(s.id==previous){antennaSelectedId=previous;break;}
}
antenna::Session* selectedAntenna(uint32_t id){for(auto& s:antennaSessions)if(s.id==id)return &s;return nullptr;}
bool antennaStorageSelfTest(){
  if(!storageReady||antennaBatch||antennaSampleActive)return false;
  auto saved=std::move(antennaSessions);String path=journalPath;journalPath="/meshpilot-antenna-fixture.log";antennaSessions.clear();
  antenna::Session s;s.id=1;s.key[0]=7;strcpy(s.place,"Fixture hill");strcpy(s.nameA,"Stock");strcpy(s.nameB,"Whip");strcpy(s.profile,"Fixture radio settings");
  s.phase=2;s.power=20;for(int p=0;p<3;p++){s.count[p]=antenna::SLOTS;for(int i=0;i<antenna::SLOTS;i++){auto& r=s.samples[p][i];r.state=antenna::State::COMPLETE;r.accepted=true;r.heard=true;r.outbound=-13;r.inbound=-9;r.rssi=-104;}}
  for(int i=0;i<antenna::SESSIONS;i++){s.id=i+1;antennaSessions.push_back(s);}
  std::vector<antenna::Session> loaded;bool ok=writeAntenna()&&readAntenna(loaded)&&loaded.size()==antenna::SESSIONS&&loaded.back().samples[2][7].rssi==-104;
  antennaSessions.resize(1);antennaSessions[0].count[2]=1;antennaSessions[0].samples[2][0].state=antenna::State::LISTENING;
  ok=writeAntenna()&&readAntenna(loaded)&&loaded.size()==1&&antenna::recover(loaded[0])&&loaded[0].samples[2][0].state==antenna::State::INTERRUPTED&&ok;
  File bad=LittleFS.open(fieldPath(".antenna"),"r+");if(bad){bad.seek(8+antenna::META+7);bad.write(uint8_t(0xff));bad.close();}else ok=false;
  ok=!readAntenna(loaded)&&loaded.size()==1&&ok;
  bad=LittleFS.open(fieldPath(".antenna"),"w");if(bad){bad.print("MPA1");bad.close();}else ok=false;ok=!readAntenna(loaded)&&ok;
  const String paths[]={fieldPath(".antenna"),fieldPath(".antenna")+".tmp"};for(const auto& p:paths)if(LittleFS.exists(p))LittleFS.remove(p);
  journalPath=path;antennaSessions=std::move(saved);return ok;
}
