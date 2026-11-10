#pragma once
struct BotChannel {uint8_t key[16]={};uint16_t flags=3;char place[25]="Ramsbottom";char test[17]="test,testing",words[2][17]={"info,about",""},replies[2][101]={"Mesh Pilot community node.",""};uint8_t interval=30;};
BotChannel botChannels[40];bool botEnabled[40]={},botDrained=false,botQueued=false,botSaveFailed=false;
String botRadio,botReply,botInfo;uint8_t botQueuedChannel=0;uint32_t botDue=0,botExpires=0;
int botPage=0,botSelected=0,botCommandSelected=0,botCommandPage=0;bot::Limits botLimits;
bool writeBotSettings(const String& path){
  FieldWriter w(path,"MPB5",40);for(const auto& c:botChannels){w.put(c.key,16);w.put(reinterpret_cast<const uint8_t*>(c.place),25);uint8_t flags[2]={uint8_t(c.flags),uint8_t(c.flags>>8)};w.put(flags,2);w.put(reinterpret_cast<const uint8_t*>(c.test),17);w.put(reinterpret_cast<const uint8_t*>(c.words),34);w.put(reinterpret_cast<const uint8_t*>(c.replies),202);w.put(&c.interval,1);}return w.finish();
}
bool readBotSettings(const String& path,BotChannel* output,BotChannel* scratch=nullptr){
  File f=LittleFS.open(path,"r");uint8_t magic[4]={};if(!f||f.read(magic,4)!=4){if(f)f.close();return false;}f.close();bool legacy=!memcmp(magic,"MPB1",4);bool v5=!memcmp(magic,"MPB5",4);bool v4=!memcmp(magic,"MPB4",4);bool latest=v5||v4||!memcmp(magic,"MPB3",4);
  FieldReader r(path,legacy?"MPB1":v5?"MPB5":v4?"MPB4":latest?"MPB3":"MPB2",40,11892);std::unique_ptr<BotChannel[]> allocated;if(!scratch){allocated.reset(new(std::nothrow) BotChannel[40]);scratch=allocated.get();}if(!scratch||scratch==output)return false;BotChannel* loaded=scratch;for(int i=0;i<40;i++)loaded[i]=BotChannel{};if(r.count!=40)r.ok=false;
  auto validText=[](const char* text,int size){if(text[size-1])return false;for(int i=0;i<size-1&&text[i];i++)if(uint8_t(text[i])<32||uint8_t(text[i])>126)return false;return true;};
  for(int row=0;row<40;row++){auto& c=loaded[row];r.get(c.key,16);r.get(reinterpret_cast<uint8_t*>(c.place),25);
    if(!legacy){uint8_t flags[2]={};r.get(flags,v5?2:1);c.flags=flags[0]|uint16_t(flags[1])<<8;r.get(reinterpret_cast<uint8_t*>(c.test),17);r.get(reinterpret_cast<uint8_t*>(c.words),34);r.get(reinterpret_cast<uint8_t*>(c.replies),202);if(v4||v5)r.get(&c.interval,1);}
    if(!bot::validInterval(c.interval)||!validText(c.place,25)||c.flags>(v5?2047:latest?31:15)||!validText(c.test,17)||!bot::aliases(c.test))return false;
    for(int i=0;i<2;i++){if(!validText(c.words[i],17)||!validText(c.replies[i],101))return false;
      if(c.words[i][0]&&(!bot::aliases(c.words[i])||bot::overlap(c.words[i],c.test)||(i&& bot::overlap(c.words[i],c.words[0]))))r.ok=false;
      if((c.flags&(4<<i))&&(!c.words[i][0]||!c.replies[i][0]))r.ok=false;}
  }
  if(!r.finish())return false;
  if(!v5)for(int i=0;i<40;i++)if(!strcmp(loaded[i].test,"test")&&!bot::overlap("testing",loaded[i].words[0])&&!bot::overlap("testing",loaded[i].words[1]))strcpy(loaded[i].test,"test,testing");memcpy(output,loaded,sizeof(BotChannel)*40);return true;
}
void resetBotLink(){cancelBotWeather();botDrained=false;botQueued=false;botReply="";}
void loadBot(){
  resetBotLink();if(botRadio==activeAddress)return;
  botRadio=activeAddress;botLimits=bot::Limits{};memset(botEnabled,0,sizeof(botEnabled));for(auto& c:botChannels)c=BotChannel{};
  botSaveFailed=false;if(LittleFS.exists(fieldPath(".bot"))&&!readBotSettings(fieldPath(".bot"),botChannels))botSaveFailed=true;
}
bool botChannelValid(int slot){return slot>=0&&slot<maxChannels&&channels[slot].length()&&haveChannelKey[slot]&&!memcmp(botChannels[slot].key,channelKeys[slot],16);}
bool saveBotSettings(){botSaveFailed=!writeBotSettings(fieldPath(".bot"));if(botSaveFailed){memset(botEnabled,0,sizeof(botEnabled));botInfo="Settings not saved; bots paused.";}return !botSaveFailed;}
void saveBotLocation(const String& text){
  stopEditedBot();
  String value=text;value.trim();memset(botChannels[botSelected].place,0,25);value.substring(0,24).toCharArray(botChannels[botSelected].place,25);
  saveBotSettings();
}
void botQueueDrained(){botDrained=true;}
void botNotice(int channel,const String& text){
  if(channel==botSelected){botInfo=text;if(!fixtureMode&&(screen==Screen::BOT_SETUP||screen==Screen::WEATHER_SETTINGS)){lcd.fillRect(12,211,296,24,BG);lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(text.substring(0,49),12,215,1);}}
  if(!fixtureMode)Serial.printf("Bot slot %d: %s\n",channel+1,text.c_str());
}
void botIncoming(const Reply& reply){
  if(fixtureMode||!ready||!linkActive||!botDrained||botQueued||storageBlocked)return;
  protocol::MessageView view;if(!protocol::message(reply.data,reply.length,view)||view.direct||view.channel>=40)return;
  int slot=view.channel;if(!botEnabled[slot]||!botChannelValid(slot))return;
  bot::Trigger trigger;if(!bot::parseAny(reply.data,reply.length,trigger)||trigger.sender==wireName.c_str())return;
  auto& config=botChannels[slot];int kind=bot::command(trigger,config.flags,config.test,config.words[0],config.words[1]);std::string weatherPlace;
  if(kind<0&&(config.flags&16)&&weather::query(bot::normalized(trigger.body),weatherPlace))kind=4;
  if(kind<0)return;
  if(botLimits.duplicate(trigger.identity,slot)){botNotice(slot,"Repeat copy ignored (same radio message)");return;}
  uint32_t wait=botLimits.remaining(millis(),slot,config.interval);
  if(wait){botNotice(slot,"Reply interval: wait "+String((wait+999)/1000)+"s");return;}
  // Remember the received identity immediately; start the interval at dispatch.
  botLimits.remember(trigger.identity,slot);
  if(kind==0){botReply="[bot] Seen";if(strlen(botChannels[slot].place))botReply+=" in "+String(botChannels[slot].place);
  botReply+=trigger.hops<0?"; hops unknown":trigger.hops==0?"; direct (0 hops)":"; "+String(trigger.hops)+" hop"+(trigger.hops==1?"":"s");
  botReply+=trigger.hasSnr?"; last-hop SNR "+String(trigger.snr/4.0f,2)+" dB":"; SNR unavailable";
  botReply+=". RX only.";
  }else if(kind==1){botReply="[bot] Commands:";for(int i=0;i<=10;i++)if(config.flags&(1u<<i)){String aliases=i==0?config.test:i==2?config.words[0]:i==3?config.words[1]:bot::fixedAliases(i);int comma=aliases.indexOf(',');botReply+=" "+(comma<0?aliases:aliases.substring(0,comma));}}
  else if(kind==2||kind==3)botReply="[bot] "+String(config.replies[kind-2]);
  else if(kind==5)botReply="[bot] pong";
  else if(kind==6)botReply="[bot] ping";
  else if(kind==7||kind==8){botReply="[bot] Last Heltec battery: ";botReply+=haveBattery?String(batteryMv/1000.0f,2)+" V":"unavailable";if(kind==7)botReply+="; display uptime "+String(uint32_t(esp_timer_get_time()/60000000ULL))+" min. Repeater reach unconfirmed.";}
  else if(kind==9)botReply="[bot] Display uptime: "+String(uint32_t(esp_timer_get_time()/60000000ULL))+" min (since CYD restart).";
  else if(kind==10)botReply="[bot] Mesh Pilot 0.21.0 / CYD + MeshCore companion.";
  botQueuedChannel=slot;botQueued=true;botDue=millis()+(config.interval==0?250+(esp_random()%501):3000+(esp_random()%5001));botExpires=millis()+(kind==4?45000:30000);
  if(kind==4){botReply="";requestWeather(weatherPlace.empty()?String(config.place):String(weatherPlace.c_str()),true);}
  botNotice(slot,"Reply waiting for a free radio.");Serial.println("Bot trigger matched; one bounded reply scheduled");
}
void tickBot(){
  if(fixtureMode)return;
  for(int i=0;i<40;i++)if(ready&&botEnabled[i]&&!botChannelValid(i)){botEnabled[i]=false;botInfo="Channel changed; enable bot again.";}
  if(!botQueued)return;
  if(!ready||!linkActive||!botDrained||!botEnabled[botQueuedChannel]||!botChannelValid(botQueuedChannel)||storageBlocked||terminal_tools::due(millis(),botExpires)){
    cancelBotWeather();botQueued=false;botInfo="Pending reply cancelled; no retry.";return;
  }
  if(botWeatherWaiting())return;
  if(!terminal_tools::due(millis(),botDue)||pending!=Pending::NONE||networkAction!=NetworkAction::NONE||remoteWaiting||nearby.active||antennaBatch||antennaSampleActive||manageAction!=ManageAction::NONE)return;
  uint32_t stamp=max(nowSeconds(),lastOutgoingTimestamp+1);
  int watch=createSendWatch(stamp,false,botQueuedChannel,nullptr,channels[botQueuedChannel],botReply,true);if(watch<0)return;
  pendingSend=watch;lastOutgoingTimestamp=stamp;uint8_t packet[133]={3,0,botQueuedChannel};protocol::write32(packet+3,stamp);
  size_t length=min(size_t(120),size_t(botReply.length()));memcpy(packet+7,botReply.c_str(),length);botQueued=false;
  botInfo="Reply requested; check Net > Sent.";
  botLimits.dispatched(millis(),botQueuedChannel);
  if(!sendCommand(packet,length+7,Pending::SEND)){
    recordOutgoing(false);sends[watch].timedOut=true;updateSendResult(stamp,field::SendState::UNCONFIRMED);pendingSend=-1;
    failLink("Bot send unconfirmed; no automatic retry.");
  }else deliveryFeedback("Bot reply requested; not delivery",0x5C7F);
}
// Checkbox hit areas are separate from detail buttons on the command list.
void botCheckbox(int x,int y,int width,const String& label,bool checked){
  smallButton(x,y,width,28,label.c_str());lcd.fillRect(x+7,y+5,18,18,BG);lcd.drawRect(x+7,y+5,18,18,TFT_WHITE);
  if(checked){lcd.drawLine(x+10,y+14,x+14,y+19,TFT_GREEN);lcd.drawLine(x+14,y+19,x+22,y+8,TFT_GREEN);}
}
const char* botCommandName(int i){static const char* names[]={"Reception test","Help / commands","Info / custom reply 1","Custom reply 2","Weather","Ping -> pong","Pong -> ping","Status","Battery","Uptime","Version"};return names[i];}
String selectedBotKeyword(){auto& c=botChannels[botSelected];return botCommandSelected==0?String(c.test):botCommandSelected==2||botCommandSelected==3?String(c.words[botCommandSelected-2]):String(bot::fixedAliases(botCommandSelected));}
void stopEditedBot(){if(botQueued&&botQueuedChannel==botSelected){cancelBotWeather();botQueued=false;}botInfo="Saved. Channel bot remains "+String(botEnabled[botSelected]?"on.":"off.");}
bool toggleBotCommand(int kind){auto& c=botChannels[botSelected];uint16_t flag=1u<<kind;
  if(!(c.flags&flag)){
    if((kind==2||kind==3)&&(!c.words[kind-2][0]||!c.replies[kind-2][0])){botInfo="Set trigger words and a reply first.";return false;}
    const char* aliases=kind==0?c.test:kind==2||kind==3?c.words[kind-2]:bot::fixedAliases(kind);
    for(int i=0;i<=10;i++)if(i!=kind&&(c.flags&(1u<<i))){const char* other=i==0?c.test:i==2||i==3?c.words[i-2]:bot::fixedAliases(i);if(bot::overlap(aliases,other)){botInfo="Trigger already used by another command.";return false;}}
  }
  c.flags^=flag;stopEditedBot();return saveBotSettings();
}
void drawBotChannels(){
  screen=Screen::BOT_CHANNELS;header("Bots / per channel");auto list=activeChannels();botPage=constrain(botPage,0,max(0,int((list.size()+3)/4)-1));
  for(int row=0;row<4;row++){int item=botPage*4+row;if(item>=int(list.size()))break;int ch=list[item];String label="     "+channels[ch];botCheckbox(12,42+row*32,296,label.substring(0,32),botEnabled[ch]&&botChannelValid(ch));}
  smallButton(12,177,68,28,"Prev");smallButton(84,177,68,28,"Next");smallButton(228,177,80,28,"Back");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(botInfo.length()?botInfo.substring(0,49):"Tick channel on/off. Tap name for settings.",12,215,1);
}
void drawBotSetup(){
  screen=Screen::BOT_SETUP;header(("Bot: "+channels[botSelected]).substring(0,22).c_str());smallButton(220,4,88,26,"Commands");
  smallButton(12,42,296,30,("Location: "+String(botChannels[botSelected].place)).c_str());
  botCheckbox(12,80,296,"     Enable this channel bot",botEnabled[botSelected]);
  smallButton(12,120,296,30,("Reply interval: "+String(botChannels[botSelected].interval?String(botChannels[botSelected].interval)+" seconds":"Off / fast testing")).c_str());
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Choose commands using their checkboxes.",12,157,1);
  smallButton(12,177,100,28,"Back");lcd.setTextColor(botSaveFailed?TFT_ORANGE:TFT_WHITE,BG);lcd.drawString(botInfo.length()?botInfo.substring(0,49):"Editing commands keeps this switch unchanged.",12,215,1);
}
void drawBotCommands(){
  screen=Screen::BOT_COMMANDS;header(("Commands "+String(botCommandPage+1)+"/3").c_str());auto& c=botChannels[botSelected];
  for(int row=0;row<4;row++){int i=botCommandPage*4+row;if(i>10)break;int y=42+row*32;botCheckbox(12,y,40,"",c.flags&(1u<<i));smallButton(58,y,250,28,botCommandName(i));}
  smallButton(12,177,68,28,"Prev");smallButton(84,177,68,28,"Next");smallButton(228,177,80,28,"Back");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(botInfo.length()?botInfo.substring(0,49):"Tick to enable. Tap name for triggers / details.",12,215,1);
}
void drawBotCommandEdit(){
  screen=Screen::BOT_COMMAND_EDIT;header(botCommandName(botCommandSelected));auto& c=botChannels[botSelected];
  botCheckbox(12,42,296,"     Enable this command",c.flags&(1u<<botCommandSelected));
  smallButton(12,78,296,28,("Triggers: "+selectedBotKeyword()).c_str());
  if(botCommandSelected==2||botCommandSelected==3){smallButton(12,114,296,28,"Edit reply");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(String(c.replies[botCommandSelected-2]).substring(0,49),12,150,1);}
  else{lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(botCommandSelected==0?"Replies with this message's reception details.":botCommandSelected==1?"Lists enabled commands for this channel.":botCommandSelected==7?"Local status; does not confirm repeater reach.":botCommandSelected==8?"Last Heltec battery reading, if available.":botCommandSelected==9?"Time since the CYD display last restarted.":botCommandSelected==10?"Installed Mesh Pilot firmware version.":botCommandSelected==5?"Replies: [bot] pong":"Replies: [bot] ping",12,125,1);}
  smallButton(12,177,100,28,"Back");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(botInfo.length()?botInfo.substring(0,49):"Any case. Optional ! prefix. Comma = aliases.",12,215,1);
}
void saveBotText(const String& text,bool reply){
  auto& c=botChannels[botSelected];String value=text;value.trim();if(botCommandSelected!=0&&botCommandSelected!=2&&botCommandSelected!=3)return;
  if(!reply){value.toLowerCase();value.replace(", ",",");value.replace(" ,",",");if(!bot::aliases(value.c_str())){botInfo="Use comma-separated words, up to 16 chars.";return;}
    for(int i=0;i<=10;i++)if(i!=botCommandSelected){const char* other=i==0?c.test:i==2||i==3?c.words[i-2]:bot::fixedAliases(i);if(bot::overlap(value.c_str(),other)){botInfo="An alias is already used by another command.";return;}}
  }else if(!value.length()){botInfo="Reply cannot be blank.";return;}
  char* dest=reply?c.replies[botCommandSelected-2]:botCommandSelected==0?c.test:c.words[botCommandSelected-2];value.toCharArray(dest,reply?101:17);stopEditedBot();saveBotSettings();
}
bool botCommandTouch(int x,int y){
  auto& c=botChannels[botSelected];bool custom=botCommandSelected==2||botCommandSelected==3;
  if(x>=12&&x<=308&&y>=42&&y<=70){toggleBotCommand(botCommandSelected);drawBotCommandEdit();}
  else if(x>=12&&x<=308&&((y>=78&&y<=106&&(botCommandSelected==0||custom))||(y>=114&&y<=142&&custom))){bool reply=y>=114;textPurpose=reply?11:10;editorReturn=Screen::BOT_COMMAND_EDIT;editText=reply?String(c.replies[botCommandSelected-2]):selectedBotKeyword();textLimit=reply?100:16;upperCase=symbols=false;drawTextEditor();}
  else if(x>=12&&x<=112&&y>=177&&y<=205)drawBotCommands();return true;
}
void toggleChannelBot(){
  if(botEnabled[botSelected]){if(botQueued&&botQueuedChannel==botSelected){cancelBotWeather();botQueued=false;}botEnabled[botSelected]=false;botInfo="Off. Already-sent replies cannot be recalled.";}
  else if(ready&&haveChannelKey[botSelected]&&!storageBlocked){memcpy(botChannels[botSelected].key,channelKeys[botSelected],16);if(saveBotSettings()){botEnabled[botSelected]=true;resetBotLink();lastPoll=millis()-3000;botInfo="On after old messages finish loading.";}}
  else botInfo="Connect the radio and check storage first.";
}
bool botTouch(int x,int y){
  if(screen==Screen::BOT_CHANNELS){auto list=activeChannels();if(x>=12&&x<=308&&y>=42&&y<170&&(y-42)%32<28){int item=botPage*4+(y-42)/32;if(item<int(list.size())){botSelected=list[item];botInfo="";if(x<=52){toggleChannelBot();drawBotChannels();}else drawBotSetup();}}
    else if(y>=177&&y<=205){if(x>=12&&x<=80){botPage--;drawBotChannels();}else if(x>=84&&x<=152){botPage++;drawBotChannels();}else if(x>=228&&x<=308)drawSettings();}return true;}
  if(screen==Screen::BOT_COMMANDS){if(x>=12&&x<=308&&y>=42&&y<170&&(y-42)%32<28){int kind=botCommandPage*4+(y-42)/32;if(kind<=10){botCommandSelected=kind;if(x<=52){toggleBotCommand(kind);drawBotCommands();}else if(x>=58){if(kind==4)drawWeatherSettings();else drawBotCommandEdit();}}}
    else if(y>=177&&y<=205){if(x>=12&&x<=80){botCommandPage=max(0,botCommandPage-1);drawBotCommands();}else if(x>=84&&x<=152){botCommandPage=min(2,botCommandPage+1);drawBotCommands();}else if(x>=228&&x<=308)drawBotSetup();}return true;}
  if(screen==Screen::BOT_COMMAND_EDIT)return botCommandTouch(x,y);if(screen!=Screen::BOT_SETUP)return false;
  if(x>=220&&x<=308&&y>=4&&y<=30){drawBotCommands();return true;}
  if(x>=12&&x<=308&&y>=42&&y<=72){textPurpose=9;editorReturn=Screen::BOT_SETUP;editText=botChannels[botSelected].place;textLimit=24;upperCase=symbols=false;drawTextEditor();}
  else if(x>=12&&x<=308&&y>=80&&y<=112){toggleChannelBot();drawBotSetup();
  }else if(x>=12&&x<=308&&y>=120&&y<=150){const uint8_t options[]={0,5,10,15,30,60};auto& interval=botChannels[botSelected].interval;for(size_t i=0;i<6;i++)if(interval==options[i]){interval=options[(i+1)%6];break;}if(saveBotSettings())botInfo="Interval saved; applies to new requests.";drawBotSetup();}
  else if(x>=12&&x<=112&&y>=177&&y<=205)drawBotChannels();return true;
}
int activeBotCount(){int count=0;for(int i=0;i<40;i++)if(botEnabled[i]&&botChannelValid(i))count++;return count;}
void botStatus(){int count=activeBotCount();Serial.printf("Bot status: enabledChannels=%d; drained=%d; queued=%d; settingsFailed=%d; attempts=%u\n",count,botDrained,botQueued,botSaveFailed,botLimits.count);}
// Hash fields explicitly: struct alignment bytes are not saved settings.
uint32_t botSettingsHash(){uint32_t hash=2166136261u;for(const auto& c:botChannels){
  hash=bot::hash(c.key,16,hash);hash=bot::hash(reinterpret_cast<const uint8_t*>(c.place),25,hash);
  uint8_t flags[2]={uint8_t(c.flags),uint8_t(c.flags>>8)};hash=bot::hash(flags,2,hash);
  hash=bot::hash(reinterpret_cast<const uint8_t*>(c.test),17,hash);hash=bot::hash(reinterpret_cast<const uint8_t*>(c.words),34,hash);
  hash=bot::hash(reinterpret_cast<const uint8_t*>(c.replies),202,hash);hash=bot::hash(&c.interval,1,hash);
}return hash;}
bool botStorageSelfTest(){
  if(!storageReady||weatherBusyForDiagnostics())return false;
  // Reserve the only decode buffer before changing RAM, so restoration cannot
  // fail just because a concurrent Wi-Fi task consumes a large heap block.
  std::unique_ptr<BotChannel[]> scratch(new(std::nothrow) BotChannel[40]);if(!scratch)return false;
  String savedJournal=journalPath;journalPath="/fixture-bot-test.log";const String path="/fixture-bot-test.bin",backup="/fixture-bot-backup.bin";
  if(!writeBotSettings(backup)){journalPath=savedJournal;return false;}
  const uint8_t intervals[]={0,5,10,15,30,60};
  for(int i=0;i<40;i++){auto& c=botChannels[i];c=BotChannel{};strcpy(c.words[0],"meeting");strcpy(c.replies[0],"Meet at the agreed place");c.flags=2039;c.interval=intervals[i%6];memset(c.key,i+1,16);snprintf(c.place,25,"Fixture %d",i);}
  uint32_t expected=botSettingsHash();
  bool ok=writeBotSettings(path);for(auto& c:botChannels)c=BotChannel{};
  ok=readBotSettings(path,botChannels,scratch.get())&&ok;ok=ok&&botSettingsHash()==expected;
  File f=LittleFS.open(path,"r+");if(f){f.seek(12);f.write(uint8_t(0xff));f.close();}else ok=false;
  ok=ok&&!readBotSettings(path,botChannels,scratch.get())&&botSettingsHash()==expected;
  FieldWriter v2(path,"MPB2",40);for(const auto& c:botChannels){uint8_t flags=7;v2.put(c.key,16);v2.put(reinterpret_cast<const uint8_t*>(c.place),25);v2.put(&flags,1);const char oldTest[17]="test";v2.put(reinterpret_cast<const uint8_t*>(oldTest),17);v2.put(reinterpret_cast<const uint8_t*>(c.words),34);v2.put(reinterpret_cast<const uint8_t*>(c.replies),202);}
  ok=v2.finish()&&ok;ok=readBotSettings(path,botChannels,scratch.get())&&ok;for(const auto& c:botChannels)ok=ok&&c.flags==7&&c.interval==30&&!strcmp(c.words[0],"meeting");
  FieldWriter v3(path,"MPB3",40);for(auto& c:botChannels){c.flags=23;v3.put(c.key,16);v3.put(reinterpret_cast<const uint8_t*>(c.place),25);uint8_t flags=23;v3.put(&flags,1);const char oldTest[17]="test";v3.put(reinterpret_cast<const uint8_t*>(oldTest),17);v3.put(reinterpret_cast<const uint8_t*>(c.words),34);v3.put(reinterpret_cast<const uint8_t*>(c.replies),202);}
  ok=v3.finish()&&ok;ok=readBotSettings(path,botChannels,scratch.get())&&ok;for(const auto& c:botChannels)ok=ok&&c.flags==23&&c.interval==30&&!strcmp(c.words[0],"meeting");
  FieldWriter v4(path,"MPB4",40);int index=0;for(const auto& c:botChannels){uint8_t flags=23,interval=intervals[index++%6];const char oldTest[17]="test";v4.put(c.key,16);v4.put(reinterpret_cast<const uint8_t*>(c.place),25);v4.put(&flags,1);v4.put(reinterpret_cast<const uint8_t*>(oldTest),17);v4.put(reinterpret_cast<const uint8_t*>(c.words),34);v4.put(reinterpret_cast<const uint8_t*>(c.replies),202);v4.put(&interval,1);}
  ok=v4.finish()&&ok;ok=readBotSettings(path,botChannels,scratch.get())&&ok;index=0;for(const auto& c:botChannels)ok=ok&&c.flags==23&&c.interval==intervals[index++%6]&&!strcmp(c.test,"test,testing")&&!strcmp(c.words[0],"meeting");
  FieldWriter legacy(path,"MPB1",40);for(const auto& c:botChannels){legacy.put(c.key,16);legacy.put(reinterpret_cast<const uint8_t*>(c.place),25);}
  ok=legacy.finish()&&ok;ok=readBotSettings(path,botChannels,scratch.get())&&ok;
  for(const auto& c:botChannels)ok=ok&&c.flags==3&&c.interval==30&&!strcmp(c.test,"test,testing")&&!strcmp(c.words[0],"info,about");
  bool restored=readBotSettings(backup,botChannels,scratch.get());
  if(restored)LittleFS.remove(backup);else{memset(botEnabled,0,sizeof(botEnabled));for(auto& c:botChannels)c=BotChannel{};botSaveFailed=true;botInfo="Restore failed; restart to reload settings.";}
  LittleFS.remove(path);if(LittleFS.exists(path+".tmp"))LittleFS.remove(path+".tmp");journalPath=savedJournal;return ok&&restored;
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
  botLimits.dispatched(millis(),0);r.data[7]=2;botIncoming(r);ok=ok&&!botQueued&&botInfo.startsWith("Reply interval:");
  botChannels[0].interval=0;botIncoming(r);ok=ok&&botQueued;
  resetBotLink();botQueueDrained();r.data[7]=3;botIncoming(r);ok=ok&&botQueued;
  resetBotLink();botQueueDrained();botChannels[0].interval=30;
  r.data[7]=2;botEnabled[0]=false;botIncoming(r);ok=ok&&!botQueued;
  botEnabled[0]=true;botLimits=bot::Limits{};botChannels[0].flags=7;strcpy(botChannels[0].words[0],"meeting");strcpy(botChannels[0].replies[0],"Use the agreed meeting place");
  memcpy(r.data+14,"HELP",4);botIncoming(r);ok=ok&&botQueued&&botReply=="[bot] Commands: test help meeting";
  resetBotLink();botQueueDrained();botLimits=bot::Limits{};
  const char custom[]="A: MeEtInG";r.length=11+strlen(custom);memcpy(r.data+11,custom,strlen(custom));botIncoming(r);
  ok=ok&&botQueued&&botReply=="[bot] Use the agreed meeting place";botChannels[0]=BotChannel{};
  for(int kind=5;kind<=10;kind++){
    resetBotLink();botQueueDrained();botLimits=bot::Limits{};botChannels[0].flags=1u<<kind;
    String aliases=bot::fixedAliases(kind);int comma=aliases.indexOf(',');String body="A: !"+(comma<0?aliases:aliases.substring(0,comma));r.length=11+body.length();memcpy(r.data+11,body.c_str(),body.length());
    botIncoming(r);ok=ok&&botQueued&&botReply.startsWith("[bot] ")&&botReply.length()<=120;
    if(kind==5)ok=ok&&botReply=="[bot] pong";if(kind==6)ok=ok&&botReply=="[bot] ping";
    resetBotLink();botQueueDrained();botLimits=bot::Limits{};botChannels[0].flags=0;botIncoming(r);ok=ok&&!botQueued;
  }
  botSelected=0;botEnabled[0]=true;botQueued=true;botQueuedChannel=0;stopEditedBot();ok=ok&&botEnabled[0]&&!botQueued;
  botChannels[0]=BotChannel{};
  fixtureMode=true;draft="Unsent family message";currentChannel=1;composingDirect=true;
  int slot=createSendWatch(42,false,0,nullptr,"Fixture","[bot] received",true);
  ok=ok&&slot>=0&&sends[slot].automatic&&!sends[slot].direct&&sends[slot].channel==0&&sends[slot].text=="[bot] received"&&draft=="Unsent family message"&&composingDirect&&currentChannel==1;
  for(int i=0;i<8;i++)ok=createSendWatch(50+i,false,0,nullptr,"Fixture","[bot] rapid",true)>=0&&ok;
  for(auto& watch:sends)watch=SendWatch{};
  for(int i=0;i<4;i++)ok=createSendWatch(70+i,false,0,nullptr,"Fixture","Manual",false)>=0&&ok;
  ok=ok&&createSendWatch(80,false,0,nullptr,"Fixture","[bot] rapid",true)<0;
  for(auto& watch:sends)watch=SendWatch{};latestSend=-1;ready=linkActive=false;maxChannels=0;
  channels[0]=channels[1]="";haveChannelKey[0]=haveChannelKey[1]=false;wireName="";currentChannel=0;composingDirect=false;
  memset(botEnabled,0,sizeof(botEnabled));botLimits=bot::Limits{};resetBotLink();botInfo="";
  int originalPurpose=textPurpose;String originalEdit=editText;textPurpose=9;editText="Ramsbottom";drawTextEditor();ok=ok&&screen==Screen::TEXT_EDIT;
  textPurpose=originalPurpose;editText=originalEdit;draft=originalDraft;screen=originalScreen;fixtureMode=originalFixture;return ok;
}
