constexpr int CHAT_X=12,CHAT_Y=43,CHAT_WIDTH=296,CHAT_HEIGHT=128,CHAT_LINE=11;
struct TimelineRow { uint64_t id; int message,ordinal,start,end; };
std::vector<ChatFlow> timelineFlows;
std::vector<TimelineRow> timelineRows;
scroll_tools::Position chatPosition;
String timelineKey;
TFT_eSprite chatCanvas(&lcd);
bool chatCanvasReady=false,chatCanvasAttempted=false;
int chatBandHeight=16;
bool chatGesture=false,chatMoved=false;
int chatSamples[3]={},chatSample=0,chatLastY=0,chatStartY=0;
uint32_t chatLastDown=0,chatLastPaint=0;
uint64_t chatTapId=0;

bool messageScreen(){return screen==Screen::CHAT||screen==Screen::DIRECT_CHAT||screen==Screen::INBOX;}
void releaseChatCanvas(){if(chatCanvasReady){chatCanvas.deleteSprite();chatCanvasReady=false;}chatCanvasAttempted=false;chatGesture=false;}
void chatButtons() {
  smallButton(12,177,140,27,chatPosition.newMessages?"New messages v":"Latest v");
  smallButton(156,177,68,27,screen==Screen::INBOX?"Channels":"Write");smallButton(228,177,80,27,"Back");
}
void drawBufferedChatRow(TFT_eSprite& target,const ChatFlow& flow,const TimelineRow& row,int y) {
  int pos=row.start;
  while(pos<row.end) {
    uint16_t colour=pos<6?uint16_t(0x5C7F):pos<flow.userEnd?uint16_t(TFT_ORANGE):uint16_t(TFT_WHITE);
    int stop=pos<6?min(row.end,6):pos<flow.userEnd?min(row.end,flow.userEnd):row.end;
    target.setTextColor(colour,BG);target.drawString(flow.text.substring(pos,stop),(pos-row.start)*6,y,1);pos=stop;
  }
}
void paintTimeline() {
  if(!chatCanvasAttempted) {
    chatCanvasAttempted=true;chatCanvas.setColorDepth(16);
    for(chatBandHeight=16;chatBandHeight>=1;chatBandHeight/=2)if(chatCanvas.createSprite(CHAT_WIDTH,chatBandHeight)){chatCanvasReady=true;break;}
    Serial.printf("Chat scrolling: %s; band rows %d; free heap %u\n",chatCanvasReady?"buffered strips":"low-memory fallback",chatBandHeight,ESP.getFreeHeap());
  }
  lcd.setTextSize(1);
  if(chatCanvasReady){chatCanvas.setTextSize(1);chatCanvas.setTextFont(1);chatCanvas.setTextDatum(TL_DATUM);}
  int padding=max(0,CHAT_HEIGHT-chatPosition.height);
  // Read markers are updated once per visible line, independently of the bands.
  for(size_t i=0;i<timelineRows.size();i++) {
    int y=padding+int(i)*CHAT_LINE-chatPosition.offset;auto& row=timelineRows[i];
    if(y>=0&&y+8<=CHAT_HEIGHT&&row.message<int(messages.size()))noteRead(messages[row.message]);
  }
  if(chatCanvasReady) {
    // Paint each strip completely in RAM before replacing that strip on screen.
    // Even the one-row fallback needs only 594 bytes, not a 38 KB contiguous block.
    for(int top=0;top<CHAT_HEIGHT;top+=chatBandHeight) {
      chatCanvas.fillSprite(BG);
      for(size_t i=0;i<timelineRows.size();i++) {
        int y=padding+int(i)*CHAT_LINE-chatPosition.offset-top;if(y+8<=0||y>=chatBandHeight)continue;
        auto& row=timelineRows[i];drawBufferedChatRow(chatCanvas,timelineFlows[row.message],row,y);
      }
      if(timelineRows.empty()){chatCanvas.setTextColor(TFT_WHITE,BG);chatCanvas.drawString("No messages yet.",0,12-top,2);}
      chatCanvas.pushSprite(CHAT_X,CHAT_Y+top);
    }
  } else {
  lcd.fillRect(CHAT_X,CHAT_Y,CHAT_WIDTH,CHAT_HEIGHT,BG);
  for(size_t i=0;i<timelineRows.size();i++) {
    int y=padding+int(i)*CHAT_LINE-chatPosition.offset;if(y+8<=0||y>=CHAT_HEIGHT)continue;
    auto& row=timelineRows[i];auto& flow=timelineFlows[row.message];
    int pos=row.start;
    while(pos<row.end) {
      uint16_t colour=pos<6?uint16_t(0x5C7F):pos<flow.userEnd?uint16_t(TFT_ORANGE):uint16_t(TFT_WHITE);
      int stop=pos<6?min(row.end,6):pos<flow.userEnd?min(row.end,flow.userEnd):row.end;
      int x=(pos-row.start)*6;
      if(y>=0&&y+8<=CHAT_HEIGHT){lcd.setTextColor(colour,BG);lcd.drawString(flow.text.substring(pos,stop),CHAT_X+x,CHAT_Y+y,1);}
      pos=stop;
    }
  }
  if(timelineRows.empty()) {
    lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("No messages yet.",CHAT_X,CHAT_Y+12,2);
  }
  }
  lcd.fillRect(311,CHAT_Y,3,CHAT_HEIGHT,BG);
  if(chatPosition.maximum()>0) {
    int thumb=max(10,CHAT_HEIGHT*CHAT_HEIGHT/chatPosition.height);
    int top=chatPosition.offset*(CHAT_HEIGHT-thumb)/chatPosition.maximum();
    lcd.fillRect(311,CHAT_Y+top,3,thumb,ACCENT);
  }
  chatLastPaint=millis();
}
void buildTimeline(bool inbox) {
  String key=activeAddress+(screen==Screen::INBOX?"/inbox":screen==Screen::DIRECT_CHAT?"/dm/"+hexText(directPeer,6):"/ch/"+String(currentChannel));
  bool reset=resetChatView||key!=timelineKey;
  uint64_t anchorId=0;int anchorOrdinal=0,within=chatPosition.offset%CHAT_LINE;
  if(!reset&&!chatPosition.following&&!timelineRows.empty()) {
    size_t first=min(size_t(chatPosition.offset/CHAT_LINE),timelineRows.size()-1);
    anchorId=timelineRows[first].id;anchorOrdinal=timelineRows[first].ordinal;
  }
  auto list=visibleMessages(inbox);timelineRows.clear();timelineFlows.clear();timelineFlows.resize(messages.size());
  for(int index:list) {
    auto& m=messages[index];timelineFlows[index]=chatFlow(m);auto rows=chatRows(timelineFlows[index].text);
    uint64_t id=(uint64_t(m.outgoing?0x80000000u:m.sequence)<<32)|m.timestamp;
    for(size_t row=0;row<rows.size();row++)timelineRows.push_back({id,index,int(row),rows[row].start,rows[row].end});
  }
  int height=timelineRows.size()*CHAT_LINE;
  if(reset){chatPosition.reset(height);chatGesture=false;}
  else {
    int anchor=0;
    if(anchorId)for(size_t i=0;i<timelineRows.size();i++)if(timelineRows[i].id==anchorId&&timelineRows[i].ordinal==anchorOrdinal){anchor=int(i)*CHAT_LINE+within;break;}
    chatPosition.resize(height,anchor);
  }
  timelineKey=key;resetChatView=false;
}
void historyBody(bool inbox){buildTimeline(inbox);paintTimeline();chatButtons();paintFooter();}
void jumpToLatest(){chatPosition.latest();paintTimeline();chatButtons();statusLine="Connected";paintFooter();}
void noteTimelineIncoming(const Reply& reply) {
  if(!messageScreen()||chatPosition.following)return;
  protocol::MessageView view;if(!protocol::message(reply.data,reply.length,view))return;
  bool relevant=screen==Screen::CHAT?!view.direct&&view.channel==currentChannel:
    view.direct&&(screen==Screen::INBOX||!memcmp(reply.data+(reply.code==16?4:1),directPeer,6));
  if(relevant)chatPosition.newMessages=true;
}
bool handleChatDrag(bool down) {
  if(!messageScreen()){chatGesture=false;return false;}
  if(displayDimmed)return false;
  if(!down) {
    if(!chatGesture)return false;
    if(millis()-chatLastDown>=70){chatGesture=false;if(!chatMoved&&chatTapId){inspectMessage(chatTapId);return true;}paintTimeline();chatButtons();paintFooter();}
    return true;
  }
  if(!chatGesture&&pressed)return false;
  TS_Point raw=touch.getPoint();int x=lroundf(ax*raw.x+bx*raw.y+cx),y=lroundf(ay*raw.x+by*raw.y+cy);
  if(!chatGesture) {
    if(x<CHAT_X||x>=CHAT_X+CHAT_WIDTH||y<CHAT_Y||y>=CHAT_Y+CHAT_HEIGHT)return false;
    chatGesture=true;chatMoved=false;chatStartY=chatLastY=y;chatSample=0;chatTapId=0;
    int row=(y-CHAT_Y+chatPosition.offset-max(0,CHAT_HEIGHT-chatPosition.height));
    if(row>=0&&row/CHAT_LINE<int(timelineRows.size()))chatTapId=timelineRows[row/CHAT_LINE].id;
    for(auto& sample:chatSamples)sample=y;
  }
  lastTouchAt=chatLastDown=millis();
  if(x<-10||x>330||y<-20||y>260)return true;
  chatSamples[chatSample++%3]=constrain(y,0,239);int filtered=scroll_tools::median(chatSamples[0],chatSamples[1],chatSamples[2]);
  if(!chatMoved&&abs(filtered-chatStartY)>=4)chatMoved=true;
  if(chatMoved) {
    int delta=constrain(filtered-chatLastY,-48,48);chatLastY+=delta;bool hadNotice=chatPosition.newMessages;
    chatPosition.drag(delta);
    if(millis()-chatLastPaint>=33){paintTimeline();if(hadNotice!=chatPosition.newMessages)chatButtons();}
  }
  return true;
}
bool scrollUiSelfTest() {
  Screen original=screen;uint8_t originalChannel=currentChannel;
  screen=Screen::CHAT;currentChannel=0;resetChatView=true;messages.clear();incomingSequence=0;
  for(int i=0;i<20;i++)remember(0,false,"Unit: scrolling fixture",i+1);
  buildTimeline(false);bool ok=chatPosition.following&&chatPosition.offset==92;
  chatPosition.drag(44);int before=chatPosition.offset;
  remember(0,false,"Unit: arriving fixture",21);buildTimeline(false);
  ok=ok&&!chatPosition.following&&chatPosition.offset==before;
  Reply different{};different.code=8;different.length=9;different.data[0]=8;different.data[1]=1;
  noteTimelineIncoming(different);ok=ok&&!chatPosition.newMessages;
  different.data[1]=0;noteTimelineIncoming(different);ok=ok&&chatPosition.newMessages;
  messages.erase(messages.begin());buildTimeline(false);ok=ok&&chatPosition.offset==before-CHAT_LINE;
  chatPosition.latest();remember(0,false,"Unit: latest fixture",22);buildTimeline(false);
  ok=ok&&chatPosition.following&&chatPosition.offset==chatPosition.maximum()&&!chatPosition.newMessages;
  messages.clear();incomingSequence=0;timelineRows.clear();timelineFlows.clear();timelineKey="";chatPosition.reset(0);
  screen=original;currentChannel=originalChannel;resetChatView=true;return ok;
}
bool scrollBandSelfTest() {
  // Compare buffered strips with a complete reference image, including glyphs
  // crossing strip edges and both viewport edges. This never touches the LCD.
  TFT_eSprite reference(&lcd),band(&lcd);reference.setColorDepth(16);band.setColorDepth(16);
  if(!reference.createSprite(64,32)||!band.createSprite(64,8)){reference.deleteSprite();band.deleteSprite();return false;}
  reference.setTextSize(1);reference.setTextFont(1);reference.setTextDatum(TL_DATUM);reference.fillSprite(BG);
  band.setTextSize(1);band.setTextFont(1);band.setTextDatum(TL_DATUM);
  ChatFlow flow={"00:00 A: abc",9};TimelineRow row={0,0,0,0,int(flow.text.length())};
  const int positions[]={-2,9,20,31};for(int y:positions)drawBufferedChatRow(reference,flow,row,y);
  bool ok=true;
  for(int top=0;top<32;top+=8){band.fillSprite(BG);for(int y:positions)drawBufferedChatRow(band,flow,row,y-top);
    for(int y=0;y<8;y++)for(int x=0;x<64;x++)if(reference.readPixel(x,top+y)!=band.readPixel(x,y))ok=false;}
  reference.deleteSprite();band.deleteSprite();return ok;
}
