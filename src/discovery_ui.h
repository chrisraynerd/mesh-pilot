#pragma once
// All RF requests here originate from a touchscreen action. Boot fixtures only
// exercise discovery_tools; reconnect never restarts a scan or announcement.
bool discoveryScreen(){return screen==Screen::DISCOVERY||screen==Screen::NEARBY||screen==Screen::NEARBY_DETAIL||screen==Screen::ADVERT;}
String nearbyName(int index){
  auto& r=nearby.results[index];
  for(auto& c:contacts)if(!memcmp(c.key,r.key,32))return displayText(String(c.name));
  return "Repeater "+hexText(r.key,4);
}
void drawRadioTools(){
  screen=Screen::DISCOVERY;header("Discover / announce");lcd.setTextColor(TFT_WHITE,BG);
  lcd.drawString("Discover: ask nearby repeaters for a reply.",12,44,1);
  lcd.drawString("A reply confirms a direct two-way radio link.",12,59,1);
  smallButton(12,78,296,33,"Discover nearby");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Advert: announce yourself through the mesh.",12,124,1);
  lcd.drawString("Does not request replies or prove coverage.",12,139,1);
  smallButton(12,158,296,33,"Flood advert");smallButton(216,200,92,28,"Back");
}
void drawNearby(){
  screen=Screen::NEARBY;header("Nearby repeaters");smallButton(244,4,64,26,nearby.active?"Wait":"Scan");
  nearbyPage=constrain(nearbyPage,0,max(0,(int(nearby.count)+3)/4-1));
  for(int row=0;row<4;row++){int i=nearbyPage*4+row;if(i>=nearby.count)break;smallButton(12,42+row*32,296,28,nearbyName(i).substring(0,30).c_str());}
  if(!nearby.count){lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Only fresh discovery replies appear here.",12,57,1);lcd.drawString("Tap Scan to send one local request.",12,78,1);lcd.drawString("Hidden / older repeaters may not reply.",12,99,1);lcd.drawString("No reply does not prove you are out of range.",12,120,1);}
  smallButton(12,177,68,28,"Prev");smallButton(84,177,68,28,"Next");smallButton(216,177,92,28,"Back");
  lcd.setTextColor(nearby.count?TFT_GREEN:TFT_WHITE,BG);
  uint32_t elapsed=millis()-nearby.started;
  String line=nearby.active?String(nearby.count)+" replied; listening "+String((elapsed>=discovery::WINDOW_MS?0:discovery::WINDOW_MS-elapsed+999)/1000)+"s":discoveryInfo;
  lcd.drawString(line.substring(0,49),12,215,1);
  if(nearby.full){lcd.setTextColor(TFT_ORANGE,BG);lcd.drawString("First 32 replies shown",12,230,1);}
}
void drawNearbyDetail(){
  if(nearbySelected<0||nearbySelected>=nearby.count){drawNearby();return;}
  screen=Screen::NEARBY_DETAIL;auto& r=nearby.results[nearbySelected];header(nearbyName(nearbySelected).substring(0,32).c_str());
  lcd.setTextColor(TFT_GREEN,BG);lcd.drawString("Discovery reply heard: direct round trip",12,44,1);
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Reply age: "+String((millis()-r.heardAt)/1000)+"s (not a live connection)",12,63,1);
  lcd.drawString("Your request at repeater: SNR "+String(r.outboundSnrQ/4.0f,1)+" dB",12,82,1);
  lcd.drawString("Reply at Heltec: SNR "+String(r.inboundSnrQ/4.0f,1)+" dB",12,101,1);
  lcd.drawString("Reply signal: "+String(r.rssi)+" dBm",12,120,1);
  lcd.drawString("Key "+hexText(r.key,8),12,139,1);
  lcd.drawString("Does not test the repeater's onward coverage.",12,158,1);
  smallButton(12,177,190,28,"Contact details");smallButton(216,177,92,28,"Back");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Names come from your saved contacts.",12,215,1);
}
void drawAdvert(){
  screen=Screen::ADVERT;header("Flood advert");lcd.setTextColor(TFT_WHITE,BG);
  lcd.drawString("Send your Heltec's name and public identity.",12,45,1);
  lcd.drawString("Includes location if enabled on the radio.",12,64,1);
  lcd.drawString("Repeaters may relay it using your radio's",12,88,1);
  lcd.drawString("existing flood scope. No replies requested.",12,103,1);
  lcd.drawString("This helps others learn about your node.",12,127,1);
  lcd.drawString("It cannot confirm reach or message delivery.",12,146,1);
  smallButton(12,177,190,28,"Send flood advert");smallButton(216,177,92,28,"Back");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(advertInfo.substring(0,49),12,215,1);
}
void refreshDiscovery(){if(screen==Screen::NEARBY)drawNearby();else if(screen==Screen::NEARBY_DETAIL)drawNearbyDetail();else if(screen==Screen::ADVERT)drawAdvert();}
bool dispatchDiscovery(NetworkAction action){
  if(action!=NetworkAction::DISCOVER&&action!=NetworkAction::ADVERT)return false;
  bool scan=action==NetworkAction::DISCOVER;uint32_t now=millis();
  if(scan){
    uint32_t token;do{token=esp_random();}while(!token||token==nearby.tag);
    nearby.begin(token,now);nearbyPage=0;nearbySelected=-1;coverageDispatched();
    uint8_t q[11];discovery::request(q,token);lastDiscoveryAt=now;haveDiscoveryAttempt=true;
    if(!sendCommand(q,sizeof(q),Pending::DISCOVER)){nearby.active=false;discoveryInfo="Request write failed; no reply confirmed.";coverageFinished(field::TestState::WRITE_FAILED);}
  }else{
    uint8_t q[]={7,1};lastAdvertAt=now;haveAdvertAttempt=true;
    if(!sendCommand(q,sizeof(q),Pending::ADVERT))advertInfo="Advert write failed; transmission unconfirmed.";
  }
  refreshDiscovery();return true;
}
bool handleDiscoveryReply(const Reply& r){
  if(r.code==0x8e){
    if(nearby.receive(r.data,r.length,millis(),haveSelfKey?selfPublicKey:nullptr)){
      discovery::Result result{};discovery::parse(r.data,r.length,nearby.tag,result);coverageReply(result);
      discoveryInfo=String(nearby.count)+" repeater(s) replied; direct round trip.";Serial.println("Fresh nearby repeater reply matched this scan");refreshDiscovery();
    }return true;
  }
  if(pending!=Pending::DISCOVER&&pending!=Pending::ADVERT)return false;
  if(r.code!=0&&r.code!=1)return false;
  bool scan=pending==Pending::DISCOVER;pending=Pending::NONE;
  if(scan){if(r.code==1){nearby.active=false;discoveryInfo="Discovery rejected / unsupported by radio.";coverageFinished(field::TestState::REJECTED);}else {coverageAccepted();discoveryInfo="Radio accepted request; listening for replies.";}}
  else advertInfo=r.code==0?"Radio accepted advert; reach unconfirmed.":"Radio rejected advert; nothing confirmed.";
  Serial.println(scan?discoveryInfo:advertInfo);refreshDiscovery();return true;
}
void tickDiscovery(){
  if(nearby.active&&(!ready||!linkActive||nearby.expired(millis()))){
    nearby.active=false;coverageFinished((!ready||!linkActive)?field::TestState::INTERRUPTED:field::TestState::COMPLETE);
    discoveryInfo=(!ready||!linkActive)?"Radio link lost; discovery incomplete.":nearby.count?String(nearby.count)+" replied; tap a repeater for signal details.":"No replies; reach unconfirmed. Try outdoors.";
    refreshDiscovery();
  }
  if(millis()-discoveryPaintAt>=1000){
    discoveryPaintAt=millis();
    if(screen==Screen::NEARBY&&nearby.active){
      lcd.fillRect(12,215,296,10,BG);lcd.setTextColor(nearby.count?TFT_GREEN:TFT_WHITE,BG);
      lcd.drawString(String(nearby.count)+" replied; listening "+String((discovery::WINDOW_MS-(millis()-nearby.started)+999)/1000)+"s",12,215,1);
    }else if(screen==Screen::NEARBY_DETAIL&&nearbySelected>=0&&nearbySelected<nearby.count){
      lcd.fillRect(12,63,296,10,BG);lcd.setTextColor(TFT_WHITE,BG);
      lcd.drawString("Reply age: "+String((millis()-nearby.results[nearbySelected].heardAt)/1000)+"s (not a live connection)",12,63,1);
    }
  }
}
bool discoveryTouch(int x,int y){
  if(!discoveryScreen())return false;
  if(screen==Screen::DISCOVERY){if(x>=12&&x<=308&&y>=78&&y<=111)drawNearby();else if(x>=12&&x<=308&&y>=158&&y<=191)drawAdvert();else if(x>=216&&x<=308&&y>=200&&y<=228)drawNetwork();return true;}
  if(screen==Screen::NEARBY_DETAIL){
    if(y>=177&&y<=205){if(x>=216&&x<=308)drawNearby();else if(x>=12&&x<=202&&nearbySelected>=0&&nearbySelected<nearby.count){
      auto& r=nearby.results[nearbySelected];nodeSelected=-1;
      for(size_t i=0;i<contacts.size();i++)if(!memcmp(contacts[i].key,r.key,32)){nodeSelected=i;break;}
      if(nodeSelected>=0)drawNode();else{editContactNew=true;editContactName=nearbyName(nearbySelected);editContactKey=hexText(r.key,32);editContactType=2;managementInfo="Discovered key; edit name, then Save to keep.";drawContactEdit();}
    }}return true;
  }
  bool scan=screen==Screen::NEARBY;
  bool start=scan?(x>=244&&x<=308&&y>=4&&y<=30):(x>=12&&x<=202&&y>=177&&y<=205);
  if(start&&scan){queueNearbyScan(false);refreshDiscovery();return true;}
  if(start){
    String& info=scan?discoveryInfo:advertInfo;
    uint32_t last=scan?lastDiscoveryAt:lastAdvertAt;bool attempted=scan?haveDiscoveryAttempt:haveAdvertAttempt;
    if(!ready||!linkActive)info="Connect the Heltec first.";
    else if(nearby.active||networkBusy())info="A network check is running; please wait.";
    else if(attempted&&millis()-last<discovery::COOLDOWN_MS)info="Please wait "+String((discovery::COOLDOWN_MS-(millis()-last)+999)/1000)+"s before retrying.";
    else if(scan&&firmwareCode<8)info="Nearby discovery needs companion protocol v8+.";
    else{networkAction=scan?NetworkAction::DISCOVER:NetworkAction::ADVERT;info="Preparing one radio request...";}
    refreshDiscovery();return true;
  }
  if(scan&&x>=12&&x<=308&&y>=42&&y<170&&(y-42)%32<28){int i=nearbyPage*4+(y-42)/32;if(i<nearby.count){nearbySelected=i;drawNearbyDetail();}return true;}
  if(y>=177&&y<=205){if(x>=216&&x<=308)drawRadioTools();else if(scan&&x>=12&&x<=80){nearbyPage--;drawNearby();}else if(scan&&x>=84&&x<=152){nearbyPage++;drawNearby();}}
  return true;
}
bool discoveryUiSelfTest(){
  // Exercise command acknowledgement and radio-response separation without
  // writing to Bluetooth, storage, or the radio. Setup runs this on HOME.
  discovery::Scan saved=nearby;Pending oldPending=pending;
  String oldInfo=discoveryInfo,oldAdvert=advertInfo;bool ok=true;
  nearby.begin(0x12345678,millis());pending=Pending::DISCOVER;
  Reply ack{};ack.code=0;ack.length=1;
  ok&=handleDiscoveryReply(ack)&&pending==Pending::NONE&&nearby.active&&nearby.count==0;
  Reply response{};response.code=0x8e;response.length=42;
  uint8_t frame[42]={0x8e,4,uint8_t(-90),0,0x92,8,0x78,0x56,0x34,0x12};frame[10]=0xa5;memcpy(response.data,frame,42);
  ok&=handleDiscoveryReply(response)&&nearby.count==1;
  response.data[6]++;handleDiscoveryReply(response);ok&=nearby.count==1;
  pending=Pending::DISCOVER;ack.code=1;ok&=handleDiscoveryReply(ack)&&!nearby.active&&pending==Pending::NONE;
  pending=Pending::ADVERT;ack.code=0;ok&=handleDiscoveryReply(ack)&&pending==Pending::NONE&&advertInfo.indexOf("unconfirmed")>=0&&nearby.count==1;
  nearby=saved;pending=oldPending;discoveryInfo=oldInfo;advertInfo=oldAdvert;return ok;
}
