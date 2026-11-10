#pragma once
String fieldUtc(uint32_t epoch){time_t value=epoch;struct tm utc{};gmtime_r(&value,&utc);char text[28];strftime(text,sizeof(text),"%d %b %Y %H:%M:%S UTC",&utc);return text;}
String fieldAge(uint32_t at){uint32_t seconds=(millis()-at)/1000;return seconds<60?String(seconds)+"s ago":seconds<3600?String(seconds/60)+"m ago":String(seconds/3600)+"h ago";}
String fieldKeyName(const uint8_t* key,bool full=true){
  for(const auto& c:contacts)if(!memcmp(c.key,key,full?32:6))return displayText(String(c.name));
  return "Node "+hexText(key,4);
}
const char* sendStateName(field::SendState state){
  switch(state){
    case field::SendState::REQUESTING:return "Requesting radio send";
    case field::SendState::ACCEPTED:return "Radio accepted; listening";
    case field::SendState::REPEAT:return "Repeat heard";
    case field::SendState::ACK:return "Recipient radio replied";
    case field::SendState::NO_REPEAT:return "No repeat; unconfirmed";
    case field::SendState::NO_ACK:return "No ACK; unconfirmed";
    case field::SendState::REJECTED:return "Radio rejected request";
    case field::SendState::INTERRUPTED:return "Tracking interrupted";
    case field::SendState::TRACKING_UNAVAILABLE:return "Radio accepted; unconfirmed";
    default:return "Send unconfirmed";
  }
}
String testStateName(const field::Test& test){
  switch(test.state){
    case field::TestState::QUEUED:return "Preparing test";
    case field::TestState::LISTENING:return "Listening: "+String(test.replies.size())+" replies";
    case field::TestState::COMPLETE:return test.replies.empty()?"No replies / unconfirmed":String(test.replies.size())+" repeaters replied";
    case field::TestState::REJECTED:return "Radio rejected discovery";
    case field::TestState::WRITE_FAILED:return "Request write unconfirmed";
    default:return "Incomplete / interrupted";
  }
}
void noteFieldProof(uint8_t kind,const String& name){if(fixtureMode)return;haveFieldProof=true;fieldProofAt=millis();fieldProofKind=kind;fieldProofTarget=name;}
String dashboardRows[6];
void paintDashboard(bool force=false){
  String rows[6];
  rows[0]=String("Heltec: ")+(ready&&linkActive?"connected by Bluetooth":"disconnected");
  rows[1]=haveHeard?"Radio traffic: heard "+fieldAge(lastHeardAt):"Radio traffic: none heard this session";
  rows[2]=haveFieldProof?String(fieldProofKind==3?"Direct ACK: ":fieldProofKind==2?"Path reply: ":"Repeater reply: ")+fieldAge(fieldProofAt):"Two-way radio link: not confirmed this session";
  rows[3]=haveFieldProof?"Reply from: "+fieldProofTarget:"A saved node is not a live connection.";
  rows[4]=nearby.active?"Nearby search: listening for replies":remoteWaiting?"Remote check: waiting for a reply":fieldScanState?"Last search: "+String(fieldScanState==2?String(nearby.count)+" replies":fieldScanState==3?"rejected":fieldScanState==4?"write unconfirmed":"interrupted")+"; "+fieldAge(fieldScanAt):"Use Discover to test nearby repeaters.";
  rows[5]="Old replies are evidence, not a live connection.";
  for(int i=0;i<6;i++)if(force||rows[i]!=dashboardRows[i]){
    dashboardRows[i]=rows[i];lcd.setTextColor(i==2&&haveFieldProof?TFT_GREEN:i==0&&ready?ACCENT:TFT_WHITE,BG);lcd.setTextPadding(296);
    lcd.drawString(rows[i].substring(0,49),12,44+i*19,1);lcd.setTextPadding(0);
  }
}
void drawDashboard(){
  header("Connection");smallButton(164,4,60,26,"Sent");smallButton(230,4,78,26,"Coverage");paintDashboard(true);
  smallButton(12,177,68,28,"Stats");smallButton(84,177,84,28,"Discover");smallButton(174,177,60,28,"Nodes");smallButton(240,177,68,28,"Back");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("A direct ACK confirms radio receipt, not reading.",12,215,1);
}
void drawSentResults(){
  screen=Screen::SENT_RESULTS;header("Saved send results");
  sentPage=constrain(sentPage,0,max(0,int((sendResults.size()+3)/4)-1));
  for(int row=0;row<4;row++){int index=int(sendResults.size())-1-(sentPage*4+row);if(index<0)break;auto& r=sendResults[index];
    int y=42+row*32;lcd.fillRoundRect(12,y,296,28,4,PANEL);lcd.setTextColor(ACCENT,PANEL);
    lcd.drawString((fieldUtc(r.timestamp).substring(12,17)+" "+String(r.target)).substring(0,47),19,y+4,1);
    lcd.setTextColor(field::positive(r.state)?TFT_GREEN:TFT_WHITE,PANEL);lcd.drawString(sendStateName(r.state),19,y+16,1);
  }
  if(sendResults.empty()){lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("New sends will keep their results here.",12,61,1);lcd.drawString("Older messages have no recorded RF result.",12,82,1);}
  smallButton(12,177,68,28,"Prev");smallButton(84,177,68,28,"Next");smallButton(216,177,92,28,"Back");
  lcd.setTextColor(sendResultsFailed?TFT_ORANGE:TFT_WHITE,BG);lcd.drawString(sendResultsFailed?"Result storage unavailable; shown from RAM.":("Latest "+String(sendResults.size())+" send attempts; tap for details."),12,215,1);
}
void drawMessageDetail(){
  screen=Screen::MESSAGE_DETAIL;header("Message details");
  auto* result=findSendResult(inspectedTimestamp);
  if(detailHasMessage&&(!detailMessage.outgoing||(result&&(result->direct!=detailMessage.direct||(!result->direct&&result->channel!=detailMessage.channel)||(result->direct&&memcmp(result->peer,detailMessage.peer,6))))))result=nullptr;
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(fieldUtc(inspectedTimestamp)+(result&&result->approximate?" (approx)":""),12,44,1);
  String target=result?String(result->target):detailHasMessage?(detailMessage.direct?fieldKeyName(detailMessage.peer,false):"Channel slot "+String(detailMessage.channel+1)):"Saved send attempt";
  lcd.drawString(target.substring(0,49),12,62,1);
  const char* label=result?sendStateName(result->state):detailHasMessage&&!detailMessage.outgoing?"Received by your Heltec":"Older result not recorded";
  lcd.setTextColor(result&&field::positive(result->state)?TFT_GREEN:TFT_WHITE,BG);lcd.drawString(label,12,80,2);
  String explanation;
  if(!result)explanation=detailHasMessage&&!detailMessage.outgoing?"This is an incoming message.":"No saved delivery evidence for this older message.";
  else if(result->state==field::SendState::ACK)explanation="A matching recipient-radio acknowledgement was received. This does not mean the person read it.";
  else if(result->state==field::SendState::REPEAT)explanation="A relayed copy was heard. Individual recipient delivery is still unconfirmed.";
  else if(result->state==field::SendState::INTERRUPTED)explanation="Restart or Bluetooth loss ended tracking. Delivery remains unconfirmed; no automatic resend.";
  else if(result->state==field::SendState::REJECTED)explanation="The Heltec rejected this command. The draft was kept for review.";
  else if(result->state==field::SendState::ACCEPTED)explanation="Your Heltec accepted the command. Waiting for radio evidence; delivery is not confirmed yet.";
  else explanation="No matching radio confirmation is recorded. This does not prove the message failed to arrive.";
  wrapped(explanation,12,106,3);
  if(detailHasMessage)wrapped(detailMessage.text,12,143,2);
  smallButton(12,177,92,28,"Back");smallButton(110,177,110,28,"All sends");if(detailHasMessage&&!detailMessage.direct)smallButton(228,177,80,28,"Reply");
  lcd.setTextColor(sendResultsFailed?TFT_ORANGE:TFT_WHITE,BG);
  lcd.drawString(sendResultsFailed?"Result not safely saved; storage unavailable.":result?"Result recorded: "+fieldUtc(result->updated):"Hold a message still, then release for details.",12,215,1);
}
void inspectMessage(uint64_t id){
  for(const auto& m:messages){uint64_t candidate=(uint64_t(m.outgoing?0x80000000u:m.sequence)<<32)|m.timestamp;if(candidate!=id)continue;
    detailReturn=screen;detailMessage=m;detailHasMessage=true;inspectedTimestamp=m.timestamp;drawMessageDetail();break;
  }
}
void inspectSend(uint32_t timestamp){
  detailHasMessage=false;inspectedTimestamp=timestamp;detailReturn=Screen::SENT_RESULTS;
  for(const auto& m:messages)if(m.outgoing&&m.timestamp==timestamp){detailMessage=m;detailHasMessage=true;break;}drawMessageDetail();
}
field::Test* findCoverage(uint32_t id){for(auto& t:coverageTests)if(t.id==id)return &t;return nullptr;}
void drawCoverage(){
  screen=Screen::COVERAGE;header("Coverage book");smallButton(144,4,80,26,"Antennas");smallButton(230,4,78,26,"New test");
  coveragePage=constrain(coveragePage,0,max(0,int((coverageTests.size()+3)/4)-1));
  for(int row=0;row<4;row++){int i=int(coverageTests.size())-1-(coveragePage*4+row);if(i<0)break;auto& t=coverageTests[i];int y=42+row*32;
    lcd.fillRoundRect(12,y,296,28,4,PANEL);lcd.setTextColor(ACCENT,PANEL);lcd.drawString(String(t.place),19,y+4,1);
    lcd.setTextColor(!t.replies.empty()?TFT_GREEN:TFT_WHITE,PANEL);lcd.drawString(testStateName(t).substring(0,46),19,y+16,1);
  }
  if(coverageTests.empty()){lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Compare places using named discovery tests.",12,61,1);lcd.drawString("Each test sends one local repeater request.",12,82,1);lcd.drawString("No GPS or internet is required.",12,103,1);}
  smallButton(12,177,68,28,"Prev");smallButton(84,177,68,28,"Next");smallButton(216,177,92,28,"Back");
  lcd.setTextColor(coverageFailed?TFT_ORANGE:TFT_WHITE,BG);lcd.drawString(coverageFailed?"Notebook storage unavailable; check before testing.":"Keeps the latest 12 tests for this Heltec.",12,215,1);
}
void drawCoverageNew(){
  screen=Screen::COVERAGE_NEW;header("New coverage test");
  smallButton(12,43,296,31,coveragePlace.length()?("Place: "+coveragePlace).c_str():"Name this place");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Give this spot a name: house, garden, hill...",12,89,1);lcd.drawString("One nearby-repeater request; listen for 30s.",12,108,1);
  lcd.drawString("Saves replies and signal readings here.",12,127,1);lcd.drawString("No reply is inconclusive, not a failed link.",12,146,1);
  smallButton(12,177,190,28,"Start test");smallButton(216,177,92,28,"Back");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(coverageInfo.substring(0,49),12,215,1);
}
void drawCoverageTest(){
  auto* t=findCoverage(coverageSelectedId);if(!t){drawCoverage();return;}
  screen=Screen::COVERAGE_TEST;header(t->place);lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(fieldUtc(t->started)+(t->approximate?" (approx)":""),12,43,1);
  lcd.drawString(testStateName(*t).substring(0,49),12,61,1);
  coverageReplyPage=constrain(coverageReplyPage,0,max(0,int((t->replies.size()+2)/3)-1));
  for(int row=0;row<3;row++){int i=coverageReplyPage*3+row;if(i>=int(t->replies.size()))break;smallButton(12,79+row*31,296,27,fieldKeyName(t->replies[i].key).substring(0,30).c_str());}
  if(t->replies.empty()){lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Only replies to this test are recorded.",12,99,1);lcd.drawString("Saved nodes are not counted as replies.",12,118,1);lcd.drawString("Hidden / older repeaters may not answer.",12,137,1);}
  smallButton(12,177,56,28,"Prev");smallButton(72,177,56,28,"Next");smallButton(132,177,80,28,"Again");smallButton(216,177,92,28,"Back");
  lcd.setTextColor(coverageFailed?TFT_ORANGE:TFT_WHITE,BG);lcd.drawString(coverageFailed?"Latest changes not saved; storage unavailable.":t->full?"First 32 replies retained; historical readings.":"Recorded test; does not establish a current link.",12,215,1);
}
void drawCoverageSignal(){
  auto* t=findCoverage(coverageSelectedId);if(!t||coverageSignalIndex<0||coverageSignalIndex>=int(t->replies.size())){drawCoverageTest();return;}
  screen=Screen::COVERAGE_SIGNAL;const auto& r=t->replies[coverageSignalIndex];header(fieldKeyName(r.key).substring(0,30).c_str());lcd.setTextColor(TFT_WHITE,BG);
  lcd.drawString("Place: "+String(t->place),12,44,1);lcd.drawString(fieldUtc(t->started)+(t->approximate?" (approx)":""),12,63,1);
  lcd.drawString("Request at repeater: SNR "+String(r.outbound/4.0f,1)+" dB",12,86,1);
  lcd.drawString("Reply at Heltec: SNR "+String(r.inbound/4.0f,1)+" dB",12,105,1);lcd.drawString("Reply signal: "+String(r.rssi)+" dBm",12,124,1);
  lcd.drawString("Key "+hexText(r.key,8),12,146,1);smallButton(216,177,92,28,"Back");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Historical direct reply; onward coverage not tested.",12,215,1);
}
void refreshFieldDetails(){if(screen==Screen::MESSAGE_DETAIL)drawMessageDetail();else if(screen==Screen::SENT_RESULTS)drawSentResults();}
void refreshCoverage(){if(screen==Screen::COVERAGE)drawCoverage();else if(screen==Screen::COVERAGE_TEST)drawCoverageTest();}
bool queueNearbyScan(bool capture,bool antennaOwner){
  String reason;
  if(!ready||!linkActive)reason="Connect the Heltec first.";
  else if(networkBusy(antennaOwner)||coverageCapture)reason="A network check is running; please wait.";
  else if(haveDiscoveryAttempt&&millis()-lastDiscoveryAt<discovery::COOLDOWN_MS)reason="Please wait "+String((discovery::COOLDOWN_MS-(millis()-lastDiscoveryAt)+999)/1000)+"s before retrying.";
  else if(firmwareCode<8)reason="Discovery needs companion protocol v8+.";
  if(capture){coveragePlace.trim();if(!coveragePlace.length())reason="Name this place before starting the test.";}
  if(reason.length()){discoveryInfo=coverageInfo=reason;return false;}
  if(capture){
    field::Test t;t.id=coverageTests.empty()?1:coverageTests.back().id+1;if(!t.id){coverageInfo="Notebook identifier limit reached.";return false;}t.started=nowSeconds();t.approximate=!clockTrusted;
    memcpy(t.place,coveragePlace.c_str(),min(size_t(24),size_t(coveragePlace.length())));
    auto old=coverageTests;if(coverageTests.size()>=field::MAX_TESTS)coverageTests.erase(coverageTests.begin());coverageTests.push_back(t);saveCoverage();
    if(coverageFailed){coverageTests=std::move(old);coverageDirty=false;coverageInfo="Cannot save test; check screen storage.";return false;}
    coverageActiveId=coverageSelectedId=t.id;coverageCapture=true;coverageReplyPage=0;
  }
  networkAction=NetworkAction::DISCOVER;discoveryInfo="Preparing one radio request...";return true;
}
void coverageDispatched(){if(fixtureMode)return;antennaDispatched();fieldScanState=1;fieldScanAt=millis();if(!coverageCapture)return;auto* t=findCoverage(coverageActiveId);if(t){t->started=nowSeconds();t->state=field::TestState::LISTENING;coverageDirty=true;}refreshCoverage();}
void coverageAccepted(){if(fixtureMode)return;antennaAccepted();if(!coverageCapture)return;auto* t=findCoverage(coverageActiveId);if(t){t->accepted=true;coverageDirty=true;}}
void coverageReply(const discovery::Result& result){
  if(fixtureMode)return;antennaReply(result);noteFieldProof(1,fieldKeyName(result.key));
  if(!coverageCapture)return;auto* t=findCoverage(coverageActiveId);if(!t)return;
  field::Signal s;memcpy(s.key,result.key,32);s.outbound=result.outboundSnrQ;s.inbound=result.inboundSnrQ;s.rssi=result.rssi;field::addReply(*t,s);coverageDirty=true;refreshCoverage();
}
void coverageFinished(field::TestState state){
  if(fixtureMode)return;antennaFinished(state);
  if(fieldScanState==1)fieldScanState=state==field::TestState::COMPLETE?2:state==field::TestState::REJECTED?3:state==field::TestState::WRITE_FAILED?4:5;
  if(!coverageCapture)return;auto* t=findCoverage(coverageActiveId);if(t){t->state=state;t->finished=nowSeconds();t->full=nearby.full;saveCoverage();}
  coverageCapture=false;coverageActiveId=0;refreshCoverage();
}
void tickField(){
  if(coverageCapture&&(!linkActive||!ready))coverageFinished(field::TestState::INTERRUPTED);
  tickFieldStorage();
  if(millis()-fieldPaintAt<1000)return;fieldPaintAt=millis();
  if(screen==Screen::NETWORK)paintDashboard();
}
void returnFromDetail(){
  if(detailReturn==Screen::SENT_RESULTS)drawSentResults();else if(detailReturn==Screen::INBOX)drawInbox();else if(detailReturn==Screen::DIRECT_CHAT)drawDirectChat();else drawChat();
}
bool replyToChannelMessage(){
  if(!detailHasMessage||detailMessage.direct)return false;
  String error;uint8_t channel=detailMessage.channel;
  if(!ready||!linkActive)error="Reconnect the Heltec before replying.";
  else if(channel>=maxChannels||!channels[channel].length()||!haveChannelKey[channel])error="Channel unavailable; reload channels first.";
  else if(pending==Pending::SEND)error="Wait for the current send to finish.";
  else if(draft.length()&&(composingDirect||currentChannel!=channel))error="Another draft is open; finish it first.";
  String sender,body=detailMessage.text;int separator=body.indexOf(": ");
  if(detailMessage.outgoing){sender=wireName.length()?displayText(wireName):"This node";if(separator>0&&separator<=64)body=body.substring(separator+2);}
  else if(separator>0&&separator<=64){sender=body.substring(0,separator);body=body.substring(separator+2);}else sender="Unknown";
  std::string quoted;
  if(!error.length()&&!reply_tools::compose(sender.c_str(),body.c_str(),draft.c_str(),quoted))error="Shorten your draft to make room for the quote.";
  if(error.length()){lcd.fillRect(12,211,296,24,BG);lcd.setTextColor(TFT_ORANGE,BG);lcd.drawString(error.substring(0,49),12,215,1);return false;}
  composingDirect=false;currentChannel=channel;draft=quoted.c_str();upperCase=symbols=false;statusLine="Reply in this channel";
  if(!fixtureMode){saveDraftNow();if(draftSaveFailed)statusLine="Draft save failed";}drawCompose();return true;
}
// Destination and draft preservation checks; no radio command is dispatched.
bool channelReplySelfTest(){
  if(!fixtureMode||client)return false;
  bool wasReady=ready,wasLink=linkActive,wasDirect=composingDirect,hadDetail=detailHasMessage;uint8_t oldChannel=currentChannel,oldMax=maxChannels;
  String oldDraft=draft,oldStatus=statusLine,oldChannelName=channels[1];bool oldKey=haveChannelKey[1];ChatMessage oldMessage=detailMessage;Screen oldScreen=screen;
  ready=linkActive=true;maxChannels=2;channels[1]="#fixture";haveChannelKey[1]=true;composingDirect=false;currentChannel=0;draft="";detailHasMessage=true;detailMessage=ChatMessage{};detailMessage.channel=1;detailMessage.text="Alice: Meet at the gate";
  bool ok=replyToChannelMessage()&&currentChannel==1&&!composingDirect&&draft=="> Alice: Meet at the gate\n"&&screen==Screen::COMPOSE;
  draft+="On my way";detailMessage.text="Bob: Bring a torch";ok=replyToChannelMessage()&&draft=="> Bob: Bring a torch\nOn my way"&&ok;
  currentChannel=0;draft="Keep other channel draft";ok=!replyToChannelMessage()&&currentChannel==0&&draft=="Keep other channel draft"&&ok;
  composingDirect=true;draft="Keep private draft";ok=!replyToChannelMessage()&&composingDirect&&draft=="Keep private draft"&&ok;
  detailMessage.direct=true;ok=!replyToChannelMessage()&&ok;
  ready=wasReady;linkActive=wasLink;composingDirect=wasDirect;currentChannel=oldChannel;maxChannels=oldMax;channels[1]=oldChannelName;haveChannelKey[1]=oldKey;draft=oldDraft;statusLine=oldStatus;detailHasMessage=hadDetail;detailMessage=oldMessage;screen=oldScreen;return ok;
}
bool fieldTouch(int x,int y){
  if(screen==Screen::NETWORK){
    if(y>=4&&y<=30){if(x>=164&&x<=224)drawSentResults();else if(x>=230&&x<=308)drawCoverage();}
    else if(y>=177&&y<=205){if(x>=12&&x<=80){drawRadioStats();requestStats();}else if(x>=84&&x<=168)drawRadioTools();else if(x>=174&&x<=234)drawNodes();else if(x>=240&&x<=308)drawChannels();}return true;
  }
  if(screen==Screen::SENT_RESULTS){
    if(y>=42&&y<170&&x>=12&&x<=308&&(y-42)%32<28){int i=int(sendResults.size())-1-(sentPage*4+(y-42)/32);if(i>=0)inspectSend(sendResults[i].timestamp);}
    else if(y>=177&&y<=205){if(x>=12&&x<=80){sentPage--;drawSentResults();}else if(x>=84&&x<=152){sentPage++;drawSentResults();}else if(x>=216&&x<=308)drawNetwork();}return true;
  }
  if(screen==Screen::MESSAGE_DETAIL){if(y>=177&&y<=205){if(x>=12&&x<=104)returnFromDetail();else if(x>=110&&x<=220)drawSentResults();else if(x>=228&&x<=308)replyToChannelMessage();}return true;}
  if(screen==Screen::COVERAGE){
    if(y>=4&&y<=30&&x>=144&&x<=224){drawAntennaList();return true;}
    if(y>=4&&y<=30&&x>=230&&x<=308){coverageInfo="";drawCoverageNew();}
    else if(y>=42&&y<170&&x>=12&&x<=308&&(y-42)%32<28){int i=int(coverageTests.size())-1-(coveragePage*4+(y-42)/32);if(i>=0){coverageSelectedId=coverageTests[i].id;coverageReplyPage=0;drawCoverageTest();}}
    else if(y>=177&&y<=205){if(x>=12&&x<=80){coveragePage--;drawCoverage();}else if(x>=84&&x<=152){coveragePage++;drawCoverage();}else if(x>=216&&x<=308)drawNetwork();}return true;
  }
  if(screen==Screen::COVERAGE_NEW){
    if(x>=12&&x<=308&&y>=43&&y<=74){textPurpose=5;editorReturn=Screen::COVERAGE_NEW;editText=coveragePlace;textLimit=24;upperCase=false;symbols=false;drawTextEditor();}
    else if(y>=177&&y<=205){if(x>=12&&x<=202){if(queueNearbyScan(true))drawCoverageTest();else drawCoverageNew();}else if(x>=216&&x<=308)drawCoverage();}return true;
  }
  if(screen==Screen::COVERAGE_TEST){
    auto* t=findCoverage(coverageSelectedId);
    if(y>=79&&y<169&&x>=12&&x<=308&&(y-79)%31<27&&t){int i=coverageReplyPage*3+(y-79)/31;if(i<int(t->replies.size())){coverageSignalIndex=i;drawCoverageSignal();}}
    else if(y>=177&&y<=205){if(x>=12&&x<=68){coverageReplyPage--;drawCoverageTest();}else if(x>=72&&x<=128){coverageReplyPage++;drawCoverageTest();}else if(x>=132&&x<=212){if(t)coveragePlace=t->place;coverageInfo="";drawCoverageNew();}else if(x>=216&&x<=308)drawCoverage();}return true;
  }
  if(screen==Screen::COVERAGE_SIGNAL){if(x>=216&&x<=308&&y>=177&&y<=205)drawCoverageTest();return true;}
  return false;
}
