#pragma once
const char* antennaPhaseName(int phase){return phase==0?"A first":phase==1?"B":"A again";}
const char* antennaSampleName(const antenna::Sample& r){
  switch(r.state){case antenna::State::QUEUED:return "Preparing";case antenna::State::LISTENING:return "Listening";case antenna::State::COMPLETE:return r.accepted?(r.heard?"Reply received":"No reply / unconfirmed"):"Excluded: acceptance unconfirmed";case antenna::State::REJECTED:return "Excluded: radio rejected request";case antenna::State::WRITE_FAILED:return "Excluded: request write unconfirmed";case antenna::State::CANCELLED:return "Excluded: stopped by user";default:return "Excluded: interrupted";}
}
void refreshAntenna(){if(screen==Screen::ANTENNA_SESSION)drawAntennaSession();else if(screen==Screen::ANTENNA_RESULTS)drawAntennaResults();}
void drawAntennaList(){
  screen=Screen::ANTENNA_LIST;header("Antenna A/B");smallButton(180,4,60,26,"Help");smallButton(244,4,64,26,"New");
  antennaPage=constrain(antennaPage,0,max(0,int((antennaSessions.size()+3)/4)-1));
  for(int row=0;row<4;row++){int i=int(antennaSessions.size())-1-(antennaPage*4+row);if(i<0)break;const auto& s=antennaSessions[i];int y=42+row*32;
    lcd.fillRoundRect(12,y,296,28,4,PANEL);lcd.setTextColor(ACCENT,PANEL);lcd.drawString(String(s.place)+" / "+fieldUtc(s.epoch).substring(0,6),19,y+4,1);
    lcd.setTextColor(TFT_WHITE,PANEL);lcd.drawString((String(s.nameA)+" vs "+s.nameB).substring(0,47),19,y+16,1);
  }
  if(antennaSessions.empty()){lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Compare A, then B, then A again.",12,60,1);lcd.drawString("Uses one fixed direct repeater throughout.",12,81,1);lcd.drawString("This measures a link, not antenna SWR.",12,102,1);}
  smallButton(12,177,68,28,"Prev");smallButton(84,177,68,28,"Next");smallButton(216,177,92,28,"Back");lcd.setTextColor(antennaFailed?TFT_ORANGE:TFT_WHITE,BG);
  lcd.drawString(antennaFailed?"Antenna records unavailable; check storage.":"Keeps the latest 4 comparisons for this Heltec.",12,215,1);
}
void drawAntennaSetup(){
  screen=Screen::ANTENNA_SETUP;header("Set up comparison");
  smallButton(12,42,296,28,antennaDraft.place[0]?("Place: "+String(antennaDraft.place)).c_str():"Name this place");
  smallButton(12,74,296,28,("A: "+String(antennaDraft.nameA)).c_str());smallButton(12,106,296,28,("B: "+String(antennaDraft.nameB)).c_str());
  smallButton(12,138,296,28,antennaDraft.key[0]||antennaDraft.targetName[0]?("To: "+String(antennaDraft.targetName)).substring(0,32).c_str():"Choose a target repeater");
  smallButton(12,177,190,28,"Create comparison");smallButton(216,177,92,28,"Back");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(antennaInfo.substring(0,49),12,215,1);
}
std::vector<int> antennaTargets(){
  std::vector<int> result;for(size_t i=0;i<contacts.size();i++)if(contacts[i].type==2)result.push_back(i);
  for(int i=0;i<nearby.count;i++){bool known=false;for(int j:result)if(j>=0&&!memcmp(contacts[j].key,nearby.results[i].key,32)){known=true;break;}if(!known)result.push_back(-i-1);}
  return result;
}
const uint8_t* antennaTargetKey(int index){return index>=0?contacts[index].key:nearby.results[-index-1].key;}
void drawAntennaPick(){
  screen=Screen::ANTENNA_PICK;header("Target repeater");auto list=antennaTargets();antennaPickPage=constrain(antennaPickPage,0,max(0,int((list.size()+3)/4)-1));
  for(int row=0;row<4;row++){int i=antennaPickPage*4+row;if(i>=int(list.size()))break;smallButton(12,42+row*32,296,28,fieldKeyName(antennaTargetKey(list[i])).substring(0,31).c_str());}
  if(list.empty()){lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("No repeater keys are available yet.",12,61,1);lcd.drawString("Use Discover or add a repeater contact first.",12,82,1);}
  smallButton(12,177,68,28,"Prev");smallButton(84,177,68,28,"Next");smallButton(216,177,92,28,"Back");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Saved nodes may be out of direct radio range.",12,215,1);
}
void drawAntennaRun(){
  auto* s=selectedAntenna(antennaSelectedId);if(!s){drawAntennaList();return;}screen=Screen::ANTENNA_RUN;header("Fit the antenna first");lcd.setTextColor(TFT_WHITE,BG);
  lcd.drawString((String(antennaPhaseName(s->phase))+": "+(s->phase==1?s->nameB:s->nameA)).substring(0,34),12,44,2);
  lcd.drawString("Power the Heltec OFF before changing antennas.",12,75,1);lcd.drawString("Reconnect it, then tap Run below.",12,94,1);
  lcd.drawString("Keep location, height and orientation the same.",12,113,1);lcd.drawString("Three requests, at least one minute apart.",12,132,1);lcd.drawString("A full stage takes about 2 minutes 30 seconds.",12,151,1);
  smallButton(12,177,190,28,"Run this stage");smallButton(216,177,92,28,"Back");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(antennaInfo.substring(0,49),12,215,1);
}
String antennaProgress(const antenna::Session& s){
  if(antennaSampleActive&&s.id==antennaRunningId)return "Listening: "+String(nearby.count)+" nodes heard; target only counted";
  if(antennaBatch&&s.id==antennaRunningId){uint32_t elapsed=millis()-lastDiscoveryAt;return "Next request in "+String(haveDiscoveryAttempt&&elapsed<discovery::COOLDOWN_MS?(discovery::COOLDOWN_MS-elapsed+999)/1000:0)+"s";}
  return antennaInfo.length()?antennaInfo:String("Paused; each stage needs a tap to start.");
}
void paintAntennaProgress(){
  auto* s=selectedAntenna(antennaSelectedId);if(!s)return;lcd.setTextColor(TFT_WHITE,BG);lcd.setTextPadding(296);lcd.drawString(antennaProgress(*s).substring(0,49),12,103,1);lcd.setTextPadding(0);
}
void drawAntennaSession(){
  auto* s=selectedAntenna(antennaSelectedId);if(!s){drawAntennaList();return;}screen=Screen::ANTENNA_SESSION;header(String(s->place).substring(0,32).c_str());
  auto summary=antenna::summary(*s,s->phase);lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(("To: "+String(s->targetName)).substring(0,49),12,43,1);
  lcd.drawString((String(antennaPhaseName(s->phase))+": "+(s->phase==1?s->nameB:s->nameA)).substring(0,49),12,63,1);
  lcd.drawString("Complete "+String(summary.attempts)+"/3 | Replies "+String(summary.replies)+"/"+String(summary.attempts)+" | Excluded "+String(summary.excluded),12,83,1);paintAntennaProgress();
  lcd.drawString("Keep the same antenna fitted during this stage.",12,123,1);lcd.drawString(String(s->profile).substring(0,49),12,143,1);
  lcd.drawString("Reported TX power: "+String(s->power)+" dBm",12,159,1);
  const char* primary=antennaBatch||antennaSampleActive?"Stop":summary.attempts>=3?(s->phase<2?"Next stage":"Review"):"Run stage";
  smallButton(12,177,92,28,primary);smallButton(110,177,92,28,"Results");smallButton(208,177,100,28,"Back");lcd.setTextColor(antennaFailed?TFT_ORANGE:TFT_WHITE,BG);
  lcd.drawString(antennaFailed?"Records not saved; automatic testing stopped.":"Back stops testing. Swaps require radio power OFF.",12,215,1);
}
const char* antennaVerdictName(antenna::Verdict verdict){
  switch(verdict){case antenna::Verdict::INCOMPLETE:return "Inconclusive: stages unfinished";case antenna::Verdict::TOO_FEW:return "Inconclusive: too few replies";case antenna::Verdict::CONDITIONS_CHANGED:return "Inconclusive: A readings changed";case antenna::Verdict::A_STRONGER:return "A had stronger SNR readings";case antenna::Verdict::B_STRONGER:return "B had stronger SNR readings";case antenna::Verdict::MIXED:return "Mixed readings; no clear advantage";default:return "No clear signal advantage";}
}
void drawAntennaResults(){
  auto* s=selectedAntenna(antennaSelectedId);if(!s){drawAntennaList();return;}screen=Screen::ANTENNA_RESULTS;header("A / B / A comparison");lcd.setTextColor(TFT_WHITE,BG);
  lcd.drawString("Phase      Replies  Out SNR   In SNR   RSSI",12,44,1);
  for(int phase=0;phase<3;phase++){auto summary=antenna::summary(*s,phase);int y=66+phase*21;lcd.drawString(antennaPhaseName(phase),12,y,1);
    lcd.drawString(String(summary.replies)+"/"+String(summary.attempts),84,y,1);lcd.drawString(summary.replies?String(summary.outbound,1):"--",142,y,1);lcd.drawString(summary.replies?String(summary.inbound,1):"--",207,y,1);lcd.drawString(summary.replies?String(summary.rssi,1):"--",264,y,1);
  }
  lcd.drawString(String(antennaVerdictName(antenna::verdict(*s))).substring(0,49),12,134,1);lcd.drawString("Median SNR: dB; median reply signal: dBm.",12,151,1);
  smallButton(12,177,100,28,"Samples");smallButton(216,177,92,28,"Back");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Target key "+hexText(s->key,6)+"; no SWR or gain measurement.",12,215,1);
}
void drawAntennaSamples(){
  auto* s=selectedAntenna(antennaSelectedId);if(!s){drawAntennaList();return;}screen=Screen::ANTENNA_SAMPLES;header((String("Samples / ")+antennaPhaseName(antennaSamplePhase)).c_str());smallButton(244,4,64,26,"Phase");
  int count=s->count[antennaSamplePhase];antennaSamplePage=constrain(antennaSamplePage,0,max(0,(count+3)/4-1));
  for(int row=0;row<4;row++){int i=antennaSamplePage*4+row;if(i>=count)break;const auto& r=s->samples[antennaSamplePhase][i];int y=43+row*32;lcd.setTextColor(TFT_WHITE,BG);lcd.drawString((String(i+1)+". "+antennaSampleName(r)).substring(0,49),12,y,1);
    String line=fieldUtc(r.epoch);if(r.heard)line="Out "+String(r.outbound/4.0f,1)+" / In "+String(r.inbound/4.0f,1)+" dB; "+String(r.rssi)+" dBm";lcd.drawString(line.substring(0,49),12,y+13,1);
  }
  smallButton(12,177,68,28,"Prev");smallButton(84,177,68,28,"Next");smallButton(216,177,92,28,"Back");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Interrupted or rejected tests are excluded.",12,215,1);
}
void drawAntennaHelp(){
  screen=Screen::ANTENNA_HELP;header("Antenna comparison help");
  wrapped("Compare one direct repeater at one fixed place. Test A, then B, then A again. Keep radio settings, height, orientation and placement unchanged. Power the Heltec OFF before swapping antennas. Each stage runs three requests at least a minute apart. Only the selected repeater counts. No replies are inconclusive. Readings describe this small trial; they do not measure antenna gain, tuning or SWR.",12,45,11);
  smallButton(216,177,92,28,"Back");
}
bool createAntennaSession(){
  String place=antennaDraft.place,a=antennaDraft.nameA,b=antennaDraft.nameB;place.trim();a.trim();b.trim();String aa=a,bb=b;aa.toLowerCase();bb.toLowerCase();
  uint8_t key=0;for(auto v:antennaDraft.key)key|=v;
  if(!ready||!linkActive||!radioProfile.length()){antennaInfo="Connect the Heltec first.";return false;}
  if(antennaBatch||antennaSampleActive||coverageCapture){antennaInfo="Stop the active test before creating another.";return false;}
  if(!place.length()||!a.length()||!b.length()||aa==bb||!key){antennaInfo="Set place, two different names and a repeater.";return false;}
  antenna::Session s=antennaDraft;s.id=antennaSessions.empty()?1:antennaSessions.back().id+1;if(!s.id){antennaInfo="Comparison identifier limit reached.";return false;}
  s.epoch=nowSeconds();s.approximate=!clockTrusted;s.power=radioTxPower;memset(s.profile,0,sizeof(s.profile));memcpy(s.profile,radioProfile.c_str(),min(size_t(64),size_t(radioProfile.length())));
  memset(s.place,0,sizeof(s.place));memcpy(s.place,place.c_str(),place.length());memset(s.nameA,0,sizeof(s.nameA));memcpy(s.nameA,a.c_str(),a.length());memset(s.nameB,0,sizeof(s.nameB));memcpy(s.nameB,b.c_str(),b.length());
  auto previous=antennaSessions;if(antennaSessions.size()>=antenna::SESSIONS)antennaSessions.erase(antennaSessions.begin());antennaSessions.push_back(s);saveAntenna();
  if(antennaFailed){antennaSessions=std::move(previous);antennaDirty=false;antennaInfo="Cannot save comparison; check screen storage.";return false;}
  antennaSelectedId=s.id;antennaInfo="";return true;
}
void antennaDispatched(){
  if(fixtureMode||!antennaSampleActive)return;auto* s=selectedAntenna(antennaRunningId);if(!s||antennaSample<0)return;auto& r=s->samples[s->phase][antennaSample];r.epoch=nowSeconds();r.state=antenna::State::LISTENING;antennaDirty=true;refreshAntenna();
}
void antennaAccepted(){if(fixtureMode||!antennaSampleActive)return;auto* s=selectedAntenna(antennaRunningId);if(s&&antennaSample>=0){s->samples[s->phase][antennaSample].accepted=true;antennaDirty=true;}}
void antennaReply(const discovery::Result& r){if(fixtureMode||!antennaSampleActive)return;auto* s=selectedAntenna(antennaRunningId);if(s&&antenna::capture(*s,antennaSample,r.key,r.outboundSnrQ,r.inboundSnrQ,r.rssi)){antennaDirty=true;refreshAntenna();}}
void antennaFinished(field::TestState state){
  if(fixtureMode)return;
  if(!antennaSampleActive){if(state==field::TestState::INTERRUPTED&&antennaBatch){antennaBatch=false;antennaInfo="Radio link lost; tap Run to resume later.";}return;}
  auto* s=selectedAntenna(antennaRunningId);if(s&&antennaSample>=0){auto& r=s->samples[s->phase][antennaSample];r.state=state==field::TestState::COMPLETE&&r.accepted?antenna::State::COMPLETE:state==field::TestState::REJECTED?antenna::State::REJECTED:state==field::TestState::WRITE_FAILED?antenna::State::WRITE_FAILED:antenna::State::INTERRUPTED;
    if(r.state!=antenna::State::COMPLETE){antennaBatch=false;antennaInfo="Test incomplete; excluded. Tap Run to retry.";}
    if(antenna::summary(*s,s->phase).attempts>=antenna::SAMPLES){antennaBatch=false;antennaInfo="Stage complete; review or prepare next antenna.";}saveAntenna();
  }
  antennaSampleActive=false;antennaSample=-1;refreshAntenna();
}
void antennaStop(bool cancelled){
  antennaBatch=false;if(antennaSampleActive){auto* s=selectedAntenna(antennaRunningId);if(s&&antennaSample>=0){auto& r=s->samples[s->phase][antennaSample];
      if(r.state==antenna::State::QUEUED&&networkAction==NetworkAction::DISCOVER)networkAction=NetworkAction::NONE;
      r.state=cancelled?antenna::State::CANCELLED:antenna::State::INTERRUPTED;saveAntenna();}}
  antennaSampleActive=false;antennaSample=-1;antennaInfo="Paused; unfinished samples are excluded.";
}
void startAntennaBatch(){
  auto* s=selectedAntenna(antennaSelectedId);if(!s)return;
  if(!ready||!linkActive){antennaInfo="Reconnect the Heltec, then tap Run.";return;}
  if(firmwareCode<8){antennaInfo="Discovery needs companion protocol v8+.";return;}
  if(networkBusy()||coverageCapture){antennaInfo="A network check is running; please wait.";return;}
  if(radioProfile!=s->profile||radioTxPower!=s->power){antennaInfo="Radio settings changed; start a new comparison.";return;}
  if(antenna::summary(*s,s->phase).attempts>=3){antennaInfo="This stage is complete.";return;}
  if(s->count[s->phase]>=antenna::SLOTS){antennaInfo="Too many interrupted attempts; start a new test.";return;}
  if(!storageReady||antennaFailed){antennaInfo="Records unavailable; check screen storage.";return;}
  antennaRunningId=s->id;antennaBatch=true;antennaInfo="";drawAntennaSession();
}
void tickAntenna(){
  if((antennaBatch||antennaSampleActive)&&(!ready||!linkActive))antennaStop(false);
  if(antennaDirty&&millis()-antennaSaveAt>=2000)saveAntenna();
  if(antennaBatch&&!antennaSampleActive&&pending==Pending::NONE&&networkAction==NetworkAction::NONE&&!nearby.active&&!remoteWaiting){
    auto* s=selectedAntenna(antennaRunningId);
    if(!s||s->count[s->phase]>=antenna::SLOTS){antennaBatch=false;antennaInfo="Attempt limit reached; start a new comparison.";refreshAntenna();}
    else if(!haveDiscoveryAttempt||millis()-lastDiscoveryAt>=discovery::COOLDOWN_MS){
      if(queueNearbyScan(false,true)){
        antennaSample=s->count[s->phase]++;auto& r=s->samples[s->phase][antennaSample];r={};r.epoch=nowSeconds();saveAntenna();
        if(antennaFailed){networkAction=NetworkAction::NONE;r.state=antenna::State::WRITE_FAILED;antennaInfo="Cannot save sample; no request transmitted.";antennaBatch=false;antennaSample=-1;}
        else antennaSampleActive=true;refreshAntenna();
      }else{antennaBatch=false;antennaInfo=discoveryInfo;refreshAntenna();}
    }
  }
  if(millis()-antennaPaintAt>=1000){antennaPaintAt=millis();if(screen==Screen::ANTENNA_SESSION)paintAntennaProgress();}
}
bool antennaTouch(int x,int y){
  if(screen==Screen::ANTENNA_LIST){
    if(y>=4&&y<=30){if(x>=180&&x<=240)drawAntennaHelp();else if(x>=244&&x<=308){antennaDraft={};strcpy(antennaDraft.nameA,"A");strcpy(antennaDraft.nameB,"B");memcpy(antennaDraft.place,coveragePlace.c_str(),min(size_t(24),size_t(coveragePlace.length())));antennaInfo="";drawAntennaSetup();}}
    else if(y>=42&&y<170&&x>=12&&x<=308&&(y-42)%32<28){int i=int(antennaSessions.size())-1-(antennaPage*4+(y-42)/32);if(i>=0){antennaSelectedId=antennaSessions[i].id;antennaInfo="";drawAntennaSession();}}
    else if(y>=177&&y<=205){if(x>=12&&x<=80){antennaPage--;drawAntennaList();}else if(x>=84&&x<=152){antennaPage++;drawAntennaList();}else if(x>=216&&x<=308)drawCoverage();}return true;
  }
  if(screen==Screen::ANTENNA_SETUP){
    if(x>=12&&x<=308&&y>=42&&y<166&&(y-42)%32<28){int row=(y-42)/32;if(row==3){antennaPickPage=0;drawAntennaPick();}else{textPurpose=6+row;editorReturn=Screen::ANTENNA_SETUP;editText=row==0?antennaDraft.place:row==1?antennaDraft.nameA:antennaDraft.nameB;textLimit=row==0?24:16;upperCase=false;symbols=false;drawTextEditor();}}
    else if(y>=177&&y<=205){if(x>=12&&x<=202){if(createAntennaSession())drawAntennaSession();else drawAntennaSetup();}else if(x>=216&&x<=308)drawAntennaList();}return true;
  }
  if(screen==Screen::ANTENNA_PICK){
    if(x>=12&&x<=308&&y>=42&&y<170&&(y-42)%32<28){auto list=antennaTargets();int i=antennaPickPage*4+(y-42)/32;if(i<int(list.size())){auto* key=antennaTargetKey(list[i]);memcpy(antennaDraft.key,key,32);memset(antennaDraft.targetName,0,33);String name=fieldKeyName(key);memcpy(antennaDraft.targetName,name.c_str(),min(size_t(32),size_t(name.length())));drawAntennaSetup();}}
    else if(y>=177&&y<=205){if(x>=12&&x<=80){antennaPickPage--;drawAntennaPick();}else if(x>=84&&x<=152){antennaPickPage++;drawAntennaPick();}else if(x>=216&&x<=308)drawAntennaSetup();}return true;
  }
  if(screen==Screen::ANTENNA_SESSION){
    if(y>=177&&y<=205){auto* s=selectedAntenna(antennaSelectedId);if(x>=12&&x<=104&&s){
        if(antennaBatch||antennaSampleActive){antennaStop(true);drawAntennaSession();}
        else if(antenna::summary(*s,s->phase).attempts>=3){if(s->phase<2){s->phase++;saveAntenna();antennaInfo="";drawAntennaRun();}else drawAntennaResults();}
        else{antennaInfo="";drawAntennaRun();}
      }else if(x>=110&&x<=202)drawAntennaResults();else if(x>=208&&x<=308){if(antennaBatch||antennaSampleActive)antennaStop(true);drawAntennaList();}}return true;
  }
  if(screen==Screen::ANTENNA_RUN){if(y>=177&&y<=205){if(x>=12&&x<=202){startAntennaBatch();if(screen==Screen::ANTENNA_RUN)drawAntennaRun();}else if(x>=216&&x<=308)drawAntennaSession();}return true;}
  if(screen==Screen::ANTENNA_RESULTS){if(y>=177&&y<=205){if(x>=12&&x<=112){antennaSamplePhase=antennaSamplePage=0;drawAntennaSamples();}else if(x>=216&&x<=308)drawAntennaSession();}return true;}
  if(screen==Screen::ANTENNA_SAMPLES){if(y>=4&&y<=30&&x>=244&&x<=308){antennaSamplePhase=(antennaSamplePhase+1)%3;antennaSamplePage=0;drawAntennaSamples();}else if(y>=177&&y<=205){if(x>=12&&x<=80){antennaSamplePage--;drawAntennaSamples();}else if(x>=84&&x<=152){antennaSamplePage++;drawAntennaSamples();}else if(x>=216&&x<=308)drawAntennaResults();}return true;}
  if(screen==Screen::ANTENNA_HELP){if(y>=177&&y<=205&&x>=216&&x<=308)drawAntennaList();return true;}
  return false;
}
