#pragma once
struct BotChannel {uint8_t key[16]={};char place[25]="Ramsbottom";};
BotChannel botChannels[40];bool botEnabled[40]={},botDrained=false,botQueued=false,botSaveFailed=false;
String botRadio,botReply,botInfo;uint8_t botQueuedChannel=0;uint32_t botDue=0,botExpires=0;
int botPage=0,botSelected=0;bot::Limits botLimits;
bool writeBotSettings(const String& path){
  FieldWriter w(path,"MPB1",40);for(const auto& c:botChannels){w.put(c.key,16);w.put(reinterpret_cast<const uint8_t*>(c.place),25);}return w.finish();
}
bool readBotSettings(const String& path,BotChannel* output){
  FieldReader r(path,"MPB1",40,1652);BotChannel loaded[40];
  if(r.count!=40)r.ok=false;
  for(auto& c:loaded){r.get(c.key,16);r.get(reinterpret_cast<uint8_t*>(c.place),25);if(c.place[24])r.ok=false;for(int i=0;i<24&&c.place[i];i++)if(uint8_t(c.place[i])<32||uint8_t(c.place[i])>126)r.ok=false;}
  if(!r.finish())return false;memcpy(output,loaded,sizeof(loaded));return true;
}
void resetBotLink(){botDrained=false;botQueued=false;botReply="";}
void loadBot(){
  resetBotLink();if(botRadio==activeAddress)return;
  botRadio=activeAddress;memset(botEnabled,0,sizeof(botEnabled));for(auto& c:botChannels)c=BotChannel{};
  botSaveFailed=false;if(LittleFS.exists(fieldPath(".bot"))&&!readBotSettings(fieldPath(".bot"),botChannels))botSaveFailed=true;
}
bool botChannelValid(int slot){return slot>=0&&slot<maxChannels&&channels[slot].length()&&haveChannelKey[slot]&&!memcmp(botChannels[slot].key,channelKeys[slot],16);}
bool saveBotSettings(){botSaveFailed=!writeBotSettings(fieldPath(".bot"));if(botSaveFailed)botInfo="Settings not saved; bot stays off.";return !botSaveFailed;}
void saveBotLocation(const String& text){
  botEnabled[botSelected]=false;if(botQueuedChannel==botSelected)botQueued=false;
  String value=text;value.trim();memset(botChannels[botSelected].place,0,25);value.substring(0,24).toCharArray(botChannels[botSelected].place,25);
  saveBotSettings();
}
void botQueueDrained(){botDrained=true;}
void botIncoming(const Reply& reply){
  if(fixtureMode||!ready||!linkActive||!botDrained||botQueued||storageBlocked)return;
  protocol::MessageView view;if(!protocol::message(reply.data,reply.length,view)||view.direct||view.channel>=40)return;
  int slot=view.channel;if(!botEnabled[slot]||!botChannelValid(slot))return;
  bot::Trigger trigger;if(!bot::parse(reply.data,reply.length,trigger)||trigger.sender==wireName.c_str())return;
  uint32_t sender=bot::hash(reinterpret_cast<const uint8_t*>(trigger.sender.data()),trigger.sender.size());
  if(!botLimits.allow(millis(),trigger.identity,sender))return;
  // Reserve before scheduling; cancelled/unconfirmed replies are never retried.
  botLimits.reserve(millis(),trigger.identity,sender);
  botReply="[bot] Seen";if(strlen(botChannels[slot].place))botReply+=" in "+String(botChannels[slot].place);
  botReply+=trigger.hops<0?"; hops unknown":trigger.hops==0?"; direct (0 hops)":"; "+String(trigger.hops)+" hop"+(trigger.hops==1?"":"s");
  botReply+=trigger.hasSnr?"; last-hop SNR "+String(trigger.snr/4.0f,2)+" dB":"; SNR unavailable";
  botReply+=". RX only.";
  botQueuedChannel=slot;botQueued=true;botDue=millis()+3000+(esp_random()%5001);botExpires=millis()+30000;
  botInfo="Reply waiting for a free radio.";Serial.println("Bot trigger matched; one bounded reply scheduled");
}
void tickBot(){
  if(fixtureMode)return;
  for(int i=0;i<40;i++)if(ready&&botEnabled[i]&&!botChannelValid(i)){botEnabled[i]=false;botInfo="Channel changed; enable bot again.";}
  if(!botQueued)return;
  if(!ready||!linkActive||!botDrained||!botEnabled[botQueuedChannel]||!botChannelValid(botQueuedChannel)||storageBlocked||terminal_tools::due(millis(),botExpires)){
    botQueued=false;botInfo="Pending reply cancelled; no retry.";return;
  }
  if(!terminal_tools::due(millis(),botDue)||pending!=Pending::NONE||networkAction!=NetworkAction::NONE||remoteWaiting||nearby.active||antennaBatch||antennaSampleActive||manageAction!=ManageAction::NONE)return;
  uint32_t stamp=max(nowSeconds(),lastOutgoingTimestamp+1);
  int watch=createSendWatch(stamp,false,botQueuedChannel,nullptr,channels[botQueuedChannel],botReply,true);if(watch<0)return;
  pendingSend=watch;lastOutgoingTimestamp=stamp;uint8_t packet[133]={3,0,botQueuedChannel};protocol::write32(packet+3,stamp);
  size_t length=min(size_t(120),size_t(botReply.length()));memcpy(packet+7,botReply.c_str(),length);botQueued=false;
  botInfo="Reply requested; check Net > Sent.";
  if(!sendCommand(packet,length+7,Pending::SEND)){
    recordOutgoing(false);sends[watch].timedOut=true;updateSendResult(stamp,field::SendState::UNCONFIRMED);pendingSend=-1;
    failLink("Bot send unconfirmed; no automatic retry.");
  }else deliveryFeedback("Bot reply requested; not delivery",0x5C7F);
}
void drawBotChannels(){
  screen=Screen::BOT_CHANNELS;header("Bots / per channel");auto list=activeChannels();botPage=constrain(botPage,0,max(0,int((list.size()+3)/4)-1));
  for(int row=0;row<4;row++){int item=botPage*4+row;if(item>=int(list.size()))break;int ch=list[item];String label=String(botEnabled[ch]&&botChannelValid(ch)?"ON  ":"OFF ")+channels[ch];smallButton(12,42+row*32,296,28,label.substring(0,32).c_str());}
  smallButton(12,177,68,28,"Prev");smallButton(84,177,68,28,"Next");smallButton(228,177,80,28,"Back");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Select a channel. Full restart turns bots off.",12,215,1);
}
void drawBotSetup(){
  screen=Screen::BOT_SETUP;header(("Bot: "+channels[botSelected]).substring(0,34).c_str());
  smallButton(12,42,296,30,("Location: "+String(botChannels[botSelected].place)).c_str());
  smallButton(12,80,296,32,botEnabled[botSelected]?"Bot ON - tap to stop":"Bot OFF - tap to enable");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Exact trigger: test / TEST / any case",12,121,1);lcd.drawString("Replies here only: place, hops, last-hop SNR",12,138,1);
  lcd.drawString("Max 1/min, 1/sender/5min, 6/hour overall",12,155,1);
  smallButton(12,177,100,28,"Back");lcd.setTextColor(botSaveFailed?TFT_ORANGE:TFT_WHITE,BG);
  lcd.drawString(botInfo.length()?botInfo.substring(0,49):"No old-message replies; no automatic retries.",12,215,1);
}
bool botTouch(int x,int y){
  if(screen==Screen::BOT_CHANNELS){auto list=activeChannels();
    if(x>=12&&x<=308&&y>=42&&y<170&&(y-42)%32<28){int item=botPage*4+(y-42)/32;if(item<int(list.size())){botSelected=list[item];botInfo="";drawBotSetup();}}
    else if(y>=177&&y<=205){if(x>=12&&x<=80){botPage--;drawBotChannels();}else if(x>=84&&x<=152){botPage++;drawBotChannels();}else if(x>=228&&x<=308)drawSettings();}return true;
  }
  if(screen!=Screen::BOT_SETUP)return false;
  if(x>=12&&x<=308&&y>=42&&y<=72){textPurpose=9;editorReturn=Screen::BOT_SETUP;editText=botChannels[botSelected].place;textLimit=24;upperCase=symbols=false;drawTextEditor();}
  else if(x>=12&&x<=308&&y>=80&&y<=112){
    if(botEnabled[botSelected]){botEnabled[botSelected]=false;if(botQueuedChannel==botSelected)botQueued=false;botInfo="Off. Already-sent replies cannot be recalled.";}
    else if(ready&&haveChannelKey[botSelected]&&!storageBlocked){memcpy(botChannels[botSelected].key,channelKeys[botSelected],16);if(saveBotSettings()){botEnabled[botSelected]=true;resetBotLink();lastPoll=millis()-3000;botInfo="On after old messages finish loading.";}}
    else botInfo="Connect the radio and check storage first.";drawBotSetup();
  }else if(x>=12&&x<=112&&y>=177&&y<=205)drawBotChannels();return true;
}
int activeBotCount(){int count=0;for(int i=0;i<40;i++)if(botEnabled[i]&&botChannelValid(i))count++;return count;}
void botStatus(){int count=activeBotCount();Serial.printf("Bot status: enabledChannels=%d; drained=%d; queued=%d; settingsFailed=%d; attempts=%u\n",count,botDrained,botQueued,botSaveFailed,botLimits.count);}
bool botStorageSelfTest(){
  if(!storageReady)return false;String savedJournal=journalPath;journalPath="/fixture-bot-test.log";const String path="/fixture-bot-test.bin";BotChannel saved[40],loaded[40];memcpy(saved,botChannels,sizeof(saved));
  for(int i=0;i<40;i++){memset(botChannels[i].key,i+1,16);snprintf(botChannels[i].place,25,"Fixture %d",i);}
  bool ok=writeBotSettings(path)&&readBotSettings(path,loaded)&&!memcmp(loaded,botChannels,sizeof(loaded));
  File f=LittleFS.open(path,"r+");if(f){f.seek(12);f.write(uint8_t(0xff));f.close();}else ok=false;
  ok=ok&&!readBotSettings(path,loaded);memcpy(botChannels,saved,sizeof(saved));LittleFS.remove(path);LittleFS.remove(path+".tmp");journalPath=savedJournal;return ok;
}

