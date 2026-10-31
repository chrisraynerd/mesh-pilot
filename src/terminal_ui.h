// Standalone terminal conveniences; no RF probe or send is performed here.
uint32_t readForPeer(const uint8_t* key) {
  for(auto& peer:readPeers)if(!memcmp(peer.key,key,6))return peer.sequence;return 0;
}
int channelUnread(uint8_t channel) {
  if(channel>=40)return 0;int count=0;
  for(auto& m:messages)if(!m.direct&&!m.outgoing&&m.channel==channel&&m.sequence>channelRead[channel])count++;
  return count;
}
int peerUnread(const uint8_t* key) {
  uint32_t read=readForPeer(key);int count=0;
  for(auto& m:messages)if(m.direct&&!m.outgoing&&!memcmp(m.peer,key,6)&&m.sequence>read)count++;
  return count;
}
int directUnread() {
  int count=0;for(auto& m:messages)if(m.direct&&!m.outgoing&&m.sequence>readForPeer(m.peer))count++;return count;
}
int allUnread() {
  int count=directUnread();for(int i=0;i<40;i++)count+=channelUnread(i);return count;
}
void noteRead(const ChatMessage& message) {
  if(message.outgoing||!message.sequence||displayDimmed)return;
  bool changed=false;
  if(!message.direct&&message.channel<40) {
    if(channelRead[message.channel]<message.sequence){channelRead[message.channel]=message.sequence;changed=true;}
  } else if(message.direct) {
    ReadPeer* found=nullptr;for(auto& peer:readPeers)if(!memcmp(peer.key,message.peer,6)){found=&peer;break;}
    if(!found) {
      if(readPeers.size()>=80)readPeers.erase(std::min_element(readPeers.begin(),readPeers.end(),[](const ReadPeer& a,const ReadPeer& b){return a.sequence<b.sequence;}));
      readPeers.push_back({{},0});found=&readPeers.back();memcpy(found->key,message.peer,6);
    }
    if(found->sequence<message.sequence){found->sequence=message.sequence;changed=true;}
  }
  if(changed&&!readDirty){readDirty=true;readDirtyAt=millis();}
}
String readStatePath(){String path=journalPath;path.replace(".log",".read");return path;}
void loadReadState() {
  memset(channelRead,0,sizeof(channelRead));readPeers.clear();readDirty=false;readSaveFailed=false;
  if(!storageReady)return;
  File file=LittleFS.open(readStatePath(),"r");if(!file)return;
  uint8_t bytes[969];size_t length=file.size();
  if(length<169||length>sizeof(bytes)||file.read(bytes,length)!=length){file.close();return;}file.close();
  if(memcmp(bytes,"MPR1",4)||bytes[164]>80||length!=size_t(169+bytes[164]*10)||protocol::read32(bytes+length-4)!=terminal_tools::checksum(bytes,length-4))return;
  for(int i=0;i<40;i++)channelRead[i]=protocol::read32(bytes+4+i*4);
  for(int i=0;i<bytes[164];i++){ReadPeer peer{};memcpy(peer.key,bytes+165+i*10,6);peer.sequence=protocol::read32(bytes+171+i*10);readPeers.push_back(peer);}
}
void saveReadState() {
  if(!readDirty||!storageReady||!journalPath.length())return;
  uint8_t bytes[969]={};memcpy(bytes,"MPR1",4);
  for(int i=0;i<40;i++)protocol::write32(bytes+4+i*4,channelRead[i]);bytes[164]=readPeers.size();
  for(size_t i=0;i<readPeers.size();i++){memcpy(bytes+165+i*10,readPeers[i].key,6);protocol::write32(bytes+171+i*10,readPeers[i].sequence);}
  size_t length=169+readPeers.size()*10;protocol::write32(bytes+length-4,terminal_tools::checksum(bytes,length-4));
  String path=readStatePath(),temporary=path+".tmp";File file=LittleFS.open(temporary,"w");
  bool ok=file&&file.write(bytes,length)==length;if(file){file.flush();file.close();}
  if(ok)ok=LittleFS.rename(temporary,path);
  if(!ok){LittleFS.remove(temporary);readSaveFailed=true;readDirtyAt=millis();return;}
  readDirty=false;readSaveFailed=false;
}
String idleStatus() {
  String result=ready?"Radio linked":"Radio disconnected";
  if(haveBattery&&batteryMv)result+=" | "+String(batteryMv/1000.0f,2)+"V";
  int unread=allUnread();if(unread)result+=" | "+String(unread)+" unread";
  if(readSaveFailed)result="Read markers not saved; check storage";
  if(draftSaveFailed)result="Draft not saved; check storage";
  if(sendResultsFailed)result="Send results not saved; check storage";
  if(coverageFailed)result="Coverage notes not saved; check storage";
  return result;
}
void scheduleReconnect() {
  String saved=prefs.getString("radioAddr","");
  if(!autoConnectEnabled||reconnectSuppressed||!saved.length()||activeAddress!=saved)return;
  uint32_t delay=terminal_tools::retryDelay(reconnectAttempt);if(reconnectAttempt<4)reconnectAttempt++;
  reconnectAt=millis()+delay;reconnectScheduled=true;reconnectPaintAt=millis()-1000;
  Serial.printf("Automatic reconnect scheduled in %lu seconds\n",static_cast<unsigned long>(delay/1000));
}
void cancelReconnect(){reconnectScheduled=false;reconnectSuppressed=true;}
void reconnectReady() {
  reconnectScheduled=false;reconnectAttempt=0;reconnectSuppressed=false;
  bool restored=restoreDraft();
  if(restored||(resumeAfterReconnect&&reconnectReturn==Screen::COMPOSE&&draft.length()&&(composingDirect||(currentChannel<maxChannels&&channels[currentChannel].length()))))drawCompose();
  else if(resumeAfterReconnect&&reconnectReturn==Screen::DIRECT_CHAT)drawDirectChat();
  else if(resumeAfterReconnect&&reconnectReturn==Screen::CHAT&&currentChannel<maxChannels&&channels[currentChannel].length())drawChat();
  else drawChannels();
  resumeAfterReconnect=false;Serial.println("Terminal ready; radio session restored; no automatic resend");
}
void drawSettings() {
  screen=Screen::SETTINGS;header("Mesh Pilot / setup");lcd.setTextColor(TFT_WHITE,BG);
  String reading=!batterySupported?"Battery reading unsupported":!haveBattery?"Reading Heltec battery...":batteryMv?"Heltec battery: "+String(batteryMv/1000.0f,2)+" V":"Heltec battery: unavailable on this board";
  lcd.drawString(reading,12,44,1);
  lcd.drawString(radioStorageTotal?"Heltec storage: "+String(radioStorageUsed)+" / "+String(radioStorageTotal)+" KB":"Heltec storage: not reported",12,64,1);
  lcd.drawString(storageReady?"Screen storage: "+String(LittleFS.usedBytes()/1024)+" / "+String(LittleFS.totalBytes()/1024)+" KB":"Screen storage unavailable",12,84,1);
  lcd.drawString("Screen brightness",12,105,1);
  smallButton(12,120,64,29,"-");smallButton(244,120,64,29,"+");lcd.setTextColor(TFT_WHITE,BG);lcd.drawCentreString((String((unsigned(brightness)*100+127)/255)+"%").c_str(),160,126,2);
  smallButton(12,157,142,29,autoDim?"Dim after 1 min":"Dimming off");smallButton(160,157,148,29,autoConnectEnabled?"Auto link on":"Auto link off");
  smallButton(12,198,92,30,"Back");smallButton(110,198,92,30,"Clock");smallButton(208,198,100,30,"Refresh");
}
bool terminalTouch(int x,int y) {
  if(screen!=Screen::SETTINGS)return false;
  if(y>=120&&y<=149) {
    if(x>=12&&x<=76)brightness=uint8_t(max(40,int(brightness)-43));
    else if(x>=244&&x<=308)brightness=uint8_t(min(255,int(brightness)+43));else return true;
    prefs.putUChar("light",brightness);ledcWrite(7,brightness);drawSettings();
  } else if(y>=157&&y<=186) {
    if(x>=12&&x<=154){autoDim=!autoDim;prefs.putBool("dim",autoDim);}
    else if(x>=160&&x<=308){autoConnectEnabled=!autoConnectEnabled;prefs.putBool("autolink",autoConnectEnabled);if(!autoConnectEnabled)cancelReconnect();else reconnectSuppressed=false;}
    drawSettings();
  } else if(y>=198&&y<=228) {
    if(x>=12&&x<=104)drawChannels();else if(x>=110&&x<=202){clockDigits="";drawClock();}
    else if(x>=208&&x<=308){batteryWanted=true;batterySupported=true;drawSettings();}
  }return true;
}
bool handleTerminalReply(const Reply& reply) {
  if(pending!=Pending::BATTERY||reply.code!=12)return false;
  uint16_t mv;uint32_t used,total;
  if(!terminal_tools::battery(reply.data,reply.length,mv,used,total))return true;
  pending=Pending::NONE;haveBattery=true;batteryMv=mv;radioStorageUsed=used;radioStorageTotal=total;batteryAt=millis();
  Serial.printf("Heltec battery: %u mV; storage: %lu/%lu KB\n",batteryMv,static_cast<unsigned long>(used),static_cast<unsigned long>(total));
  if(screen==Screen::SETTINGS)drawSettings();else paintFooter();return true;
}
void tickTerminal() {
  if(reconnectScheduled&&!linkActive&&pending==Pending::NONE) {
    if(screen==Screen::LINK&&millis()-reconnectPaintAt>=1000) {
      lcd.fillRect(12,146,296,26,BG);lcd.setTextColor(ACCENT,BG);
      uint32_t remaining=terminal_tools::due(millis(),reconnectAt)?0:(reconnectAt-millis()+999)/1000;
      lcd.drawString(("Auto retry in "+String(remaining)+"s; Back cancels").c_str(),12,150,1);reconnectPaintAt=millis();
    }
    if(terminal_tools::due(millis(),reconnectAt))connectRadio(true);
  }
  if(readDirty&&millis()-readDirtyAt>=3000)saveReadState();
  tickDraft();
  if(autoDim&&!displayDimmed&&millis()-lastTouchAt>=60000){displayDimmed=true;ledcWrite(7,8);}
  if(ready&&batterySupported&&pending==Pending::NONE&&networkAction==NetworkAction::NONE&&(batteryWanted||millis()-batteryAt>=60000)) {
    uint8_t request=20;batteryWanted=false;if(!sendCommand(&request,1,Pending::BATTERY))batteryWanted=true;
  }
}
bool terminalSelfTest() {
  // Isolated RAM fixtures: no live messages, radio changes or flash writes.
  messages.clear();incomingSequence=0;memset(channelRead,0,sizeof(channelRead));readPeers.clear();readDirty=false;
  uint8_t alice[6]={1},bob[6]={2};
  remember(2,false,"A",1);remember(3,false,"B",2);remember(255,true,"Hi",3,alice);remember(255,true,"Yo",4,bob);remember(2,false,"Me: reply",5,nullptr,true);
  bool ok=allUnread()==4&&channelUnread(2)==1&&peerUnread(alice)==1&&directUnread()==2;
  noteRead(messages[0]);noteRead(messages[2]);ok=ok&&allUnread()==2&&channelUnread(2)==0&&peerUnread(alice)==0&&peerUnread(bob)==1&&channelUnread(3)==1;
  noteRead(messages[4]);ok=ok&&allUnread()==2&&readDirty;
  displayDimmed=true;noteRead(messages[1]);ok=ok&&channelUnread(3)==1;displayDimmed=false;
  messages.clear();readPeers.clear();memset(channelRead,0,sizeof(channelRead));incomingSequence=0;readDirty=false;readDirtyAt=0;
  return ok;
}
