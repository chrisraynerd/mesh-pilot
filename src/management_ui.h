// Included after the existing radio and drawing helpers in main.cpp.
String hexText(const uint8_t* data,size_t length) {
  const char* digits="0123456789abcdef";String result;result.reserve(length*2);
  for(size_t i=0;i<length;i++){result+=digits[data[i]>>4];result+=digits[data[i]&15];}return result;
}
std::vector<int> filteredContacts(bool nodes) {
  std::vector<int> list;uint8_t filter=nodes?nodeFilter:contactFilter;
  String find=contactSearch;find.toLowerCase();
  for(size_t i=0;i<contacts.size();i++) {
    auto& c=contacts[i];
    if(filter>=1&&filter<=4&&c.type!=filter)continue;
    if(filter==5&&!(c.flags&1))continue;
    if(filter==6&&!c.seen)continue;
    String label=displayText(String(c.name));label.toLowerCase();
    if(find.length()&&label.indexOf(find)<0&&hexText(c.key,6).indexOf(find)<0)continue;
    list.push_back(i);
  }
  std::sort(list.begin(),list.end(),[](int a,int b){
    bool fa=contacts[a].flags&1,fb=contacts[b].flags&1;if(fa!=fb)return fa;
    String na=displayText(String(contacts[a].name)),nb=displayText(String(contacts[b].name));na.toLowerCase();nb.toLowerCase();
    int order=na.compareTo(nb);return order?order<0:memcmp(contacts[a].key,contacts[b].key,32)<0;
  });return list;
}
bool managementIdle() {
  if(!ready||!linkActive||pending!=Pending::NONE||networkAction!=NetworkAction::NONE||remoteWaiting) {
    managementInfo="Radio busy; please try again shortly";return false;
  }return true;
}
void openDirectContact() {
  if(nodeSelected<0||nodeSelected>=int(contacts.size())||contacts[nodeSelected].type!=1)return;
  auto& c=contacts[nodeSelected];if(!c.saved){managementInfo="Save this discovery first (Edit > Save)";drawNode();return;}
  if(!composingDirect||memcmp(directPeer,c.key,32))draft="";
  composingDirect=true;memcpy(directPeer,c.key,32);directName=c.name;historyPage=0;drawDirectChat();
}
void drawChannelSlots() {
  screen=Screen::CHANNEL_SLOTS;header("Manage channels");
  int pages=max(1,int((maxChannels+3)/4));slotPage=constrain(slotPage,0,pages-1);
  for(int row=0;row<4;row++) {
    int slot=slotPage*4+row;if(slot>=maxChannels)break;
    String label=String(slot+1)+": "+(channels[slot].length()?channels[slot]:String("+ Add channel"));
    smallButton(12,42+row*32,296,28,label.substring(0,32).c_str());
  }
  smallButton(12,177,68,28,"Prev");smallButton(84,177,68,28,"Next");smallButton(228,177,80,28,"Back");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(managementInfo.length()?managementInfo.substring(0,49):String(slotPage+1)+"/"+String(pages)+"; "+String(maxChannels)+" slots on Heltec",12,215,1);
}
void openChannelEditor(int slot) {
  if(slot<0||slot>=maxChannels)return;
  editSlot=slot;editChannelName=channels[slot];editChannelKey=haveChannelKey[slot]?hexText(channelKeys[slot],16):String("");
  uint8_t derived[16];channelDerived=management::hashtag(editChannelName.c_str(),editChannelName.length(),derived)&&haveChannelKey[slot]&&!memcmp(derived,channelKeys[slot],16);
  if(!editChannelName.length())channelDerived=true;
  managementInfo="";drawChannelEdit();
}
void drawChannelEdit() {
  screen=Screen::CHANNEL_EDIT;header(("Channel slot "+String(editSlot+1)).c_str());
  smallButton(12,42,296,28,("Name: "+(editChannelName.length()?displayText(editChannelName):String("tap to enter"))).substring(0,32).c_str());
  bool publicKey=editChannelKey=="8b3387e9c5cdea6ac9e5edbaa115cd72";
  smallButton(12,76,180,28,channelDerived?"Type: #channel":publicKey?"Type: Public":"Type: shared key");smallButton(198,76,110,28,"New key");
  lcd.setTextColor(TFT_WHITE,BG);
  lcd.drawString(channelDerived?"Key follows the exact #name you enter.":publicKey?"Public key: everyone can join.":"Shared key (keep private):",12,111,1);
  lcd.drawString(channelDerived?"Anyone knowing the name can join.":editChannelKey,12,127,1);
  if(!channelDerived)smallButton(12,143,110,26,"Enter key");
  smallButton(12,177,80,28,"Back");smallButton(98,177,100,28,"Remove");smallButton(204,177,104,28,"Save");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(managementInfo.substring(0,49),12,215,1);
}
void openContactEditor(bool fresh) {
  if(pending==Pending::CONTACTS)return;
  editContactNew=fresh;managementInfo="";
  if(fresh){editContactName="";editContactKey="";editContactType=1;}
  else {
    if(nodeSelected<0||nodeSelected>=int(contacts.size()))return;
    auto& c=contacts[nodeSelected];editContactName=c.name;editContactKey=hexText(c.key,32);editContactType=c.type;
  }drawContactEdit();
}
void drawContactEdit() {
  screen=Screen::CONTACT_EDIT;header(editContactNew?"Add contact / public key":"Edit contact");
  smallButton(12,42,296,28,("Name: "+(editContactName.length()?displayText(editContactName):String("tap to enter"))).substring(0,32).c_str());
  smallButton(12,76,142,28,("Type: "+String(contactType(editContactType))).c_str());
  bool saved=!editContactNew&&nodeSelected>=0&&nodeSelected<int(contacts.size())&&contacts[nodeSelected].saved;
  smallButton(160,76,148,28,editContactNew?"Enter public key":"Reset route");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Public key:",12,113,1);
  lcd.drawString(editContactKey.substring(0,32),12,130,1);lcd.drawString(editContactKey.substring(32),12,144,1);
  smallButton(12,177,80,28,"Back");if(saved)smallButton(98,177,100,28,"Remove");smallButton(204,177,104,28,"Save");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(managementInfo.substring(0,49),12,215,1);
}
void beginTextEdit(int purpose) {
  textPurpose=purpose;editorReturn=screen;
  if(purpose==0){editText=contactSearch;textLimit=24;}
  else if(purpose==1){editText=editChannelName;textLimit=31;}
  else if(purpose==2){editText=editChannelKey;textLimit=32;}
  else if(purpose==3){editText=editContactName;textLimit=31;}
  else {editText=editContactKey;textLimit=64;}
  upperCase=false;symbols=false;drawTextEditor();
}
void returnEditor() {
  if(editorReturn==Screen::CHANNEL_EDIT)drawChannelEdit();
  else if(editorReturn==Screen::CONTACT_EDIT)drawContactEdit();
  else if(editorReturn==Screen::NODES)drawNodes();else drawDirectPicker();
}
void drawTextEditor() {
  screen=Screen::TEXT_EDIT;
  const char* titles[]={"Find contact (blank clears)","Channel name","Channel key: 32 hex digits","Contact name","Public key: 64 hex digits"};header(titles[textPurpose]);
  wrapped(displayText(editText),12,40,3);
  const char* normal[]={"qwertyuiop","asdfghjkl","zxcvbnm.,?"};
  const char* special[]={"1234567890","!@#$%&*()","-_=+/:;?'"};
  const char* hexKeys[]={"1234567890","abcdef",""};
  bool hex=textPurpose==2||textPurpose==4;
  for(int row=0;row<3;row++) {
    String keys=hex?hexKeys[row]:symbols?special[row]:normal[row];if(!keys.length())continue;if(upperCase&&!hex&&!symbols)keys.toUpperCase();
    int width=296/keys.length();for(size_t col=0;col<keys.length();col++){char label[]={keys[col],0};smallButton(12+col*width,83+row*32,width-2,29,label);}
  }
  if(hex){smallButton(12,180,96,30,"Clear");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("0-9 and a-f",122,190,1);}
  else {smallButton(12,180,46,30,upperCase?"abc":"ABC");smallButton(62,180,46,30,symbols?"abc":"123");smallButton(112,180,100,30,"Space");}
  smallButton(216,180,92,30,"Delete");
  smallButton(12,214,68,23,"Cancel");smallButton(228,214,80,23,"Done");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString((String(editText.length())+"/"+String(textLimit)).c_str(),112,220,1);
}
void showConfirm(ManageAction action) {
  confirmAction=action;confirmReturn=screen;screen=Screen::CONFIRM;
  bool channel=action==ManageAction::CHANNEL_SAVE||action==ManageAction::CHANNEL_DELETE;
  bool removing=action==ManageAction::CHANNEL_DELETE||action==ManageAction::CONTACT_DELETE;
  header(removing?"Confirm removal":"Confirm change");lcd.setTextColor(TFT_WHITE,BG);
  wrapped(channel?displayText(editChannelName):displayText(editContactName),12,45,2);
  String detail;
  if(action==ManageAction::CHANNEL_SAVE)detail="Save this channel on the Heltec? A different key starts a fresh local chat view. Previous history stays in the journal.";
  else if(action==ManageAction::CHANNEL_DELETE)detail="Remove this channel from the Heltec? Previous history stays in the journal.";
  else if(action==ManageAction::CONTACT_DELETE)detail="Remove this contact from the Heltec? Saved messages are kept. A future advert may add it again.";
  else if(action==ManageAction::RESET_PATH)detail="Forget this contact's saved route? Future messages use flooding until a route is learned.";
  else detail="Save these contact details on the Heltec?";
  wrapped(detail,12,82,6);smallButton(12,177,140,32,"Cancel");smallButton(166,177,142,32,removing?"Remove":"Confirm");
}
void restoreConfirmation() {
  if(confirmReturn==Screen::CHANNEL_EDIT)drawChannelEdit();else drawContactEdit();
}
bool prepareChannel(bool remove) {
  uint8_t key[16]={};
  if(!remove) {
    if(!editChannelName.length()||editChannelName.length()>31){managementInfo="Enter a name (1-31 bytes)";drawChannelEdit();return false;}
    if(channelDerived) {
      if(!management::hashtag(editChannelName.c_str(),editChannelName.length(),key)){managementInfo="A #channel name must start with #";drawChannelEdit();return false;}
    } else if(!management::hex(editChannelKey.c_str(),editChannelKey.length(),key,16)||!management::nonzero(key,16)) {
      managementInfo="Enter 32 hex digits, or tap New key";drawChannelEdit();return false;
    }
  }
  if(!management::channel(editSlot,maxChannels,remove?"":editChannelName.c_str(),remove?0:editChannelName.length(),key,managementFrame))return false;
  managementLength=50;return true;
}
bool prepareContact() {
  uint8_t key[32];
  if(!editContactName.length()||editContactName.length()>31){managementInfo="Enter a name (1-31 bytes)";drawContactEdit();return false;}
  if(!management::hex(editContactKey.c_str(),editContactKey.length(),key,32)||!management::nonzero(key,32)) {managementInfo="Enter the full 64 digit public key";drawContactEdit();return false;}
  if(editContactNew) {
    for(auto& c:contacts)if(!memcmp(c.key,key,32)){managementInfo="Already saved; edit the existing contact";drawContactEdit();return false;}
    memset(managementFrame,0,148);managementFrame[35]=255;managementLength=144;
  } else {
    auto it=std::find_if(contacts.begin(),contacts.end(),[&](const diagnostics::Contact& c){return !memcmp(c.key,key,32);});
    if(it==contacts.end()){managementInfo="Contact changed; reload the list";drawContactEdit();return false;}
    if(it->routeStale){managementInfo="Route changed; reload list before editing";drawContactEdit();return false;}
    memcpy(managementFrame,it->frame,it->length);managementLength=min(size_t(it->length),size_t(144));
  }
  managementFrame[0]=9;memcpy(managementFrame+1,key,32);managementFrame[33]=editContactType;
  memset(managementFrame+100,0,32);memcpy(managementFrame+100,editContactName.c_str(),editContactName.length());
  memcpy(managementKey,key,32);return true;
}
void startManagement(ManageAction action) {
  if(!managementIdle()){if(action==ManageAction::FAVOURITE)drawNode();else restoreConfirmation();return;}
  if(action==ManageAction::CHANNEL_SAVE||action==ManageAction::CHANNEL_DELETE) {
    if(!prepareChannel(action==ManageAction::CHANNEL_DELETE))return;
  } else if(action==ManageAction::CONTACT_SAVE) {if(!prepareContact())return;}
  else {
    int index=nodeSelected;
    if(action==ManageAction::CONTACT_DELETE||action==ManageAction::RESET_PATH) {
      uint8_t key[32];index=-1;
      if(management::hex(editContactKey.c_str(),editContactKey.length(),key,32))for(size_t i=0;i<contacts.size();i++)if(!memcmp(contacts[i].key,key,32)){index=i;break;}
    }
    if(index<0||index>=int(contacts.size())){managementInfo="Contact changed; reload the list";drawDirectPicker();return;}
    auto& c=contacts[index];memcpy(managementKey,c.key,32);
    if(action==ManageAction::FAVOURITE&&c.routeStale){managementInfo="Route changed; reload list before editing";drawNode();return;}
    if(action==ManageAction::FAVOURITE) {memcpy(managementFrame,c.frame,c.length);managementFrame[0]=9;managementFrame[34]^=1;managementLength=min(size_t(c.length),size_t(144));}
    else {managementFrame[0]=action==ManageAction::CONTACT_DELETE?15:13;memcpy(managementFrame+1,c.key,32);managementLength=33;}
  }
  manageAction=action;
  if(!sendCommand(managementFrame,managementLength,Pending::MANAGE)){managementResult(false,"Could not write change; try again");return;}
  if(action==ManageAction::FAVOURITE){managementInfo="Saving favourite...";drawNode();}
  else if(action==ManageAction::CHANNEL_SAVE||action==ManageAction::CHANNEL_DELETE){managementInfo="Saving channel...";drawChannelEdit();}
  else {managementInfo="Saving contact...";drawContactEdit();}
}
void managementResult(bool ok,const char* detail) {
  ManageAction action=manageAction;manageAction=ManageAction::NONE;managementInfo=detail;
  if(action==ManageAction::CHANNEL_SAVE||action==ManageAction::CHANNEL_DELETE){if(ok){statusLine=detail;drawChannelSlots();}else drawChannelEdit();}
  else if(action==ManageAction::FAVOURITE||action==ManageAction::RESET_PATH){drawNode();}
  else if(action==ManageAction::CONTACT_DELETE&&ok){nodeSelected=-1;drawDirectPicker();}
  else if(action==ManageAction::CONTACT_SAVE&&ok){editContactNew=false;drawNode();}
  else drawContactEdit();
  Serial.printf("Management: %s\n",detail); // Never log channel keys or form contents.
}
bool handleManagementReply(const Reply& r) {
  if(pending==Pending::MANAGE&&r.code==0) {
    pending=Pending::NONE;
    if(manageAction==ManageAction::CHANNEL_SAVE||manageAction==ManageAction::CHANNEL_DELETE) {
      uint8_t q[]={31,uint8_t(editSlot)};
      if(!sendCommand(q,2,Pending::VERIFY_CHANNEL))managementResult(false,"Saved; read-back failed. Please reload");return true;
    }
    if(manageAction==ManageAction::CONTACT_SAVE||manageAction==ManageAction::FAVOURITE) {
      Reply updated{};updated.code=3;updated.length=managementLength;memcpy(updated.data,managementFrame,managementLength);updated.data[0]=3;upsertContact(updated,false);
      nodeSelected=-1;for(size_t i=0;i<contacts.size();i++)if(!memcmp(contacts[i].key,managementKey,32)){nodeSelected=i;break;}
    } else if(manageAction==ManageAction::CONTACT_DELETE) {
      contacts.erase(std::remove_if(contacts.begin(),contacts.end(),[](const diagnostics::Contact& c){return !memcmp(c.key,managementKey,32);}),contacts.end());
    } else if(manageAction==ManageAction::RESET_PATH) {
      for(auto& c:contacts)if(!memcmp(c.key,managementKey,32)){c.path=255;c.frame[35]=255;}
    }
    managementResult(true,"Saved on Heltec; allow 5s before powering off");return true;
  }
  if(pending==Pending::VERIFY_CHANNEL&&r.code==18&&r.length>=50&&r.data[1]==editSlot) {
    pending=Pending::NONE;
    if(!management::sameChannel(r.data,r.length,managementFrame)){managementResult(false,"Channel read-back differs; reload before retry");return true;}
    bool changed=haveChannelKey[editSlot]&&memcmp(channelKeys[editSlot],r.data+34,16);
    if(changed) {
      uint8_t boundary[]={0xf4,uint8_t(editSlot)};savePacket(boundary,2);
      messages.erase(std::remove_if(messages.begin(),messages.end(),[](const ChatMessage& m){return !m.direct&&m.channel==editSlot;}),messages.end());
      for(auto& watch:sends)if(watch.active&&!watch.direct)watch.active=false;
      if(!composingDirect&&currentChannel==editSlot)draft="";
    }
    channels[editSlot]=packetText(r.data+2,32);memcpy(channelKeys[editSlot],r.data+34,16);haveChannelKey[editSlot]=true;
    managementResult(true,storageBlocked?"Channel saved; history storage error":"Channel saved and read back from Heltec");return true;
  }
  return false;
}
bool managementTouch(int x,int y) {
  bool managementScreen=screen==Screen::TEXT_EDIT||screen==Screen::CONFIRM||screen==Screen::CHANNEL_EDIT||screen==Screen::CHANNEL_SLOTS||screen==Screen::CONTACT_EDIT||screen==Screen::DIRECT_PICK||screen==Screen::NODES||screen==Screen::NODE;
  if(!managementScreen)return false;
  if(pending==Pending::MANAGE||pending==Pending::VERIFY_CHANNEL)return true;
  if(screen==Screen::TEXT_EDIT) {
    bool hex=textPurpose==2||textPurpose==4;
    if(y>=83&&y<179&&x>=12&&x<308) {
      int row=(y-83)/32;const char* normal[]={"qwertyuiop","asdfghjkl","zxcvbnm.,?"};const char* special[]={"1234567890","!@#$%&*()","-_=+/:;?'"};const char* digits[]={"1234567890","abcdef",""};
      String keys=hex?digits[row]:symbols?special[row]:normal[row];if(upperCase&&!hex&&!symbols)keys.toUpperCase();
      if(keys.length()){int width=296/keys.length(),col=(x-12)/width;if(col<int(keys.length())&&(x-12)%width<width-2&&(y-83)%32<29&&editText.length()<textLimit){editText+=keys[col];drawTextEditor();}}
    } else if(y>=180&&y<210) {
      if(x>=12&&x<=58){if(hex)editText="";else upperCase=!upperCase;}
      else if(x>=62&&x<=108){if(hex)editText="";else symbols=!symbols;}
      else if(x>=112&&x<=212){if(!hex&&editText.length()<textLimit)editText+=' ';}
      else if(x>=216&&x<=308&&editText.length()){size_t pos=editText.length()-1;while(pos&&((uint8_t(editText[pos])&0xc0)==0x80))pos--;editText.remove(pos);}
      drawTextEditor();
    } else if(y>=214&&y<=237) {
      if(x>=12&&x<=80)returnEditor();
      else if(x>=228&&x<=308) {
        if(textPurpose==0){contactSearch=editText;contactSearch.trim();nodePage=directPage=0;}
        else if(textPurpose==1)editChannelName=editText;
        else if(textPurpose==2)editChannelKey=editText;
        else if(textPurpose==3)editContactName=editText;
        else editContactKey=editText;
        returnEditor();
      }
    }return true;
  }
  if(screen==Screen::CONFIRM) {
    if(y>=177&&y<=209){if(x>=12&&x<=152)restoreConfirmation();else if(x>=166&&x<=308)startManagement(confirmAction);}return true;
  }
  if(screen==Screen::CHANNEL_SLOTS) {
    if(x>=12&&x<=308&&y>=42&&y<170&&(y-42)%32<28)openChannelEditor(slotPage*4+(y-42)/32);
    else if(y>=177&&y<=205){if(x>=12&&x<=80){slotPage--;drawChannelSlots();}else if(x>=84&&x<=152){slotPage++;drawChannelSlots();}else if(x>=228&&x<=308)drawChannels();}return true;
  }
  if(screen==Screen::CHANNEL_EDIT) {
    if(x>=12&&x<=308&&y>=42&&y<=70)beginTextEdit(1);
    else if(x>=12&&x<=192&&y>=76&&y<=104){channelDerived=!channelDerived;drawChannelEdit();}
    else if(x>=198&&x<=308&&y>=76&&y<=104){uint8_t key[16];esp_fill_random(key,16);editChannelKey=hexText(key,16);channelDerived=false;drawChannelEdit();}
    else if(!channelDerived&&x>=12&&x<=122&&y>=143&&y<=169)beginTextEdit(2);
    else if(y>=177&&y<=205){if(x>=12&&x<=92)drawChannelSlots();else if(x>=98&&x<=198){if(channels[editSlot].length())showConfirm(ManageAction::CHANNEL_DELETE);}else if(x>=204&&x<=308&&prepareChannel(false))showConfirm(ManageAction::CHANNEL_SAVE);}return true;
  }
  if(screen==Screen::CONTACT_EDIT) {
    if(x>=12&&x<=308&&y>=42&&y<=70)beginTextEdit(3);
    else if(x>=12&&x<=154&&y>=76&&y<=104){editContactType=editContactType%4+1;drawContactEdit();}
    else if(x>=160&&x<=308&&y>=76&&y<=104){if(editContactNew)beginTextEdit(4);else showConfirm(ManageAction::RESET_PATH);}
    else if(y>=177&&y<=205){if(x>=12&&x<=92){if(editContactNew)drawDirectPicker();else drawNode();}else if(x>=98&&x<=198&&!editContactNew&&nodeSelected>=0&&nodeSelected<int(contacts.size())&&contacts[nodeSelected].saved)showConfirm(ManageAction::CONTACT_DELETE);else if(x>=204&&x<=308&&prepareContact())showConfirm(ManageAction::CONTACT_SAVE);}return true;
  }
  if(screen==Screen::NODES||screen==Screen::DIRECT_PICK) {
    bool nodes=screen==Screen::NODES;int& page=nodes?nodePage:directPage;
    if(y>=4&&y<=30) {
      if(x>=88&&x<=176){uint8_t& filter=nodes?nodeFilter:contactFilter;filter=(filter+1)%7;page=0;if(nodes)drawNodes();else drawDirectPicker();}
      else if(x>=180&&x<=240)beginTextEdit(0);else if(x>=244&&x<=308)openContactEditor(true);
    } else if(x>=12&&x<=308&&y>=42&&y<170&&(y-42)%32<28) {
      if(pending==Pending::CONTACTS)return true;auto list=filteredContacts(nodes);int item=page*4+(y-42)/32;
      if(item<int(list.size())){nodeSelected=list[item];contactsForDirect=!nodes;managementInfo="";drawNode();}
    } else if(y>=177&&y<=205){if(x>=12&&x<=68){page--;if(nodes)drawNodes();else drawDirectPicker();}else if(x>=72&&x<=128){page++;if(nodes)drawNodes();else drawDirectPicker();}else if(x>=132&&x<=212)requestContacts(!nodes);else if(x>=216&&x<=308){if(nodes)drawNetwork();else drawChannels();}}return true;
  }
  if(screen==Screen::NODE) {
    if(x>=180&&x<=240&&y>=4&&y<=30)startManagement(ManageAction::FAVOURITE);
    else if(x>=244&&x<=308&&y>=4&&y<=30)openContactEditor(false);
    else if(y>=177&&y<=205){if(x>=12&&x<=104){if(nodeSelected>=0&&nodeSelected<int(contacts.size())&&contacts[nodeSelected].type==1)openDirectContact();else requestProbe(false);}else if(x>=110&&x<=202)requestProbe(true);else if(x>=208&&x<=308){if(contactsForDirect)drawDirectPicker();else drawNodes();}}return true;
  }
  return true;
}
bool managementUiSelfTest() {
  // No BLE calls or flash writes. Exercise sorting, selection and command
  // construction using independent sample contacts before loading user data.
  const char* names[]={"Zoe","Alpha relay","Amy"};
  for(int i=0;i<3;i++) {
    Reply sample{};sample.code=3;sample.length=148;sample.data[0]=3;
    sample.data[1]=i+1;sample.data[33]=i==1?2:1;sample.data[34]=i==2?0xa5:0xa4;
    sample.data[35]=1;sample.data[36]=0x42;sample.data[136]=0x55;sample.data[140]=0xaa;
    memcpy(sample.data+100,names[i],strlen(names[i]));upsertContact(sample,false);
  }
  bool ok=true;auto people=filteredContacts(false);
  ok=ok&&people.size()==2&&people[0]==2&&people[1]==0;
  nodeFilter=2;auto repeaters=filteredContacts(true);ok=ok&&repeaters.size()==1&&repeaters[0]==1;
  contactSearch="zO";auto search=filteredContacts(false);ok=ok&&search.size()==1&&search[0]==0;contactSearch="";
  contactFilter=5;auto faves=filteredContacts(false);ok=ok&&faves.size()==1&&faves[0]==2;
  editContactNew=false;editContactName="Renamed";editContactKey=hexText(contacts[0].key,32);editContactType=1;
  ok=ok&&prepareContact()&&managementLength==144&&managementFrame[0]==9&&managementFrame[34]==0xa4
    &&managementFrame[35]==1&&managementFrame[36]==0x42&&managementFrame[136]==0x55&&managementFrame[140]==0xaa
    &&!memcmp(managementFrame+100,"Renamed",7)&&managementFrame[107]==0;
  editContactNew=true;uint8_t fresh[32]={9};editContactKey=hexText(fresh,32);editContactName="New person";
  ok=ok&&prepareContact()&&managementFrame[35]==255&&managementFrame[34]==0&&managementFrame[136]==0&&managementLength==144;
  maxChannels=8;editSlot=1;editChannelName="#test";channelDerived=true;
  ok=ok&&prepareChannel(false)&&managementLength==50&&managementFrame[34]==0x9c&&managementFrame[49]==0x3f;
  remember(1,false,"Old channel",1);remember(2,false,"Other channel",2);uint8_t boundary[]={0xf4,1};decodeHistory(boundary,2);
  ok=ok&&messages.size()==1&&messages[0].channel==2;
  messages.clear();contacts.clear();contactFilter=1;nodeFilter=0;contactSearch="";
  maxChannels=0;editSlot=-1;editContactNew=false;editContactName="";editContactKey="";editChannelName="";editChannelKey="";channelDerived=false;
  managementLength=0;memset(managementFrame,0,sizeof(managementFrame));memset(managementKey,0,sizeof(managementKey));
  return ok;
}
