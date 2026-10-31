String draftStatePath(){String path=journalPath;path.replace(".log",".draft");return path;}
bool writeDraftFile(const draft_tools::Draft& saved) {
  uint8_t bytes[203];size_t length=draft_tools::encode(saved,bytes);if(!length)return false;
  String path=draftStatePath(),temporary=path+".tmp";File file=LittleFS.open(temporary,"w");
  bool ok=file&&file.write(bytes,length)==length;if(file){file.flush();file.close();}
  if(ok)ok=LittleFS.rename(temporary,path);if(!ok)LittleFS.remove(temporary);return ok;
}
bool loadDraftFile(draft_tools::Draft& saved) {
  File file=LittleFS.open(draftStatePath(),"r");if(!file)return false;uint8_t bytes[203];size_t length=file.size();
  bool ok=length<=sizeof(bytes)&&file.read(bytes,length)==length;file.close();
  return ok&&draft_tools::decode(bytes,length,saved);
}
bool captureDraft(draft_tools::Draft& saved) {
  if(!draft.length()||draft.length()>120)return false;
  saved={};saved.direct=composingDirect;saved.channel=currentChannel;
  if(composingDirect)memcpy(saved.peer,directPeer,32);
  else {if(currentChannel>=maxChannels||!haveChannelKey[currentChannel])return false;if(!draft_tools::fingerprint(channelKeys[currentChannel],saved.channelHash))return false;}
  String name=composingDirect?directName:channels[currentChannel];memcpy(saved.name,name.c_str(),min(size_t(32),size_t(name.length())));
  memcpy(saved.text,draft.c_str(),draft.length());return true;
}
void clearSavedDraft() {
  if(storageReady&&journalPath.length()&&LittleFS.exists(draftStatePath())&&!LittleFS.remove(draftStatePath())){draftSaveFailed=true;return;}
  draftSavedHash=draftObservedHash=0;draftDirty=false;draftSaveFailed=false;
}
void saveDraftNow() {
  if(!storageReady||!journalPath.length())return;
  if(!draft.length()){clearSavedDraft();return;}
  draft_tools::Draft saved{};if(!captureDraft(saved))return;
  uint8_t bytes[203];size_t length=draft_tools::encode(saved,bytes);uint32_t hash=terminal_tools::checksum(bytes,length);
  if(hash==draftSavedHash&&!draftSaveFailed)return;
  if(!writeDraftFile(saved)){draftSaveFailed=true;draftChangedAt=millis();return;}
  draftSavedHash=draftObservedHash=hash;draftDirty=false;draftSaveFailed=false;
  Serial.println("Unsent draft saved locally; no automatic send");
}
bool restoreDraft() {
  if(draft.length()||!storageReady)return false;
  draft_tools::Draft saved{};if(!loadDraftFile(saved))return false;
  if(!saved.direct) {
    if(saved.channel>=maxChannels||!channels[saved.channel].length()||!haveChannelKey[saved.channel])return false;
    uint8_t hash[8];if(!draft_tools::fingerprint(channelKeys[saved.channel],hash)||memcmp(hash,saved.channelHash,8))return false;
  }
  composingDirect=saved.direct;currentChannel=saved.channel;memcpy(directPeer,saved.peer,32);directName=saved.name;draft=saved.text;
  uint8_t bytes[203];size_t length=draft_tools::encode(saved,bytes);draftSavedHash=draftObservedHash=terminal_tools::checksum(bytes,length);draftDirty=false;
  statusLine="Draft restored; review before sending";Serial.println("Unsent draft restored; waiting for user Send");return true;
}
void tickDraft() {
  if(!ready||!storageReady||!journalPath.length())return;
  uint32_t hash=0;
  if(draft.length()) {draft_tools::Draft saved{};if(!captureDraft(saved))return;uint8_t bytes[203];size_t length=draft_tools::encode(saved,bytes);hash=terminal_tools::checksum(bytes,length);}
  if(hash!=draftObservedHash){draftObservedHash=hash;draftChangedAt=millis();draftDirty=true;}
  if(draftDirty&&millis()-draftChangedAt>=2000)saveDraftNow();
}
bool terminalStorageSelfTest() {
  if(!storageReady)return false;
  // Reserved synthetic namespace; normal user journals use a radio MAC filename.
  String originalPath=journalPath;journalPath="/meshpilot-selftest.log";
  uint32_t originalChannels[40];memcpy(originalChannels,channelRead,sizeof(channelRead));auto originalPeers=readPeers;
  bool originalDirty=readDirty,originalFailure=readSaveFailed;uint32_t originalAt=readDirtyAt;
  memset(channelRead,0,sizeof(channelRead));channelRead[3]=42;readPeers.clear();readPeers.push_back({{1,2,3,4,5,6},77});readDirty=true;
  saveReadState();bool ok=!readDirty&&!readSaveFailed;memset(channelRead,0,sizeof(channelRead));readPeers.clear();loadReadState();
  ok=ok&&channelRead[3]==42&&readPeers.size()==1&&readPeers[0].sequence==77;
  channelRead[3]=43;readDirty=true;saveReadState();loadReadState();ok=ok&&channelRead[3]==43;
  File corrupt=LittleFS.open(readStatePath(),"w");if(corrupt){corrupt.print("invalid");corrupt.close();}else ok=false;
  loadReadState();ok=ok&&channelRead[3]==0&&readPeers.empty();
  draft_tools::Draft sample{},loaded{};sample.channel=1;strcpy(sample.name,"#fixture");strcpy(sample.text,"Local unsent draft fixture");
  ok=writeDraftFile(sample)&&loadDraftFile(loaded)&&!strcmp(sample.text,loaded.text)&&ok;
  strcpy(sample.text,"Replaced fixture");ok=writeDraftFile(sample)&&loadDraftFile(loaded)&&!strcmp(sample.text,loaded.text)&&ok;
  // Exercise the production restore path without writing to a user journal.
  String originalDraft=draft,originalName=directName,originalStatus=statusLine,originalChannel=channels[1];
  uint8_t originalPeer[32],originalKey[16];memcpy(originalPeer,directPeer,32);memcpy(originalKey,channelKeys[1],16);
  uint8_t originalCurrent=currentChannel,originalMaximum=maxChannels;bool originalDirect=composingDirect,originalHaveKey=haveChannelKey[1];
  uint32_t originalSavedHash=draftSavedHash,originalObservedHash=draftObservedHash,originalChangedAt=draftChangedAt;
  bool originalDraftDirty=draftDirty,originalDraftFailure=draftSaveFailed;
  maxChannels=40;channels[1]="#fixture";haveChannelKey[1]=true;memset(channelKeys[1],0x42,16);draft="";
  draft_tools::fingerprint(channelKeys[1],sample.channelHash);ok=writeDraftFile(sample)&&restoreDraft()&&draft==sample.text&&!composingDirect&&currentChannel==1&&ok;
  draft="";sample.channelHash[0]^=1;ok=writeDraftFile(sample)&&!restoreDraft()&&!draft.length()&&ok;
  sample.direct=true;sample.peer[0]=9;ok=writeDraftFile(sample)&&restoreDraft()&&composingDirect&&directPeer[0]==9&&ok;
  draft=originalDraft;directName=originalName;statusLine=originalStatus;channels[1]=originalChannel;maxChannels=originalMaximum;
  memcpy(directPeer,originalPeer,32);memcpy(channelKeys[1],originalKey,16);currentChannel=originalCurrent;composingDirect=originalDirect;haveChannelKey[1]=originalHaveKey;
  draftSavedHash=originalSavedHash;draftObservedHash=originalObservedHash;draftChangedAt=originalChangedAt;draftDirty=originalDraftDirty;draftSaveFailed=originalDraftFailure;
  corrupt=LittleFS.open(draftStatePath(),"w");if(corrupt){corrupt.print("invalid");corrupt.close();}else ok=false;
  ok=!loadDraftFile(loaded)&&ok;
  const String cleanup[]={readStatePath(),readStatePath()+".tmp",draftStatePath(),draftStatePath()+".tmp"};
  for(const auto& path:cleanup)if(LittleFS.exists(path))LittleFS.remove(path);
  journalPath=originalPath;memcpy(channelRead,originalChannels,sizeof(channelRead));readPeers=originalPeers;readDirty=originalDirty;readSaveFailed=originalFailure;readDirtyAt=originalAt;
  return ok;
}