bool botUiSelfTest(){
  // Boot-only fixtures. No send dispatcher is called and flash writes are disabled.
  if(!fixtureMode||client)return false;
  bool originalFixture=fixtureMode;String originalDraft=draft;Screen originalScreen=screen;
  ready=linkActive=true;maxChannels=2;channels[0]="Fixture";channels[1]="Other";
  haveChannelKey[0]=haveChannelKey[1]=true;memset(channelKeys,0,sizeof(channelKeys));
  botEnabled[0]=true;botEnabled[1]=false;wireName="Self";
  Reply r{};const uint8_t frame[]={17,20,0,0,0,2,0,1,0,0,0,'A',':',' ','T','e','S','t'};
  r.code=17;r.length=sizeof(frame);memcpy(r.data,frame,sizeof(frame));fixtureMode=false;
  botIncoming(r);bool ok=!botQueued;botQueueDrained();r.data[4]=1;botIncoming(r);ok=ok&&!botQueued;
  r.data[4]=0;botIncoming(r);ok=ok&&botQueued&&botQueuedChannel==0&&botReply=="[bot] Seen in Ramsbottom; 2 hops; last-hop SNR 5.00 dB. RX only.";
  resetBotLink();botQueueDrained();botIncoming(r);ok=ok&&!botQueued; // duplicate
  r.data[7]=2;botEnabled[0]=false;botIncoming(r);ok=ok&&!botQueued;
  fixtureMode=true;draft="Unsent family message";currentChannel=1;composingDirect=true;
  int slot=createSendWatch(42,false,0,nullptr,"Fixture","[bot] received",true);
  ok=ok&&slot>=0&&sends[slot].automatic&&!sends[slot].direct&&sends[slot].channel==0&&sends[slot].text=="[bot] received"&&draft=="Unsent family message"&&composingDirect&&currentChannel==1;
  for(auto& watch:sends)watch=SendWatch{};latestSend=-1;ready=linkActive=false;maxChannels=0;
  channels[0]=channels[1]="";haveChannelKey[0]=haveChannelKey[1]=false;wireName="";currentChannel=0;composingDirect=false;
  memset(botEnabled,0,sizeof(botEnabled));botLimits=bot::Limits{};resetBotLink();botInfo="";
  draft=originalDraft;screen=originalScreen;fixtureMode=originalFixture;return ok;
}
