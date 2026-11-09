constexpr int CHAT_X=12,CHAT_Y=43,CHAT_WIDTH=296,CHAT_HEIGHT=128;
struct BubbleLine {int start,end,width;};
struct TimelineBubble {uint64_t id;int message,top,height,x,width;String sender,body,time;std::vector<BubbleLine> lines;};
std::vector<TimelineBubble> timelineBubbles;
scroll_tools::Position chatPosition;
String timelineKey;
TFT_eSprite chatCanvas(&lcd);
bool chatCanvasReady=false,chatCanvasAttempted=false;
int chatBandHeight=16;
bool chatGesture=false,chatMoved=false;
int chatSamples[3]={},chatSample=0,chatLastY=0,chatStartY=0;
uint32_t chatLastDown=0,chatLastPaint=0;
uint64_t chatTapId=0;
scroll_tools::DetailHold chatDetailHold;

bool messageScreen(){return screen==Screen::CHAT||screen==Screen::DIRECT_CHAT||screen==Screen::INBOX;}
void releaseChatCanvas(){if(chatCanvasReady){chatCanvas.deleteSprite();chatCanvasReady=false;}chatCanvasAttempted=false;chatGesture=false;}
void chatButtons(){smallButton(12,177,140,27,chatPosition.newMessages?"New messages v":"Latest v");smallButton(156,177,68,27,screen==Screen::INBOX?"Channels":"Write");smallButton(228,177,80,27,"Back");}
String bubbleBody(String body){
  const char* labels[]={"[smile]","[heart]","[thumbs up]","[check]","[warning]","[SOS]","[hand]","[wave]","[cat]","[laugh]"};
  for(int i=0;i<10;i++){char marker[2]={char(128+i),0};body.replace(labels[i],marker);}return body;
}
int bubbleCharWidth(char c){if(uint8_t(c)>=128)return 18;char glyph[2]={c,0};return lcd.textWidth(glyph,2);}
std::vector<BubbleLine> bubbleLines(const String& text){
  std::vector<BubbleLine> rows;int start=0,length=text.length();
  while(start<length){int end=start,width=0,space=-1,spaceWidth=0;
    while(end<length){int next=bubbleCharWidth(text[end]);if(width+next>244)break;if(text[end]==' '){space=end;spaceWidth=width;}width+=next;end++;}
    if(end<length&&space>start){end=space;width=spaceWidth;}if(end==start){end++;width=bubbleCharWidth(text[start]);}
    rows.push_back({start,end,width});start=end;while(start<length&&text[start]==' ')start++;
  }if(rows.empty())rows.push_back({0,0,0});return rows;
}
void bubbleIcon(TFT_eSprite& t,uint8_t icon,int x,int y,uint16_t background){
  const uint16_t yellow=0xFEA0,red=0xF986;int cx=x+8,cy=y+8;
  if(icon==1){t.fillCircle(x+5,y+5,4,red);t.fillCircle(x+11,y+5,4,red);t.fillTriangle(x+1,y+6,x+15,y+6,x+8,y+15,red);}
  else if(icon==3){t.drawLine(x+2,y+8,x+6,y+12,TFT_GREEN);t.drawLine(x+6,y+12,x+14,y+3,TFT_GREEN);t.drawLine(x+2,y+9,x+6,y+13,TFT_GREEN);t.drawLine(x+6,y+13,x+14,y+4,TFT_GREEN);}
  else if(icon==4){t.fillTriangle(x+8,y,x,y+15,x+16,y+15,yellow);t.fillRect(x+7,y+5,2,6,background);t.fillRect(x+7,y+12,2,2,background);}
  else if(icon==5){t.fillRoundRect(x,y+1,17,14,2,red);t.setTextColor(TFT_WHITE,red);t.drawString("SOS",x,y+4,1);}
  else if(icon==2){t.fillRoundRect(x+5,y+6,10,9,2,yellow);t.fillRoundRect(x+7,y+1,4,9,2,yellow);t.fillRect(x+1,y+7,3,8,0x5C7F);}
  else if(icon==6||icon==7){t.fillRoundRect(x+4,y+7,10,9,3,yellow);for(int i=0;i<4;i++)t.fillRoundRect(x+3+i*3,y+1+(i%3==0?2:0),2,10,1,yellow);t.drawLine(x+1,y+8,x+5,y+13,yellow);}
  else {if(icon==8){t.fillTriangle(x+1,y,x+2,y+9,x+7,y+5,yellow);t.fillTriangle(x+15,y,x+14,y+9,x+9,y+5,yellow);}
    t.fillCircle(cx,cy,7,yellow);t.fillCircle(x+5,y+6,1,background);t.fillCircle(x+11,y+6,1,background);
    t.drawLine(x+4,y+10,x+6,y+12,background);t.drawLine(x+6,y+12,x+10,y+12,background);t.drawLine(x+10,y+12,x+12,y+10,background);
    if(icon==9){t.fillCircle(x+1,y+9,2,0x5C7F);t.fillCircle(x+15,y+9,2,0x5C7F);}}
}
String bubbleStatus(const ChatMessage& m){
  if(!m.outgoing)return "";
  for(const auto& result:sendResults)if(result.timestamp==m.timestamp){switch(result.state){
    case field::SendState::ACK:return "Radio ACK";case field::SendState::REPEAT:return "Repeated";
    case field::SendState::REQUESTING:return "Sending";case field::SendState::ACCEPTED:return "Accepted";
    case field::SendState::REJECTED:return "Rejected";default:return "Unconfirmed";}}
  return "Unconfirmed";
}
void drawBubble(TFT_eSprite& target,const TimelineBubble& b,int y){
  bool outgoing=messages[b.message].outgoing;uint16_t background=outgoing?uint16_t(0x124B):uint16_t(0x2968);
  target.fillRoundRect(b.x,y,b.width,b.height-7,8,background);
  target.setTextSize(1);target.setTextColor(outgoing?uint16_t(0x8FFB):uint16_t(0xFD8E),background);target.drawString(b.sender,b.x+9,y+6,1);
  for(size_t line=0;line<b.lines.size();line++){int yy=y+20+int(line)*19;if(yy+17<=0||yy>=target.height())continue;
    auto& row=b.lines[line];int x=b.x+9;
    for(int i=row.start;i<row.end;i++){uint8_t c=b.body[i];if(c>=128)bubbleIcon(target,c-128,x,yy,background);
      else {char glyph[2]={char(c),0};target.setTextColor(TFT_WHITE,background);target.drawString(glyph,x,yy,2);}x+=bubbleCharWidth(c);}}
  String meta=b.time,delivery=bubbleStatus(messages[b.message]);if(delivery.length())meta+="  "+delivery;
  target.setTextColor(0x5C7F,background);target.drawString(meta,b.x+b.width-9-meta.length()*6,y+6,1);
}
void paintTimeline(){
  if(!chatCanvasAttempted){chatCanvasAttempted=true;chatCanvas.setColorDepth(16);
    for(chatBandHeight=16;chatBandHeight>=1;chatBandHeight/=2)if(chatCanvas.createSprite(CHAT_WIDTH,chatBandHeight)){chatCanvasReady=true;break;}
    Serial.printf("Chat bubbles: %s; band rows %d; free heap %u\n",chatCanvasReady?"buffered strips":"unavailable",chatBandHeight,ESP.getFreeHeap());}
  int padding=max(0,CHAT_HEIGHT-chatPosition.height);lcd.setTextSize(1);
  for(const auto& b:timelineBubbles){int y=padding+b.top-chatPosition.offset;if(y<CHAT_HEIGHT&&y+b.height-7>0)noteRead(messages[b.message]);}
  if(chatCanvasReady){chatCanvas.setTextSize(1);chatCanvas.setTextDatum(TL_DATUM);
    for(int top=0;top<CHAT_HEIGHT;top+=chatBandHeight){chatCanvas.fillSprite(BG);
      for(const auto& b:timelineBubbles){int y=padding+b.top-chatPosition.offset-top;if(y+b.height<=0||y>=chatBandHeight)continue;drawBubble(chatCanvas,b,y);}
      if(timelineBubbles.empty()){chatCanvas.setTextColor(TFT_WHITE,BG);chatCanvas.drawString("No messages yet.",0,12-top,2);}chatCanvas.pushSprite(CHAT_X,CHAT_Y+top);}
  }else {lcd.fillRect(CHAT_X,CHAT_Y,CHAT_WIDTH,CHAT_HEIGHT,BG);lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Low display memory. Reopen chat.",CHAT_X,CHAT_Y+20,2);}
  lcd.fillRect(311,CHAT_Y,3,CHAT_HEIGHT,BG);if(chatPosition.maximum()>0){int thumb=max(10,CHAT_HEIGHT*CHAT_HEIGHT/chatPosition.height);int top=chatPosition.offset*(CHAT_HEIGHT-thumb)/chatPosition.maximum();lcd.fillRect(311,CHAT_Y+top,3,thumb,ACCENT);}chatLastPaint=millis();
}
void buildTimeline(bool inbox){
  lcd.setTextSize(1);
  String key=activeAddress+(screen==Screen::INBOX?"/inbox":screen==Screen::DIRECT_CHAT?"/dm/"+hexText(directPeer,6):"/ch/"+String(currentChannel));
  bool reset=resetChatView||key!=timelineKey;uint64_t anchorId=0;int within=0;
  if(!reset&&!chatPosition.following)for(const auto& b:timelineBubbles)if(b.top+b.height>chatPosition.offset){anchorId=b.id;within=chatPosition.offset-b.top;break;}
  auto list=visibleMessages(inbox);timelineBubbles.clear();int height=0;
  for(int index:list){auto& m=messages[index];auto flow=chatFlow(m);TimelineBubble b{};
    b.id=(uint64_t(m.outgoing?0x80000000u:m.sequence)<<32)|m.timestamp;b.message=index;b.top=height;
    b.sender=m.outgoing?"You":flow.text.substring(6,flow.userEnd-2);
    b.body=bubbleBody(flow.text.substring(flow.userEnd));b.time=flow.text.substring(0,5);b.lines=bubbleLines(b.body);
    String delivery=bubbleStatus(m);int metaWidth=(b.time.length()+(delivery.length()?2+delivery.length():0))*6;
    int nameChars=(268-18-12-metaWidth)/6;
    if(int(b.sender.length())>nameChars)b.sender=b.sender.substring(0,nameChars-3)+"...";
    int width=int(b.sender.length())*6+12+metaWidth;for(const auto& row:b.lines)width=max(width,row.width);
    b.width=min(268,max(92,width+18));b.x=m.outgoing?CHAT_WIDTH-b.width:0;b.height=31+b.lines.size()*19;height+=b.height;timelineBubbles.push_back(std::move(b));}
  if(reset){chatPosition.reset(height);chatGesture=false;}else{int anchor=0;if(anchorId)for(const auto& b:timelineBubbles)if(b.id==anchorId){anchor=b.top+within;break;}chatPosition.resize(height,anchor);}
  timelineKey=key;resetChatView=false;
}
void refreshChatDelivery(){if(messageScreen()){buildTimeline(screen!=Screen::CHAT);paintTimeline();}}
void historyBody(bool inbox){buildTimeline(inbox);paintTimeline();chatButtons();paintFooter();}
void jumpToLatest(){chatPosition.latest();paintTimeline();chatButtons();statusLine="Connected";paintFooter();}
void noteTimelineIncoming(const Reply& reply){
  if(!messageScreen()||chatPosition.following)return;protocol::MessageView view;if(!protocol::message(reply.data,reply.length,view))return;
  bool relevant=screen==Screen::CHAT?!view.direct&&view.channel==currentChannel:view.direct&&(screen==Screen::INBOX||!memcmp(reply.data+(reply.code==16?4:1),directPeer,6));if(relevant)chatPosition.newMessages=true;
}
bool handleChatDrag(bool down) {
  if(!messageScreen()){chatGesture=false;return false;}
  if(displayDimmed)return false;
  if(!down) {
    if(!chatGesture)return false;
    if(millis()-chatLastDown>=70){chatGesture=false;if(chatDetailHold.opens(chatMoved)&&chatTapId){inspectMessage(chatTapId);return true;}paintTimeline();chatButtons();paintFooter();}
    return true;
  }
  if(!chatGesture&&pressed)return false;
  TS_Point raw=touch.getPoint();int x=lroundf(ax*raw.x+bx*raw.y+cx),y=lroundf(ay*raw.x+by*raw.y+cy);
  if(!chatGesture) {
    if(x<CHAT_X||x>=CHAT_X+CHAT_WIDTH||y<CHAT_Y||y>=CHAT_Y+CHAT_HEIGHT)return false;
    chatGesture=true;chatMoved=false;chatStartY=chatLastY=y;chatSample=0;chatTapId=0;chatDetailHold.begin(x,y,millis());
    int row=(y-CHAT_Y+chatPosition.offset-max(0,CHAT_HEIGHT-chatPosition.height));
    for(const auto& b:timelineBubbles)if(row>=b.top&&row<b.top+b.height-7&&x-CHAT_X>=b.x&&x-CHAT_X<b.x+b.width){chatTapId=b.id;break;}
    for(auto& sample:chatSamples)sample=y;
  }
  lastTouchAt=chatLastDown=millis();
  if(x<-10||x>330||y<-20||y>260){chatDetailHold.eligible=false;return true;}
  chatSamples[chatSample++%3]=constrain(y,0,239);int filtered=scroll_tools::median(chatSamples[0],chatSamples[1],chatSamples[2]);
  if(!chatMoved&&abs(filtered-chatStartY)>=4)chatMoved=true;
  chatDetailHold.observe(x,y,millis(),chatMoved);
  if(chatMoved) {
    int delta=constrain(filtered-chatLastY,-48,48);chatLastY+=delta;bool hadNotice=chatPosition.newMessages;
    chatPosition.drag(delta);
    if(millis()-chatLastPaint>=33){paintTimeline();if(hadNotice!=chatPosition.newMessages)chatButtons();}
  }
  return true;
}

bool scrollUiSelfTest(){
  Screen original=screen;uint8_t originalChannel=currentChannel;screen=Screen::CHAT;currentChannel=0;resetChatView=true;messages.clear();incomingSequence=0;
  for(int i=0;i<20;i++)remember(0,false,"Unit: scrolling fixture",i+1);buildTimeline(false);
  bool ok=chatPosition.following&&chatPosition.offset==chatPosition.maximum()&&timelineBubbles.size()==20&&timelineBubbles[0].height>=50;
  int removedHeight=timelineBubbles[0].height;chatPosition.drag(44);int before=chatPosition.offset;
  remember(0,false,"Unit: arriving fixture",21);buildTimeline(false);ok=ok&&!chatPosition.following&&chatPosition.offset==before;
  Reply different{};different.code=8;different.length=9;different.data[0]=8;different.data[1]=1;noteTimelineIncoming(different);ok=ok&&!chatPosition.newMessages;
  different.data[1]=0;noteTimelineIncoming(different);ok=ok&&chatPosition.newMessages;messages.erase(messages.begin());buildTimeline(false);ok=ok&&chatPosition.offset==before-removedHeight;
  chatPosition.latest();remember(0,false,"Me: A much longer outgoing bubble with a [heart] and [thumbs up] and averylongwordthatmustwrapwithoutlosinganycharacters",22,nullptr,true);buildTimeline(false);
  ok=ok&&chatPosition.following&&chatPosition.offset==chatPosition.maximum()&&!chatPosition.newMessages;
  const auto& b=timelineBubbles.back();ok=ok&&b.x+b.width==CHAT_WIDTH&&b.lines.size()>1&&b.sender=="You";
  for(const auto& row:b.lines)ok=ok&&row.width<=244&&row.end>row.start;
  messages.clear();incomingSequence=0;timelineBubbles.clear();timelineKey="";chatPosition.reset(0);screen=original;currentChannel=originalChannel;resetChatView=true;return ok;
}
bool scrollBandSelfTest(){
  TFT_eSprite reference(&lcd),band(&lcd);reference.setColorDepth(16);band.setColorDepth(16);
  if(!reference.createSprite(CHAT_WIDTH,96)||!band.createSprite(CHAT_WIDTH,8)){reference.deleteSprite();band.deleteSprite();return false;}
  messages.push_back({0,false,"",1,{},false,1});TimelineBubble b{};b.message=0;b.x=0;b.width=268;b.height=69;b.sender="Fixture";b.time="12:34";
  b.body=bubbleBody("Hello [heart] [smile] [thumbs up] [warning] [check] [SOS] [cat] [hand] [laugh]");b.lines=bubbleLines(b.body);
  reference.fillSprite(BG);drawBubble(reference,b,-5);bool ok=true;
  for(int top=0;top<96;top+=8){band.fillSprite(BG);drawBubble(band,b,-5-top);for(int y=0;y<8;y++)for(int x=0;x<CHAT_WIDTH;x++)if(reference.readPixel(x,top+y)!=band.readPixel(x,y))ok=false;}
  messages.clear();reference.deleteSprite();band.deleteSprite();return ok;
}
void chatPreview(){
  // Render invented messages only, to USB. Never display/alter user history or send RF.
  auto savedMessages=std::move(messages);auto savedBubbles=std::move(timelineBubbles);auto savedPosition=chatPosition;String savedKey=timelineKey;
  Screen savedScreen=screen;uint8_t savedChannel=currentChannel;bool savedReset=resetChatView;uint32_t savedSequence=incomingSequence;
  messages.clear();screen=Screen::CHAT;currentChannel=0;resetChatView=true;
  remember(0,false,"Alex: We are home safely [heart]",43200);remember(0,false,"Me: Good to hear! [thumbs up]",43260,nullptr,true);buildTimeline(false);
  TFT_eSprite preview(&lcd);preview.setColorDepth(16);
  if(preview.createSprite(CHAT_WIDTH,CHAT_HEIGHT)){
    preview.fillSprite(BG);for(const auto& b:timelineBubbles)drawBubble(preview,b,max(0,CHAT_HEIGHT-chatPosition.height)+b.top-chatPosition.offset);
    Serial.printf("PREVIEW %d %d\n",CHAT_WIDTH,CHAT_HEIGHT);
    uint8_t row[CHAT_WIDTH*2];for(int y=0;y<CHAT_HEIGHT;y++){for(int x=0;x<CHAT_WIDTH;x++){uint16_t pixel=preview.readPixel(x,y);row[x*2]=pixel>>8;row[x*2+1]=pixel;}Serial.write(row,sizeof(row));}Serial.println("\nPREVIEW END");preview.deleteSprite();
  }
  messages=std::move(savedMessages);timelineBubbles=std::move(savedBubbles);chatPosition=savedPosition;timelineKey=savedKey;screen=savedScreen;currentChannel=savedChannel;resetChatView=savedReset;incomingSequence=savedSequence;
}
