#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <vector>
#include <deque>
#include <memory>
#include <new>
#include <atomic>
#include <LittleFS.h>
#include "protocol.h"
#include "diagnostics.h"
#include "text_render.h"
#include "clock_tools.h"
#include "send_tracking.h"
#include <time.h>
#include <algorithm>
#include "management_tools.h"
#include "terminal_tools.h"
#include "draft_tools.h"
#include "build_stamp.h"
#include "scroll_tools.h"
#include "discovery_tools.h"
#include "field_tools.h"
#include "antenna_tools.h"
#include "bot_tools.h"
#include "weather_tools.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <sys/time.h>
#include <esp_heap_caps.h>

TFT_eSPI lcd;
SPIClass touchBus(HSPI);
XPT2046_Touchscreen touch(33, 36);
Preferences prefs;
const uint16_t BG = 0x10A3, PANEL = 0x2126, ACCENT = 0x07D9;
const char* SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
// Calibration samples at top left, top right, bottom left; affine transform.
float ax, bx, cx, ay, by, cy;
bool calibrated = false;
bool pressed = false;
enum class Screen { HOME, RADIOS, PIN, LINK, CHANNELS, CHAT, COMPOSE, INBOX, NETWORK, NODES, NODE, CLOCK, DIRECT_PICK, DIRECT_CHAT, CHANNEL_SLOTS, CHANNEL_EDIT, CONTACT_EDIT, TEXT_EDIT, CONFIRM, SETTINGS, DISCOVERY, NEARBY, NEARBY_DETAIL, ADVERT, RADIO_STATS, SENT_RESULTS, MESSAGE_DETAIL, COVERAGE, COVERAGE_NEW, COVERAGE_TEST, COVERAGE_SIGNAL, ANTENNA_LIST, ANTENNA_SETUP, ANTENNA_PICK, ANTENNA_SESSION, ANTENNA_RUN, ANTENNA_RESULTS, ANTENNA_SAMPLES, ANTENNA_HELP, BOT_CHANNELS, BOT_SETUP, BOT_COMMANDS, BOT_COMMAND_EDIT, WIFI_SETUP, WEATHER_SETTINGS, WEATHER_RESULT };
Screen screen = Screen::HOME;
std::vector<NimBLEAdvertisedDevice> radios;
int selected = -1;
String pinText;
std::atomic<uint32_t> pairingPin{0};
std::atomic<bool> allowNewPairing{false};
String activeAddress,activeName;
uint8_t activeAddressType=0;
bool autoConnectEnabled=true,startupReconnect=false;
bool reconnectScheduled=false,reconnectSuppressed=false,resumeAfterReconnect=false;
uint8_t reconnectAttempt=0;
uint32_t reconnectAt=0,reconnectPaintAt=0;
Screen reconnectReturn=Screen::CHANNELS;
bool batteryWanted=true,batterySupported=true,haveBattery=false;
uint16_t batteryMv=0;
uint32_t batteryAt=0,radioStorageUsed=0,radioStorageTotal=0;
uint8_t brightness=255;
bool autoDim=true,displayDimmed=false;
uint32_t lastTouchAt=0;
struct ReadPeer { uint8_t key[6]; uint32_t sequence; };
std::vector<ReadPeer> readPeers;
uint32_t channelRead[40]={},incomingSequence=0,readDirtyAt=0;
bool readDirty=false,readSaveFailed=false;
uint32_t draftSavedHash=0,draftObservedHash=0,draftChangedAt=0;
bool draftDirty=false,draftSaveFailed=false;
void saveDraftNow();
void clearSavedDraft();
bool restoreDraft();
void tickDraft();
bool terminalStorageSelfTest();
void tickTerminal();
void drawSettings();
bool terminalTouch(int x,int y);
void loadReadState();
void saveReadState();
void noteRead(const struct ChatMessage& message);
int channelUnread(uint8_t channel);
int peerUnread(const uint8_t* peer);
int directUnread();
int allUnread();
String idleStatus();
void scheduleReconnect();
void cancelReconnect();
void reconnectReady();
bool handleTerminalReply(const struct Reply& reply);
bool terminalSelfTest();
NimBLEClient* client = nullptr;
NimBLERemoteCharacteristic* commandChar = nullptr;
struct Reply { uint8_t code; uint16_t length; uint8_t data[256]; };
struct RadioPacket {uint16_t length;uint8_t code;};
QueueHandle_t replies;
uint8_t waitingFor = 0;
uint32_t replyDeadline = 0;
bool linkActive = false;
enum class Pending { NONE, SELF, INFO, CLOCK, CHANNEL, SYNC, SEND, CONTACTS, STATS, PROBE, SET_CLOCK, MANAGE, VERIFY_CHANNEL, BATTERY, DISCOVER, ADVERT };
Pending pending=Pending::NONE;
String channels[40];
uint8_t channelKeys[40][16],selfPublicKey[32];
bool haveChannelKey[40]={},haveSelfKey=false;
String wireName;
uint8_t maxChannels=0,channelLoading=0,currentChannel=0;
int channelPage=0;
bool resetChatView=true;
void releaseChatCanvas();
bool messageScreen();
bool handleChatDrag(bool down);
void refreshChatDelivery();
String hexText(const uint8_t* data,size_t length);
String draft,statusLine;
bool composingDirect=false,contactsForDirect=false;
uint8_t directPeer[32]={};
String directName;
int directPage=0;
String feedbackText;
uint16_t feedbackColour=TFT_WHITE;
uint32_t feedbackUntil=0;
struct SendWatch {
  bool active,direct,done,timedOut,accepted;
  bool automatic=false;
  uint32_t started,deadline,timestamp,ack;
  uint16_t payloadLength;
  uint8_t payload[180];
  String target,text;
  uint8_t channel=0,peer[6]{};
};
SendWatch sends[4]={};
int pendingSend=-1,latestSend=-1;
uint32_t lastOutgoingTimestamp=0;
void paintFooter();
void deliveryFeedback(const String& text,uint16_t colour,uint32_t duration=12000);
void tickDelivery();
bool handleDelivery(const Reply& reply);
void drawDirectPicker();
void drawChannelSlots();
void drawChannelEdit();
void drawContactEdit();
void drawTextEditor();
bool managementTouch(int x,int y);
std::vector<int> filteredContacts(bool nodes);
void openContactEditor(bool fresh);
void openChannelEditor(int slot);
void openDirectContact();
void managementResult(bool ok,const char* detail);
bool handleManagementReply(const Reply& r);
void beginTextEdit(int purpose);
String contactSearch;
uint8_t contactFilter=1,nodeFilter=0;
int slotPage=0,editSlot=-1;
String editChannelName,editChannelKey,editContactName,editContactKey,managementInfo;
uint8_t editContactType=1;
bool editContactNew=false,channelDerived=false;
enum class ManageAction { NONE, CHANNEL_SAVE, CHANNEL_DELETE, CONTACT_SAVE, CONTACT_DELETE, FAVOURITE, RESET_PATH };
ManageAction manageAction=ManageAction::NONE,confirmAction=ManageAction::NONE;
uint8_t managementFrame[148]={};
size_t managementLength=0;
uint8_t managementKey[32]={};
Screen editorReturn=Screen::CHANNELS,confirmReturn=Screen::CHANNELS;
String editText;
int textPurpose=0;
size_t textLimit=31;
bool contactsLoaded=false;
bool fixtureMode=false;
void drawBotChannels();
void drawBotSetup();
bool botTouch(int x,int y);
void loadBot();
void resetBotLink();
void botIncoming(const Reply& reply);
void tickBot();
void botQueueDrained();
void saveBotLocation(const String& text);
void saveBotText(const String& text,bool reply);
void drawBotCommands();
void drawBotCommandEdit();
void botStatus();
int activeBotCount();
bool botStorageSelfTest();
void initInternet();
void tickInternet();
void drawWifi();
void drawWeatherSettings();
void stopEditedBot();
bool internetTouch(int x,int y);
void saveWifiText(const String& value,bool password);
bool requestWeather(const String& place,bool forBot);
void cancelBotWeather();
bool botWeatherWaiting();
void internetStatus();
void localWeatherTest();
bool weatherJsonSelfTest();
bool contactsParked=false,contactsRefreshAfterWeather=false;
uint32_t contactRevision=0;
bool parkWeatherContacts();
void restoreWeatherContacts();
bool weatherContactSelfTest();
bool weatherBusyForDiagnostics();
Reply parkedAdverts[4];uint8_t parkedAdvertCount=0;
void replayParkedAdverts();
uint8_t credentialSymbols=0;
void chatPreview();
std::vector<antenna::Session> antennaSessions;
antenna::Session antennaDraft;
uint32_t antennaSelectedId=0,antennaRunningId=0,antennaSaveAt=0,antennaPaintAt=0;
int antennaPage=0,antennaPickPage=0,antennaSample=-1,antennaSamplePhase=0,antennaSamplePage=0;
bool antennaBatch=false,antennaSampleActive=false,antennaDirty=false,antennaFailed=false;
int8_t radioTxPower=0;
String antennaInfo,antennaDataRadio;
void drawAntennaList();
void drawAntennaSetup();
void drawAntennaSession();
void drawAntennaResults();
void tickAntenna();
bool antennaTouch(int x,int y);
void loadAntenna();
void saveAntenna();
void antennaDispatched();
void antennaAccepted();
void antennaReply(const discovery::Result& result);
void antennaFinished(field::TestState state);
void antennaStop(bool cancelled);
bool antennaStorageSelfTest();

std::vector<field::SendResult> sendResults;
std::vector<field::Test> coverageTests;
bool sendResultsDirty=false,sendResultsFailed=false,coverageDirty=false,coverageFailed=false;
bool coverageCapture=false,haveFieldProof=false;
uint32_t coverageActiveId=0,coverageSelectedId=0,fieldProofAt=0,fieldPaintAt=0,fieldSaveAt=0;
uint8_t fieldProofKind=0,fieldScanState=0;
uint32_t fieldScanAt=0;
String fieldProofTarget,coveragePlace,coverageInfo;
int coveragePage=0,coverageReplyPage=0,coverageSignalIndex=-1,sentPage=0;
uint32_t inspectedTimestamp=0;
Screen detailReturn=Screen::CHAT;
void loadFieldData();
void addSendResult(const SendWatch& s);
void updateSendResult(uint32_t timestamp,field::SendState state);
void interruptSendResults();
void tickField();
void tickFieldStorage();
void refreshFieldDetails();
void drawDashboard();
void drawRadioStats();
void drawSentResults();
void drawCoverage();
void drawCoverageNew();
void drawCoverageTest();
void inspectMessage(uint64_t id);
bool fieldTouch(int x,int y);
bool fieldStorageSelfTest();
void noteFieldProof(uint8_t kind,const String& name);
bool queueNearbyScan(bool capture,bool antennaOwner=false);
void coverageDispatched();
void coverageAccepted();
void coverageReply(const discovery::Result& r);
void coverageFinished(field::TestState state);


void drawDirectChat();
void requestContacts(bool forDirect=false);
bool upperCase=false,symbols=false,ready=false,storageReady=false,storageBlocked=false;
uint32_t clockBase=BUILD_UNIX_TIME,clockAt=0,lastPoll=0;
bool clockTrusted=false,clockWriteWanted=false;
String clockSource="Approximate; set clock",clockDigits;
uint32_t clockCheckpointAt=0,clockPaintAt=0;
void drawClock();
void tickClock();
void smallButton(int x,int y,int w,int h,const char* label);
String journalPath;
struct ChatMessage { uint8_t channel; bool direct; String text; uint32_t timestamp; uint8_t peer[6]; bool outgoing; uint32_t sequence; };
std::vector<ChatMessage> messages;
ChatMessage detailMessage{};
bool detailHasMessage=false;
std::deque<diagnostics::Contact> contacts;
std::atomic<bool> replyOverflow{false};
int nodePage=0,nodeSelected=-1;
uint8_t firmwareCode=0,statsStep=0;
bool contactsTruncated=false,haveStats[3]={false,false,false};
uint32_t packetsRx=0,packetsTx=0,rawHeard=0,lastHeardAt=0,statsAt=0,statsBaselineRx=0,statsBaselineTx=0;
bool haveHeard=false,statsBaseline=false;
int lastRssi=0,lastSnrQ=0,noiseFloor=0,txQueue=-1;
String radioProfile,networkResult="No remote test yet",networkInfo;
enum class NetworkAction { NONE, STATS, CONTACTS, STATUS, PATH, DISCOVER, ADVERT };
NetworkAction networkAction=NetworkAction::NONE;
discovery::Scan nearby;
int nearbyPage=0,nearbySelected=-1;
uint32_t lastDiscoveryAt=0,lastAdvertAt=0,discoveryPaintAt=0;
bool haveDiscoveryAttempt=false,haveAdvertAttempt=false;
String discoveryInfo="Tap Scan to check for nearby repeaters.",advertInfo="Sends once; no delivery confirmation.";
void drawRadioTools();
void drawNearby();
void refreshDiscovery();
void tickDiscovery();
bool discoveryTouch(int x,int y);
bool dispatchDiscovery(NetworkAction action);
bool handleDiscoveryReply(const Reply& r);

bool remoteWaiting=false,remotePath=false;
bool haveProbeTarget=false;
uint32_t remoteDeadline=0,remoteStarted=0;
uint8_t remoteKey[32];
void drawNetwork();
void drawNodes();
void drawNode();
void tickNetwork();
bool handleNetworkReply(const Reply& r);
void refreshNetwork();
void drawChannels();
void drawChat();
void drawCompose();
void drawInbox();
void pollMessages();
bool sendCommand(const uint8_t* data,size_t length,Pending next);
void processMessage(const Reply& reply);
void drawRadios();
void drawPin();
void connectRadio(bool automatic=false);
void linkScreen(const char* title, const char* detail);

class ClientCallbacks : public NimBLEClientCallbacks {
  uint32_t onPassKeyRequest() override {
    if(!allowNewPairing.load()) { if(client)client->disconnect();return 0; }
    Serial.println("Radio requested pairing PIN");
    return pairingPin.load();
  }
  bool onConfirmPIN(uint32_t) override { return false; }
  void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
    Serial.printf("Pairing result: encrypted=%d bonded=%d\n",desc->sec_state.encrypted,desc->sec_state.bonded);
  }
};
ClientCallbacks clientCallbacks;

String displayText(const String& input) {
  return String(text_render::ascii(reinterpret_cast<const uint8_t*>(input.c_str()),input.length()).c_str());
}
void header(const char* title) {
  if(!messageScreen())releaseChatCanvas();
  lcd.fillScreen(BG);
  lcd.setTextColor(TFT_WHITE, BG);
  lcd.drawString(displayText(String(title)), 12, 10, 2);
  lcd.drawFastHLine(12, 34, 296, ACCENT);
}
void button(int x, int y, int w, const char* label) {
  lcd.fillRoundRect(x,y,w,40,6,PANEL);
  lcd.setTextColor(ACCENT,PANEL);
  lcd.drawCentreString(label,x+w/2,y+12,2);
}
void home() {
  screen=Screen::HOME;
  header("Mesh Pilot / 0.15.1");
  lcd.drawString("Screen + touch ready",12,48,2);
  lcd.drawString("Heltec stays on MeshCore firmware.",12,72,2);
  lcd.drawString("Turn on your Heltec, then scan.",12,96,2);
  lcd.drawString("Connect for channels and messages.",12,128,2);
  button(12,184,190,"Find MeshCore radios");
  button(212,184,96,"Calibrate");
}
void calibration() {
  float rx[3], ry[3];
  const int px[3]={25,295,25}, py[3]={50,50,215};
  for(int i=0;i<3;i++) {
    header("Touch calibration");
    lcd.drawCentreString("Tap and hold the cross",160,90,2);
    lcd.drawFastHLine(px[i]-10,py[i],21,ACCENT);
    lcd.drawFastVLine(px[i],py[i]-10,21,ACCENT);
    while(touch.touched()) delay(20);
    delay(200);
    while(!touch.touched()) delay(20);
    delay(100);
    long sx=0,sy=0; int n=0;
    for(int k=0;k<15;k++) { if(touch.touched()) { TS_Point p=touch.getPoint(); sx+=p.x;sy+=p.y;n++; } delay(10); }
    if(!n) { i--;continue; }
    rx[i]=float(sx)/n;ry[i]=float(sy)/n;
    Serial.printf("CAL %d raw %.0f %.0f\n",i,rx[i],ry[i]);
    while(touch.touched()) delay(20);
  }
  float ux=rx[1]-rx[0],uy=ry[1]-ry[0],vx=rx[2]-rx[0],vy=ry[2]-ry[0];
  float det=ux*vy-uy*vx;
  if(fabsf(det)<10000) { Serial.println("Calibration invalid; retry");calibration();return; }
  ax=270*vy/det;bx=-270*vx/det;cx=25-ax*rx[0]-bx*ry[0];
  ay=-165*uy/det;by=165*ux/det;cy=50-ay*rx[0]-by*ry[0];
  prefs.putFloat("ax",ax);prefs.putFloat("bx",bx);prefs.putFloat("cx",cx);
  prefs.putFloat("ay",ay);prefs.putFloat("by",by);prefs.putFloat("cy",cy);
  prefs.putBool("cal",true); calibrated=true;
  Serial.println("Touch calibration saved");home();
}
void scan() {
  saveReadState();saveDraftNow();
  cancelReconnect();
  waitingFor=0;pending=Pending::NONE;ready=false; linkActive=false; commandChar=nullptr;
  if(client && client->isConnected()) client->disconnect();
  radios.clear();selected=-1;
  header("Looking for MeshCore radios...");
  lcd.drawString("Scanning Bluetooth for 5 seconds",12,55,2);
  Serial.println("BLE scan starting");
  NimBLEScan* scanner=NimBLEDevice::getScan();
  scanner->setActiveScan(true);
  NimBLEScanResults results=scanner->start(5,false);
  int count=0;
  for(int i=0;i<results.getCount();i++) {
    NimBLEAdvertisedDevice d=results.getDevice(i);
    String name=d.getName().c_str();
    bool match=d.isAdvertisingService(NimBLEUUID(SERVICE)) || name.indexOf("MeshCore")>=0;
    if(!match) continue;
    Serial.printf("MeshCore candidate: %s %s RSSI %d\n",name.c_str(),d.getAddress().toString().c_str(),d.getRSSI());
    if(count<4) radios.push_back(d);
    count++;
  }
  Serial.printf("Scan complete: %d candidate(s); free heap %u\n",count,ESP.getFreeHeap());
  scanner->clearResults();
  drawRadios();
}
void drawRadios() {
  screen=Screen::RADIOS;
  header("Tap a radio to connect");
  for(size_t i=0;i<radios.size();i++) {
    int y=42+i*34;
    lcd.fillRoundRect(12,y,296,30,4,PANEL);
    lcd.setTextColor(ACCENT,PANEL);
    String name=radios[i].getName().c_str();
    if(!name.length()) name="Unnamed MeshCore";
    lcd.drawString(name.substring(0,28),20,y+6,2);
    lcd.drawString(">",290,y+6,2);
  }
  lcd.setTextColor(TFT_WHITE,BG);
  if(radios.empty()) {
    lcd.drawString("No MeshCore radio found.",12,55,2);
    lcd.drawString("Disconnect the phone app and retry.",12,82,2);
    lcd.drawString("Keep the Heltec powered nearby.",12,109,2);
  }
  button(12,184,190,"Scan again");button(212,184,96,"Calibrate");
}
void drawPin() {
  screen=Screen::PIN;
  header("Heltec Bluetooth PIN");
  lcd.drawString("Use the PIN shown by your radio.",12,39,2);
  String dots;for(size_t i=0;i<pinText.length();i++) dots+='*';
  lcd.drawString((dots+"______").substring(0,6),20,61,2);
  const char* keys[12]={"1","2","3","4","5","6","7","8","9","Back","0","Del"};
  for(int i=0;i<12;i++) {
    int x=12+(i%3)*64,y=88+(i/3)*36;
    lcd.fillRoundRect(x,y,58,30,4,PANEL);
    lcd.setTextColor(ACCENT,PANEL);lcd.drawCentreString(keys[i],x+29,y+6,2);
  }
  button(212,112,96,"Connect");
  lcd.setTextColor(TFT_WHITE,BG);
  lcd.drawString("Blank PIN:",210,163,2);
  lcd.drawString("bonded or",210,182,2);
  lcd.drawString("no PIN only",210,201,2);
}
void linkScreen(const char* title, const char* detail) {
  screen=Screen::LINK;
  header(title);
  if(activeName.length())lcd.drawString(displayText(activeName).substring(0,35),12,47,2);
  lcd.drawString(detail,12,80,2);
  lcd.drawString("Keep the phone app disconnected.",12,120,2);
  button(12,184,190,"Back to radios");
  if(prefs.getString("radioAddr","").length())button(212,184,96,"Saved");
}
void failLink(const char* detail,bool retry=true) {
  resetBotLink();
  if(ready){resumeAfterReconnect=true;reconnectReturn=screen;}
  saveReadState();saveDraftNow();
  interruptSendResults();coverageFinished(field::TestState::INTERRUPTED);
  bool unconfirmed=false;for(auto& s:sends){if(s.active&&!s.done)unconfirmed=true;s.active=false;}
  if(unconfirmed)deliveryFeedback("Radio disconnected; delivery unconfirmed",TFT_ORANGE);
  waitingFor=0;pending=Pending::NONE;ready=false;linkActive=false;commandChar=nullptr;
  networkAction=NetworkAction::NONE;remoteWaiting=false;
  if(client && client->isConnected()) client->disconnect();
  Serial.printf("Connection failed: %s\n",detail);
  linkScreen("Radio disconnected",detail);
  if(retry)scheduleReconnect();else cancelReconnect();
}
void clearReplies(){if(!replies)return;RadioPacket* packet=nullptr;while(xQueueReceive(replies,&packet,0)==pdTRUE)free(packet);}
bool popReply(Reply& reply){RadioPacket* packet=nullptr;if(!replies||xQueueReceive(replies,&packet,0)!=pdTRUE)return false;reply=Reply{};reply.code=packet->code;reply.length=packet->length;memcpy(reply.data,reinterpret_cast<uint8_t*>(packet+1),packet->length);free(packet);return true;}
void notifyRadio(NimBLERemoteCharacteristic*,uint8_t* data,size_t length,bool) {
  if(!length||length>256||!replies)return;
  bool critical=data[0]<0x80||data[0]==0x82||data[0]==0x87||data[0]==0x8d||data[0]==0x8e;
  RadioPacket* packet=static_cast<RadioPacket*>(malloc(sizeof(RadioPacket)+length));
  if(!packet){if(critical)replyOverflow.store(true);return;}
  packet->code=data[0];packet->length=length;memcpy(reinterpret_cast<uint8_t*>(packet+1),data,length);
  if(xQueueSend(replies,&packet,0)!=pdTRUE){free(packet);if(critical)replyOverflow.store(true);}
}
bool replyQueueSelfTest(){
  QueueHandle_t original=replies;bool overflow=replyOverflow.load();size_t before=ESP.getFreeHeap();replies=xQueueCreate(2,sizeof(RadioPacket*));if(!replies){replies=original;return false;}
  bool ok=true;uint8_t a[256],b[256];Reply got{};
  for(int cycle=0;cycle<64;cycle++){
    for(int i=0;i<256;i++){a[i]=i^cycle;b[i]=(255-i)^cycle;}a[0]=b[0]=17;int n=1+cycle*4;
    notifyRadio(nullptr,a,n,false);notifyRadio(nullptr,b,256,false);notifyRadio(nullptr,a,n,false);ok=ok&&replyOverflow.exchange(false);
    ok=popReply(got)&&got.length==n&&!memcmp(got.data,a,n)&&ok;ok=popReply(got)&&got.length==256&&!memcmp(got.data,b,256)&&ok;
    ok=ok&&!popReply(got);
    notifyRadio(nullptr,a,n,false);notifyRadio(nullptr,b,256,false);clearReplies();ok=ok&&!popReply(got);
  }
  clearReplies();vQueueDelete(replies);replies=original;replyOverflow.store(overflow);return ok&&ESP.getFreeHeap()==before;
}
void connectRadio(bool automatic) {
  resetBotLink();
  reconnectScheduled=false;if(!automatic){reconnectSuppressed=false;reconnectAttempt=0;batterySupported=true;}
  if(!automatic&&pinText.length()!=0 && pinText.length()!=6) {
    lcd.setTextColor(TFT_RED,BG);lcd.drawString("Enter all six digits",110,61,2);return;
  }
  if(automatic) {
    activeAddress=prefs.getString("radioAddr","");activeName=prefs.getString("radioName","Saved MeshCore radio");activeAddressType=prefs.getUChar("radioType",0);
    if(!activeAddress.length()) { home();return; }
    pinText="";
    if(!NimBLEDevice::isBonded(NimBLEAddress(std::string(activeAddress.c_str()),activeAddressType))) {
      failLink("Pairing needed. Scan and enter PIN.",false);return;
    }
  } else {
    if(selected<0||selected>=int(radios.size()))return;
    String chosen=radios[selected].getAddress().toString().c_str();
    if(activeAddress.length()&&activeAddress!=chosen){saveDraftNow();draft="";draftSavedHash=draftObservedHash=0;draftDirty=false;}
    activeAddress=radios[selected].getAddress().toString().c_str();activeAddressType=radios[selected].getAddress().getType();activeName=radios[selected].getName().c_str();
  }
  allowNewPairing.store(!automatic);
  pairingPin.store(pinText.toInt());
  NimBLEDevice::setSecurityIOCap(pinText.length()?BLE_HS_IO_KEYBOARD_ONLY:BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityAuth(true,pinText.length()!=0,true);
  pinText="";
  linkScreen("Connecting...","Please wait for the radio.");
  clearReplies();
  antennaBatch=false;antennaSampleActive=false;antennaRunningId=0;
  ready=false;pending=Pending::NONE;haveBattery=false;batteryWanted=true;
  memset(channelKeys,0,sizeof(channelKeys));memset(haveChannelKey,0,sizeof(haveChannelKey));haveSelfKey=false;wireName="";
  for(auto& s:sends)s.active=false;pendingSend=-1;latestSend=-1;
  contactRevision++;contacts.clear();contactsLoaded=false;nodeSelected=-1;nodePage=0;contactsTruncated=false;contactSearch="";manageAction=ManageAction::NONE;
  fieldScanState=0;haveFieldProof=false;fieldProofTarget="";coverageCapture=false;coverageActiveId=0;
  nearby.active=false;nearby.count=0;discoveryInfo="Tap Scan to check for nearby repeaters.";
  networkAction=NetworkAction::NONE;remoteWaiting=false;haveProbeTarget=false;networkResult="No remote test yet";
  haveStats[0]=haveStats[1]=haveStats[2]=false;statsBaseline=false;rawHeard=0;haveHeard=false;replyOverflow.store(false);
  bool connected=false;
  for(int attempt=0;attempt<2;attempt++) {
    if(attempt) { linkScreen("Retrying connection...","One more attempt. Please wait.");delay(800); }
    if(client->connect(NimBLEAddress(std::string(activeAddress.c_str()),activeAddressType))) { connected=true;break; }
    Serial.printf("BLE connection attempt %d failed: %d\n",attempt+1,client->getLastError());
    if(client->isConnected()) { client->disconnect();delay(500); }
  }
  if(!connected) { failLink("Radio unavailable. Retry scan.");return; }
  Serial.println("BLE link connected; securing");
  if(!client->secureConnection()) { failLink(automatic?"Saved pairing failed. Scan for PIN.":"Pairing failed. Check the PIN.",false);return; }
  allowNewPairing.store(false);
  auto service=client->getService(SERVICE);
  if(!service) { failLink("MeshCore service not found.",false);return; }
  commandChar=service->getCharacteristic("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
  auto notifyChar=service->getCharacteristic("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
  if(!commandChar || (!commandChar->canWrite() && !commandChar->canWriteNoResponse()) || !notifyChar || !notifyChar->canNotify()) {
    failLink("Radio service is incompatible.",false);return;
  }
  if(!notifyChar->subscribe(true,notifyRadio,true)) { failLink("Cannot subscribe to radio.");return; }
  // Start companion session before all other commands.
  uint8_t start[]={0x01,0,0,0,0,0,0,0,'C','Y','D'};
  linkActive=true;
  if(!sendCommand(start,sizeof(start),Pending::SELF)) { failLink("Radio handshake write failed.");return; }
  linkScreen("Bluetooth connected","Checking MeshCore response...");
}
uint32_t nowSeconds() { return clockBase+(millis()-clockAt)/1000; }
String clockText() {
  time_t value=nowSeconds();struct tm utc;gmtime_r(&value,&utc);char text[32];strftime(text,sizeof(text),"%Y-%m-%d %H:%M UTC",&utc);return String(text);
}
void setClock(uint32_t value,const char* source) {
  clockBase=value;clockAt=millis();clockTrusted=true;clockSource=source;
  prefs.putULong("timeLast",value);clockCheckpointAt=millis();clockWriteWanted=true;
  Serial.printf("Clock set: %s (%s)\n",clockText().c_str(),source);
  if(screen==Screen::CLOCK)drawClock();
}
void drawClock() {
  screen=Screen::CLOCK;header("Clock / UTC");
  lcd.drawString(clockText(),12,41,2);lcd.drawString(clockSource.substring(0,45),12,66,1);
  lcd.drawString("Enter YYYYMMDDhhmm (UTC):",12,82,1);
  lcd.drawString((clockDigits+"____________").substring(0,12),12,97,2);
  const char* keys[]={"1","2","3","4","5","6","7","8","9","Clear","0","Del"};
  for(int i=0;i<12;i++)smallButton(12+(i%3)*64,122+(i/3)*28,58,25,keys[i]);
  smallButton(212,130,96,36,"Set time");smallButton(212,180,96,36,"Back");
  clockPaintAt=millis();
}
void tickClock() {
  static char command[40];static size_t used=0;
  while(Serial.available()) {
    char c=Serial.read();
    if(c=='\n') {
      command[used]=0;
      if(used>5&&strncmp(command,"TIME ",5)==0) {
        char* end=nullptr;unsigned long value=strtoul(command+5,&end,10);
        if(end&&!*end&&value>=1577836800u&&value<=4102444799u)setClock(value,"USB synchronised");
      } else if(strcmp(command,"LINKTEST")==0&&ready){Serial.println("Diagnostic local BLE disconnect; no RF send");client->disconnect();}
      else if(strcmp(command,"STATUS?")==0)Serial.printf("Terminal status: ready=%d; battery=%u mV; contacts=%u; unread=%d; retry=%d; draft=%u; dimmed=%d\n",ready,batteryMv,unsigned(contacts.size()),allUnread(),reconnectScheduled,unsigned(draft.length()),displayDimmed);
      else if(strcmp(command,"STORAGETEST")==0&&pending!=Pending::SEND&&pending!=Pending::MANAGE&&pending!=Pending::VERIFY_CHANNEL&&!antennaBatch&&!antennaSampleActive)Serial.printf("Isolated read-state and draft storage fixtures: %s\n",terminalStorageSelfTest()?"passed":"FAILED");
      else if(strcmp(command,"FIELDTEST")==0&&pending!=Pending::SEND&&pending!=Pending::MANAGE&&pending!=Pending::VERIFY_CHANNEL&&!coverageCapture&&!antennaBatch&&!antennaSampleActive)Serial.printf("Isolated delivery and coverage storage fixtures: %s\n",fieldStorageSelfTest()?"passed":"FAILED");
      else if(strcmp(command,"ANTTEST")==0&&pending!=Pending::SEND&&pending!=Pending::MANAGE&&pending!=Pending::VERIFY_CHANNEL&&!antennaBatch&&!antennaSampleActive)Serial.printf("Isolated antenna storage fixtures: %s\n",antennaStorageSelfTest()?"passed":"FAILED");
      else if(strcmp(command,"NET?")==0)internetStatus();
      else if(strcmp(command,"WXSTORETEST")==0&&pending==Pending::NONE&&!weatherBusyForDiagnostics())Serial.println(weatherContactSelfTest()?"Weather contact fixtures passed; no RF send":"Weather contact fixtures FAILED");
      else if(strcmp(command,"WXLOCAL")==0) localWeatherTest();
      else if(strcmp(command,"BOT?")==0)botStatus();
      else if(strcmp(command,"BOTTEST")==0&&pending==Pending::NONE&&!antennaBatch)Serial.println(botStorageSelfTest()?"Bot storage fixtures passed; no RF send":"Bot storage fixtures FAILED");
      else if(strcmp(command,"CHATPREVIEW")==0&&pending==Pending::NONE&&!antennaBatch)chatPreview();
      else if(strcmp(command,"ANT?")==0)Serial.printf("Antenna status: sessions=%u; batch=%d; sample=%d; storeFailed=%d\n",unsigned(antennaSessions.size()),antennaBatch,antennaSampleActive,antennaFailed);
      else if(strcmp(command,"FIELD?")==0)Serial.printf("Field status: results=%u; tests=%u; capture=%d; resultStoreFailed=%d; coverageStoreFailed=%d; proof=%d\n",unsigned(sendResults.size()),unsigned(coverageTests.size()),coverageCapture,sendResultsFailed,coverageFailed,haveFieldProof);
      else if(strcmp(command,"CLOCK?")==0)Serial.printf("Clock now: %s (%s)\n",clockText().c_str(),clockSource.c_str());
      used=0;
    } else if(c!='\r') {if(used<sizeof(command)-1)command[used++]=c;else used=0;}
  }
  if(millis()-clockCheckpointAt>=60000){uint32_t current=nowSeconds();clockBase=current;clockAt=millis();prefs.putULong("timeLast",current);clockCheckpointAt=millis();}
  if(clockWriteWanted&&ready&&pending==Pending::NONE) {
    uint8_t q[5]={6};protocol::write32(q+1,nowSeconds());clockWriteWanted=false;
    if(!sendCommand(q,5,Pending::SET_CLOCK))statusLine="Local time set; radio update failed";
  }
  if(screen==Screen::CLOCK&&millis()-clockPaintAt>=1000) {
    lcd.fillRect(12,41,296,23,BG);lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(clockText(),12,41,2);clockPaintAt=millis();
  }
}
#include "field_store.h"
#include "antenna_store.h"

String packetText(const uint8_t* p,size_t n) {
  return String(text_render::ascii(p,n).c_str());
}
void remember(uint8_t ch,bool dm,const String& text,uint32_t timestamp,const uint8_t* peer=nullptr,bool outgoing=false) {
  if(messages.size()>=80) messages.erase(messages.begin());
  messages.push_back({ch,dm,text,timestamp,{},outgoing,outgoing?0:++incomingSequence});
  if(peer)memcpy(messages.back().peer,peer,6);
}
void decodeHistory(const uint8_t* p,size_t n) {
  if(n==2&&p[0]==0xf4) {
    messages.erase(std::remove_if(messages.begin(),messages.end(),[&](const ChatMessage& m){return !m.direct&&m.channel==p[1];}),messages.end());return;
  }
  if((n>=6&&(p[0]==0xf0||p[0]==0xf1))||(n>=11&&(p[0]==0xf2||p[0]==0xf3)))lastOutgoingTimestamp=max(lastOutgoingTimestamp,protocol::read32(p+(p[0]>=0xf2?7:2)));
  protocol::MessageView v;
  if(protocol::message(p,n,v)) remember(v.channel,v.direct,packetText(p+v.textOffset,n-v.textOffset),v.timestamp,v.direct?p+((p[0]==16)?4:1):nullptr);
  else if(n>=6 && (p[0]==0xf0||p[0]==0xf1))
    remember(p[1],false,String(p[0]==0xf0?"Me: ":"Me (unconfirmed): ")+packetText(p+6,n-6),protocol::read32(p+2),nullptr,true);
  else if(n>=11&&(p[0]==0xf2||p[0]==0xf3))
    remember(255,true,String(p[0]==0xf2?"Me: ":"Me (unconfirmed): ")+packetText(p+11,n-11),protocol::read32(p+7),p+1,true);
}
bool savePacket(const uint8_t* p,size_t n) {
  if(!storageReady || storageBlocked || n>256) return false;
  File f=LittleFS.open(journalPath,"a");
  if(!f || f.size()+n+2>524288) { if(f)f.close();storageBlocked=true;return false; }
  uint8_t length[2]={uint8_t(n),uint8_t(n>>8)};
  bool ok=f.write(length,2)==2 && f.write(p,n)==n;
  f.flush();f.close();
  if(!ok) storageBlocked=true;
  return ok;
}
void loadHistory() {
  messages.clear();incomingSequence=0;storageBlocked=false;
  String id=activeAddress;id.replace(":","");
  journalPath="/"+id+".log";loadReadState();loadFieldData();loadAntenna();loadBot();
  if(!storageReady) { storageBlocked=true;return; }
  File f=LittleFS.open(journalPath,"r");
  if(!f)return;
  while(f.available()) {
    uint8_t len[2],data[256];
    if(f.read(len,2)!=2) { storageBlocked=true;break; }
    size_t n=len[0]|(len[1]<<8);
    if(!n||n>256||f.read(data,n)!=n) { storageBlocked=true;break; }
    decodeHistory(data,n);
  }
  f.close();
}
bool sendCommand(const uint8_t* data,size_t length,Pending next) {
  if(!linkActive||!commandChar||pending!=Pending::NONE) return false;
  pending=next;replyDeadline=millis()+8000;
  if(!commandChar->writeValue(data,length,commandChar->canWrite())) { pending=Pending::NONE;return false; }
  return true;
}
void fetchChannel() {
  if(channelLoading>=maxChannels) {
    ready=true;statusLine=storageBlocked?"Storage unavailable; reception paused":"Connected";
    reconnectReady();lastPoll=millis()-3000;requestContacts();return;
  }
  uint8_t q[]={0x1f,channelLoading};
  if(!sendCommand(q,2,Pending::CHANNEL)) failLink("Channel request failed.");
}
void smallButton(int x,int y,int w,int h,const char* label) {
  lcd.fillRoundRect(x,y,w,h,4,PANEL);lcd.setTextColor(ACCENT,PANEL);
  lcd.drawCentreString(label,x+w/2,y+(h-16)/2,2);
}
void paintFooter() {
  bool compose=screen==Screen::COMPOSE;
  if(!compose&&screen!=Screen::CHAT&&screen!=Screen::DIRECT_CHAT&&screen!=Screen::INBOX&&screen!=Screen::CHANNELS)return;
  int x=compose?86:12,y=compose?214:212,width=compose?138:296;
  lcd.fillRect(x,y,width,compose?23:18,BG);lcd.setTextSize(1);lcd.setTextDatum(TL_DATUM);
  bool feedback=feedbackText.length()&&int32_t(feedbackUntil-millis())>0&&!storageBlocked;
  lcd.setTextColor(feedback?feedbackColour:storageBlocked?TFT_ORANGE:TFT_WHITE,BG);
  String normal=(statusLine=="Connected"||statusLine=="New message saved"||statusLine=="")?idleStatus():statusLine;
  String line=feedback?feedbackText:storageBlocked?String("History storage unavailable; reception paused"):normal;
  lcd.drawString(line.substring(0,width/6),x,compose?220:215,1);
}
void deliveryFeedback(const String& text,uint16_t colour,uint32_t duration) {
  feedbackText=text;feedbackColour=colour;feedbackUntil=millis()+duration;refreshChatDelivery();paintFooter();
  Serial.printf("Send feedback: %s\n",text.c_str());
}
int createSendWatch(uint32_t timestamp,bool direct,uint8_t channel,const uint8_t* peer,const String& target,const String& text,bool automatic=false) {
  int slot=-1;
  for(int i=0;i<4;i++)if(!sends[i].active){slot=i;break;}
  if(slot<0)for(int i=0;i<4;i++)if(sends[i].done||sends[i].timedOut){slot=i;break;}
  if(slot<0&&automatic){
    uint32_t oldest=0;for(int i=0;i<4;i++)if(sends[i].automatic){uint32_t age=millis()-sends[i].started;if(slot<0||age>=oldest){slot=i;oldest=age;}}
    if(slot>=0)updateSendResult(sends[slot].timestamp,sends[slot].accepted?field::SendState::TRACKING_UNAVAILABLE:field::SendState::UNCONFIRMED);
  }
  if(slot<0)return -1;
  sends[slot]=SendWatch{};auto& s=sends[slot];s.active=true;s.direct=direct;s.automatic=automatic;s.started=millis();s.deadline=millis()+(direct?120000:45000);s.timestamp=timestamp;
  s.target=displayText(target);s.text=text;s.channel=channel;if(direct&&peer)memcpy(s.peer,peer,6);
  addSendResult(s);
  if(direct&&haveSelfKey)s.ack=send_tracking::directAck(selfPublicKey,text.c_str(),timestamp);
  else if(!direct&&haveChannelKey[channel]&&wireName.length())s.payloadLength=send_tracking::channelPayload(channelKeys[channel],wireName.c_str(),text.c_str(),timestamp,s.payload);
  latestSend=slot;return slot;
}
int newSendWatch(uint32_t timestamp){return createSendWatch(timestamp,composingDirect,currentChannel,directPeer,composingDirect?directName:channels[currentChannel],draft);}

void tickDelivery() {
  if(feedbackText.length()&&int32_t(millis()-feedbackUntil)>=0){feedbackText="";paintFooter();}
  for(auto& s:sends) {
    if(!s.active)continue;
    if(!s.done&&!s.timedOut&&int32_t(millis()-s.deadline)>=0) {
      s.timedOut=true;updateSendResult(s.timestamp,s.direct?field::SendState::NO_ACK:field::SendState::NO_REPEAT);
      deliveryFeedback(s.direct?"No ACK: "+s.target.substring(0,15)+"; unconfirmed":"No repeat heard: "+s.target.substring(0,12)+"; unconfirmed",TFT_ORANGE);
    }
    if(millis()-s.started>180000)s.active=false;
  }
}
bool handleDelivery(const Reply& r) {
  if(r.code==0x82&&r.length>=9) {
    uint32_t ack=protocol::read32(r.data+1);
    for(auto& s:sends)if(s.active&&s.direct&&!s.done&&s.ack&&s.ack==ack) {
      s.done=true;updateSendResult(s.timestamp,field::SendState::ACK);noteFieldProof(3,s.target);deliveryFeedback("Delivered: "+s.target.substring(0,28),TFT_GREEN);break;
    }
    return true;
  }
  if(r.code==0x88&&r.length>=5)for(auto& s:sends)if(s.active&&!s.direct&&!s.done&&s.payloadLength) {
    uint8_t last[3],width;
    if(send_tracking::echoed(r.data+3,r.length-3,s.payload,s.payloadLength,last,width)) {
      s.done=true;updateSendResult(s.timestamp,field::SendState::REPEAT);deliveryFeedback("Repeat heard: "+s.target.substring(0,12)+"; not delivery",TFT_GREEN);
    }
  }
  return false;
}
std::vector<int> activeChannels() {
  std::vector<int> a;for(int i=0;i<maxChannels;i++)if(channels[i].length())a.push_back(i);return a;
}
void drawChannels() {
  screen=Screen::CHANNELS;header("Mesh Pilot");
  smallButton(108,4,60,26,"Setup");
  smallButton(174,4,78,26,"Manage");
  smallButton(258,4,50,26,"Net");
  auto list=activeChannels();int pages=max(1,int((list.size()+3)/4));channelPage=constrain(channelPage,0,pages-1);
  for(int row=0;row<4;row++) {
    int item=channelPage*4+row;if(item>=int(list.size()))break;
    int unread=channelUnread(list[item]);String label=channels[list[item]].substring(0,unread?26:32);if(unread)label+=" ("+String(unread)+")";
    smallButton(12,42+row*32,296,28,label.c_str());
  }
  if(list.empty()) { lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("No configured channels on radio.",12,55,2); }
  smallButton(12,177,68,27,"Prev");smallButton(84,177,68,27,"Next");
  smallButton(156,177,68,27,directUnread()?("DM "+String(directUnread())).c_str():"Inbox");smallButton(228,177,80,27,"Contacts");
  paintFooter();
}
void wrapped(const String& text,int x,int y,int maxLines) {
  // Fixed-width font makes a dependable layout on the small resistive screen.
  lcd.setTextFont(1);lcd.setTextSize(1);lcd.setTextColor(TFT_WHITE,BG);
  int pos=0;for(int line=0;line<maxLines && pos<int(text.length());line++) {
    lcd.drawString(text.substring(pos,pos+48),x,y+line*11,1);pos+=48;
  }
  if(pos<int(text.length())) lcd.drawString("...",x+282,y+(maxLines-1)*11,1);
}
std::vector<int> visibleMessages(bool inbox) {
  std::vector<int> out;for(size_t i=0;i<messages.size();i++)
    if(inbox?(messages[i].direct&&(screen!=Screen::DIRECT_CHAT||memcmp(messages[i].peer,directPeer,6)==0)):(!messages[i].direct&&messages[i].channel==currentChannel))out.push_back(i);
  return out;
}
struct ChatFlow { String text; int userEnd; };
ChatFlow chatFlow(const ChatMessage& m) {
  uint32_t tod=m.timestamp%86400;
  char stamp[8];snprintf(stamp,sizeof(stamp),"%02u:%02u",unsigned(tod/3600),unsigned((tod%3600)/60));
  String username="Unknown",body=m.text;
  int separator=body.indexOf(": ");
  if(m.direct&&!m.outgoing) {
    for(auto& c:contacts)if(memcmp(c.key,m.peer,6)==0){username=displayText(String(c.name));break;}
    if(username=="Unknown"){char id[20];snprintf(id,sizeof(id),"Node %02X%02X%02X",m.peer[0],m.peer[1],m.peer[2]);username=id;}
  } else if(separator>0&&separator<=64){username=body.substring(0,separator);body=body.substring(separator+2);}
  String prefix=String(stamp)+" "+username+": ";
  return {prefix+body,int(prefix.length())};
}
#include "scroll_ui.h"

void drawChat() { screen=Screen::CHAT;header(channels[currentChannel].substring(0,30).c_str());lcd.setTextColor(0x5C7F,BG);lcd.drawString("UTC",284,16,1);historyBody(false); }
void drawInbox() { screen=Screen::INBOX;header("Direct messages");smallButton(230,4,48,26,"To");lcd.setTextColor(0x5C7F,BG);lcd.drawString("UTC",284,16,1);historyBody(true); }
void drawDirectChat() {screen=Screen::DIRECT_CHAT;header(displayText(directName).substring(0,30).c_str());lcd.setTextColor(0x5C7F,BG);lcd.drawString("UTC",284,16,1);historyBody(true);}
std::vector<int> directContacts() { return filteredContacts(false); }
void drawContactList(bool nodes) {
  screen=nodes?Screen::NODES:Screen::DIRECT_PICK;header(nodes?"Nodes":"Contacts");
  const char* filters[]={"All","People","Repeaters","Rooms","Sensors","Faves","Heard"};
  uint8_t filter=nodes?nodeFilter:contactFilter;
  smallButton(88,4,88,26,filters[filter]);smallButton(180,4,60,26,"Find");smallButton(244,4,64,26,"Add");
  auto list=filteredContacts(nodes);int& page=nodes?nodePage:directPage;
  page=constrain(page,0,max(0,int((list.size()+3)/4)-1));
  for(int row=0;row<4;row++) {
    int item=page*4+row;if(item>=int(list.size()))break;auto& c=contacts[list[item]];
    String label=String(!c.saved?"New: ":(c.flags&1)?"* ":"")+displayText(String(c.name));
    int unread=peerUnread(c.key);if(unread)label=label.substring(0,23)+" ("+String(unread)+")";
    smallButton(12,42+row*32,296,28,label.substring(0,29).c_str());
    lcd.setTextColor(c.seen?TFT_GREEN:0x5C7F,PANEL);lcd.drawString(c.type==1?"P":c.type==2?"R":c.type==3?"G":c.type==4?"S":"?",295,50+row*32,1);
  }
  if(list.empty()){lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(contactsParked?"Weather lookup: contacts return shortly.":contactsLoaded?"No matches. Change filter or search.":"Loading the Heltec's saved contacts...",12,60,1);}
  smallButton(12,177,56,28,"Prev");smallButton(72,177,56,28,"Next");smallButton(132,177,80,28,"Load");smallButton(216,177,92,28,"Back");
  lcd.setTextColor(TFT_WHITE,BG);
  String info=String(list.size())+" shown / "+String(contacts.size())+" known; "+String(page+1)+"/"+String(max(1,int((list.size()+3)/4)));
  if(contactSearch.length())info="Find: "+contactSearch.substring(0,15)+"; "+info;
  if(pending==Pending::CONTACTS)info="Loading contacts...";
  else if(contactsTruncated)info="First 256 contacts only; "+info;
  lcd.drawString(info.substring(0,49),12,215,1);
}
void drawDirectPicker() { drawContactList(false); }
void drawCompose() {
  screen=Screen::COMPOSE;header(("To: "+displayText(composingDirect?directName:channels[currentChannel])).substring(0,36).c_str());
  wrapped(draft.length()>96?draft.substring(draft.length()-96):draft,12,40,2);
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString((String(draft.length())+"/120").c_str(),260,67,1);
  const char* rowsNormal[]={"qwertyuiop","asdfghjkl","zxcvbnm.,?"};
  const char* rowsSymbols[]={"1234567890","!@#$%&*()","-_=+/:;?'"};
  for(int row=0;row<3;row++) {
    String keys=symbols?rowsSymbols[row]:rowsNormal[row];if(upperCase&&!symbols)keys.toUpperCase();
    int width=296/keys.length();
    for(size_t col=0;col<keys.length();col++) { char label[]={keys[col],0};smallButton(12+col*width,83+row*32,width-2,29,label); }
  }
  smallButton(12,180,46,30,upperCase?"abc":"ABC");smallButton(62,180,46,30,symbols?"abc":"123");
  smallButton(112,180,100,30,"Space");smallButton(216,180,92,30,"Delete");
  smallButton(12,214,68,23,"Back");smallButton(228,214,80,23,"Send");
  paintFooter();
}
void refreshMessageScreen() {
  if(screen==Screen::CHAT)drawChat();else if(screen==Screen::DIRECT_CHAT)drawDirectChat();else if(screen==Screen::CHANNELS)drawChannels();else if(screen==Screen::INBOX)drawInbox();
  // Incoming traffic must not disturb the keyboard or draft.
}
void pollMessages() {
  if(!ready||pending!=Pending::NONE||storageBlocked||millis()-lastPoll<3000)return;
  lastPoll=millis();uint8_t q=0x0a;
  if(!sendCommand(&q,1,Pending::SYNC))failLink("Message request failed.");
}
void processMessage(const Reply& reply) {
  // The radio dequeues a message on SYNC. Persist each full frame before asking
  // for another, including unknown/datagram types so they are not discarded.
  bool saved=savePacket(reply.data,reply.length);
  decodeHistory(reply.data,reply.length);
  statusLine=saved?"New message saved":"Storage error; reception paused";
  if(saved)botIncoming(reply);
  noteTimelineIncoming(reply);
  refreshMessageScreen();if(screen==Screen::COMPOSE)paintFooter();if(screen==Screen::DIRECT_PICK||screen==Screen::NODES)refreshNetwork();
}
void recordOutgoing(bool confirmed) {
  if(pendingSend<0)return;const auto& s=sends[pendingSend];
  uint8_t data[131];size_t offset;
  if(s.direct){data[0]=confirmed?0xf2:0xf3;memcpy(data+1,s.peer,6);protocol::write32(data+7,s.timestamp);offset=11;}
  else {data[0]=confirmed?0xf0:0xf1;data[1]=s.channel;protocol::write32(data+2,s.timestamp);offset=6;}
  memcpy(data+offset,s.text.c_str(),s.text.length());
  if(!savePacket(data,s.text.length()+offset))statusLine="Storage error; reception paused";
  decodeHistory(data,s.text.length()+offset);
}

void sendDraft() {
  if(!draft.length())return;
  if(pending!=Pending::NONE) { statusLine="Radio busy; tap Send again";drawCompose();return; }
  if(!ready||!client->isConnected()) { statusLine="Disconnected; draft kept";drawCompose();return; }
  uint32_t stamp=max(nowSeconds(),lastOutgoingTimestamp+1);
  pendingSend=newSendWatch(stamp);
  if(pendingSend<0){deliveryFeedback("Waiting for earlier sends; try shortly",TFT_ORANGE);return;}
  lastOutgoingTimestamp=stamp;
  uint8_t packet[133]={uint8_t(composingDirect?0x02:0x03),0,uint8_t(composingDirect?0:currentChannel)};
  protocol::write32(packet+3,stamp);size_t offset=composingDirect?13:7;
  if(composingDirect)memcpy(packet+7,directPeer,6);
  memcpy(packet+offset,draft.c_str(),draft.length());
  statusLine="Sending...";drawCompose();
  deliveryFeedback("Sending to radio...",0x5C7F);
  if(!sendCommand(packet,draft.length()+offset,Pending::SEND)) {
    statusLine="Not confirmed; check before retry";recordOutgoing(false);sends[pendingSend].timedOut=true;updateSendResult(sends[pendingSend].timestamp,field::SendState::UNCONFIRMED);drawCompose();
    deliveryFeedback("Radio write unconfirmed; check before retry",TFT_ORANGE);
  }
}
const char* contactType(uint8_t type) {
  return type==2?"Repeater":type==3?"Room":type==1?"Person":type==4?"Sensor":"Other";
}
void refreshNetwork() {
  refreshDiscovery();
  if(screen==Screen::NETWORK)drawNetwork();else if(screen==Screen::RADIO_STATS)drawRadioStats();else if(screen==Screen::NODES)drawNodes();else if(screen==Screen::NODE)drawNode();else if(screen==Screen::DIRECT_PICK)drawDirectPicker();
}
void drawNetwork(){screen=Screen::NETWORK;drawDashboard();}
void drawRadioStats() {
  screen=Screen::RADIO_STATS;header("Network checks");
  smallButton(230,4,78,26,autoConnectEnabled?"Auto on":"Auto off");
  smallButton(164,4,60,26,"Clock");
  lcd.setTextColor(TFT_WHITE,BG);
  lcd.drawString(radioProfile.substring(0,46),12,43,1);
  String counters=haveStats[2]?"Radio RX "+String(packetsRx)+" / TX "+String(packetsTx)+" (since boot)":"Packet counters: not read / unsupported";
  lcd.drawString(counters,12,61,1);
  if(haveStats[2]&&statsBaseline)lcd.drawString("Change since first check: RX +"+String(packetsRx-statsBaselineRx)+" TX +"+String(packetsTx-statsBaselineTx),12,78,1);
  if(haveStats[1])lcd.drawString("Last packet RSSI "+String(lastRssi)+" dBm / SNR "+String(lastSnrQ/4.0f,1)+" dB",12,95,1);
  if(haveStats[0])lcd.drawString("Radio outbound queue: "+String(txQueue),12,112,1);
  String heard=haveHeard?"Packet heard "+String((millis()-lastHeardAt)/1000)+"s ago; "+String(rawHeard)+" this session":"No live packet heard this session yet";
  lcd.drawString(heard,12,129,1);
  smallButton(12,147,68,25,"Leave");
  smallButton(86,147,110,25,"Read stats");
  smallButton(12,177,92,28,"Discover");smallButton(110,177,92,28,"Nodes");smallButton(208,177,100,28,"Back");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString((networkInfo.length()?networkInfo:String("Only a reply confirms a radio round trip.")).substring(0,49),12,218,1);
}
void drawNodes() { drawContactList(true); }
void drawNode() {
  if(nodeSelected<0||nodeSelected>=int(contacts.size())){drawNodes();return;}
  screen=Screen::NODE;auto& c=contacts[nodeSelected];header(displayText(String(c.name)).substring(0,21).c_str());
  smallButton(180,4,60,26,(c.flags&1)?"Unfav":"Fave");smallButton(244,4,64,26,"Edit");
  lcd.setTextColor(TFT_WHITE,BG);
  String route=c.routeStale?"Route changed; reload list":c.path==255?"Route: unknown / flood":String("Route: ")+String(c.path&63)+" hops (saved)";
  lcd.drawString(String(contactType(c.type))+" / "+route,12,44,1);
  String heard=!c.saved?"New discovery: Edit > Save to keep on Heltec":c.seen?"Advert heard "+String((millis()-c.seenAt)/1000)+"s ago this session":"Saved contact; current reachability unknown";
  lcd.drawString(heard,12,62,1);
  char keyText[32];snprintf(keyText,sizeof(keyText),"Key %02X%02X%02X%02X%02X%02X",c.key[0],c.key[1],c.key[2],c.key[3],c.key[4],c.key[5]);lcd.drawString(keyText,12,80,1);
  wrapped(haveProbeTarget&&memcmp(c.key,remoteKey,32)==0?networkResult:String("No test for this contact yet."),12,103,3);
  lcd.drawString(managementInfo.substring(0,48),12,150,1);
  smallButton(12,177,92,28,c.type==1?"Message":"Status");smallButton(110,177,92,28,"Find path");smallButton(208,177,100,28,"Back");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Saved details do not mean the node is online.",12,217,1);
}
void upsertContact(const Reply& r,bool live) {
  if(contactsParked){if(live&&parkedAdvertCount<4)parkedAdverts[parkedAdvertCount++]=r;else contactsRefreshAfterWeather=true;return;}
  diagnostics::Contact c{};if(!diagnostics::contact(r.data,r.length,c))return;c.saved=!live;
  for(auto& old:contacts)if(memcmp(old.key,c.key,32)==0) {
    c.seen=live||old.seen;c.seenAt=live?millis():old.seenAt;c.saved=!live||old.saved;old=c;return;
  }
  c.seen=live;c.seenAt=live?millis():0;
  if(contacts.size()<256)contacts.push_back(c);else contactsTruncated=true;
}
bool networkBusy(bool antennaOwner=false) {
  if(networkAction!=NetworkAction::NONE||pending==Pending::CONTACTS||pending==Pending::STATS||pending==Pending::PROBE||pending==Pending::DISCOVER||pending==Pending::ADVERT||remoteWaiting||nearby.active||antennaSampleActive||(antennaBatch&&!antennaOwner)) {
    networkInfo="A network check is already running";return true;
  }
  return false;
}
void requestStats() {
  if(networkBusy()){refreshNetwork();return;}
  statsStep=0;networkAction=NetworkAction::STATS;networkInfo="Reading local radio counters...";refreshNetwork();
}
void requestContacts(bool forDirect) {
  if(contactsParked){networkInfo="Weather lookup running; try Load shortly";refreshNetwork();return;}
  if(networkBusy()){refreshNetwork();return;}
  contactsForDirect=forDirect;
  nodeSelected=-1;networkAction=NetworkAction::CONTACTS;networkInfo="Loading cached contacts...";refreshNetwork();
}
void requestProbe(bool path) {
  if(nodeSelected<0||nodeSelected>=int(contacts.size()))return;
  if(networkBusy()){networkResult="A test is already running. Please wait.";drawNode();return;}
  if(!path&&contacts[nodeSelected].type!=2&&contacts[nodeSelected].type!=3) {
    networkResult="Status is for repeaters / room servers. Use Find path for a node.";drawNode();return;
  }
  networkAction=path?NetworkAction::PATH:NetworkAction::STATUS;
  memcpy(remoteKey,contacts[nodeSelected].key,32);haveProbeTarget=true;
  networkResult="Preparing one radio request...";drawNode();
}
void tickNetwork() {
  if(remoteWaiting&&int32_t(millis()-remoteDeadline)>=0) {
    remoteWaiting=false;networkResult="No reply before timeout. This does not prove the node is offline.";refreshNetwork();
  }
  if(!ready||!linkActive||pending!=Pending::NONE||networkAction==NetworkAction::NONE)return;
  NetworkAction action=networkAction;networkAction=NetworkAction::NONE;
  if(dispatchDiscovery(action))return;
  if(action==NetworkAction::STATS) {
    uint8_t q[]={0x38,statsStep};
    if(!sendCommand(q,2,Pending::STATS)){networkInfo="Could not request stats";refreshNetwork();}
  } else if(action==NetworkAction::CONTACTS) {
    if(contactsParked){contactsRefreshAfterWeather=true;return;}contactRevision++;contacts.clear();nodePage=0;contactsTruncated=false;uint8_t q=4;
    if(!sendCommand(&q,1,Pending::CONTACTS)){networkInfo="Could not read contacts";refreshNetwork();}
  } else if(haveProbeTarget) {
    remotePath=action==NetworkAction::PATH;
    uint8_t q[34]={uint8_t(remotePath?0x34:0x1b),0};size_t offset=remotePath?2:1;
    memcpy(q+offset,remoteKey,32);
    remoteWaiting=true;remoteStarted=millis();remoteDeadline=millis()+60000;
    networkResult="Requesting radio transmission; waiting for an actual reply...";
    if(!sendCommand(q,offset+32,Pending::PROBE)){remoteWaiting=false;networkResult="Request write failed; no reply confirmed.";}
    refreshNetwork();
  }
}
bool handleNetworkReply(const Reply& r) {
  if(contactsParked&&(r.code==0x8f||r.code==0x81||r.code==0x90)){contactsRefreshAfterWeather=true;return true;}
  if(r.code==0x8f&&r.length>=33) {
    for(size_t i=0;i<contacts.size();i++)if(!memcmp(contacts[i].key,r.data+1,32)) {
      contacts.erase(contacts.begin()+i);if(nodeSelected==int(i))nodeSelected=-1;else if(nodeSelected>int(i))nodeSelected--;break;
    }
    networkInfo="Heltec removed an old contact";refreshNetwork();return true;
  }
  if(r.code==0x90){networkInfo="Heltec contact storage full";managementInfo=networkInfo;refreshNetwork();return true;}
  if(r.code==0x81&&r.length>=33) {
    for(auto& c:contacts)if(!memcmp(c.key,r.data+1,32))c.routeStale=true;
    networkInfo="Contact route changed; Load for latest";refreshNetwork();return true;
  }
  if(r.code==0x88&&r.length>=4) {
    haveHeard=true;lastHeardAt=millis();rawHeard++;lastSnrQ=int8_t(r.data[1]);lastRssi=int8_t(r.data[2]);
    return true;
  }
  if(r.code==0x8a) {upsertContact(r,true);refreshNetwork();return true;}
  if(r.code==0x80&&r.length>=33) {
    for(auto& c:contacts)if(memcmp(c.key,r.data+1,32)==0){c.seen=true;c.seenAt=millis();break;}
    refreshNetwork();return true;
  }
  if(remoteWaiting&&diagnostics::sameReply(r.data,r.length,remoteKey,remotePath?0x8d:0x87)) {
    if(remotePath) {
      uint8_t out,in;if(!diagnostics::paths(r.data,r.length,out,in))return true;
      networkResult="REPLY RECEIVED: path confirmed. Out / back hops: "+String(out)+" / "+String(in)+". "+String((millis()-remoteStarted)/1000.0f,1)+"s";
    } else {
      if(r.length<=8)return true;
      networkResult="REPLY RECEIVED from this repeater/server in "+String((millis()-remoteStarted)/1000.0f,1)+"s. Round trip confirmed.";
    }
    noteFieldProof(remotePath?2:1,String(contacts.size()?"Selected node":"Node"));
    for(const auto& c:contacts)if(!memcmp(c.key,remoteKey,32)){fieldProofTarget=displayText(String(c.name));break;}
    remoteWaiting=false;Serial.println("Remote network reply matched selected contact");refreshNetwork();return true;
  }
  if(pending==Pending::CONTACTS) {
    if(r.code==2||r.code==3) {
      if(r.code==3)upsertContact(r,false);replyDeadline=millis()+12000;return true;
    }
    if(r.code==4){pending=Pending::NONE;contactsLoaded=true;networkInfo=String(contacts.size())+" cached contacts loaded";refreshNetwork();return true;}
  }
  if(pending==Pending::STATS&&r.code==24&&r.length>=2&&r.data[1]==statsStep) {
    bool valid=false;
    if(statsStep==0&&r.length>=11){txQueue=r.data[10];valid=true;}
    if(statsStep==1&&r.length>=14){noiseFloor=int16_t(r.data[2]|(r.data[3]<<8));lastRssi=int8_t(r.data[4]);lastSnrQ=int8_t(r.data[5]);valid=true;}
    if(statsStep==2&&r.length>=10){packetsRx=protocol::read32(r.data+2);packetsTx=protocol::read32(r.data+6);valid=true;
      if(!statsBaseline||packetsRx<statsBaselineRx||packetsTx<statsBaselineTx){statsBaseline=true;statsBaselineRx=packetsRx;statsBaselineTx=packetsTx;}}
    haveStats[statsStep]=valid;pending=Pending::NONE;statsStep++;
    if(statsStep<3)networkAction=NetworkAction::STATS;
    else {statsAt=millis();networkInfo="Stats read; remote reachability still needs a reply";refreshNetwork();}
    return true;
  }
  if(pending==Pending::PROBE&&r.code==6&&r.length>=10) {
    pending=Pending::NONE;
    if(remoteWaiting){uint32_t timeout=protocol::read32(r.data+6);timeout=constrain(timeout+5000,uint32_t(15000),uint32_t(120000));remoteDeadline=millis()+timeout;
      networkResult=String("Request accepted by radio (")+(r.data[1]?"flood":"saved route")+"). Waiting up to "+String(timeout/1000)+"s for remote reply.";refreshNetwork();}
    return true;
  }
  return false;
}
#include "field_ui.h"
#include "antenna_ui.h"
#include "discovery_ui.h"
#include "bot_ui.h"
#include "internet_ui.h"
#include "management_ui.h"
#include "terminal_ui.h"
#include "draft_ui.h"

void processReplies() {
  if(replyOverflow.exchange(false)) {
    if(pending==Pending::SEND)recordOutgoing(false);
    failLink("Bluetooth data overflow. Reconnect.");return;
  }
  if(linkActive&&!client->isConnected()) {
    if(pending==Pending::SEND)recordOutgoing(false);
    failLink("Radio disconnected. Draft kept.");return;
  }
  Reply r;
  while(popReply(r)) {
    Serial.printf("Companion reply 0x%02x (%u bytes)\n",r.code,r.length);
    if(!linkActive)continue;
    if(r.code==0x83) { lastPoll=millis()-3000;continue; }
    if(handleTerminalReply(r))continue;
    if(handleDelivery(r))continue;
    if(handleDiscoveryReply(r))continue;
    if(handleNetworkReply(r))continue;
    if(handleManagementReply(r))continue;
    if(pending==Pending::NONE)continue;
    Pending job=pending;
    if(r.code==1) {
      pending=Pending::NONE;
      if(job==Pending::CHANNEL) { channelLoading++;fetchChannel(); }
      else if(job==Pending::SEND) { bool automatic=pendingSend>=0&&sends[pendingSend].automatic;if(pendingSend>=0){updateSendResult(sends[pendingSend].timestamp,field::SendState::REJECTED);sends[pendingSend].active=false;}statusLine=automatic?"Bot reply rejected":"Radio rejected message; draft kept";if(!automatic)drawCompose();deliveryFeedback(statusLine,TFT_RED);pendingSend=-1; }
      else if(job==Pending::SYNC) { statusLine="Receive failed; will retry";refreshMessageScreen(); }
      else if(job==Pending::STATS) {
        haveStats[statsStep]=false;statsStep++;
        if(statsStep<3)networkAction=NetworkAction::STATS;
        else {networkInfo="Some stats unsupported by Heltec firmware";refreshNetwork();}
      }
      else if(job==Pending::CONTACTS) { networkInfo="Contact list request rejected";refreshNetwork(); }
      else if(job==Pending::PROBE) { remoteWaiting=false;networkResult="Radio rejected request. Contact / command may be unsupported.";refreshNetwork(); }
      else if(job==Pending::BATTERY) {batterySupported=false;batteryWanted=false;Serial.println("Battery query unsupported; normal messaging retained");if(screen==Screen::SETTINGS)drawSettings();}
      else if(job==Pending::MANAGE||job==Pending::VERIFY_CHANNEL) { managementResult(false,"Radio rejected change; please reload"); }
      else if(job==Pending::SET_CLOCK) { statusLine="Local time set; radio rejected update";Serial.println("Radio rejected clock update; local clock retained"); }
      else failLink("Radio rejected a setup request.");
      continue;
    }
    if(job==Pending::SELF&&r.code==5) {
      if(r.length>=3)radioTxPower=int8_t(r.data[2]);
      if(r.length>=36){memcpy(selfPublicKey,r.data+4,32);haveSelfKey=true;}
      wireName="";if(r.length>58)wireName.concat(reinterpret_cast<const char*>(r.data+58),r.length-58);
      if(r.length>=58)radioProfile=String(protocol::read32(r.data+48)/1000.0f,3)+" MHz / "+String(protocol::read32(r.data+52)/1000.0f,2)+" kHz / SF"+String(r.data[56])+" CR4/"+String(r.data[57]);
      pending=Pending::NONE;uint8_t q[]={0x16,3};if(!sendCommand(q,2,Pending::INFO))failLink("Device query failed.");
    } else if(job==Pending::INFO&&r.code==13&&r.length>=4) {
      firmwareCode=r.data[1];
      // Save a peer only after encrypted connection and MeshCore handshake.
      prefs.putString("radioAddr",activeAddress);prefs.putString("radioName",activeName);prefs.putUChar("radioType",activeAddressType);
      Serial.println("Successful radio identity saved for reconnect");
      maxChannels=min(40,int(r.data[3]));channelLoading=0;channelPage=0;
      for(auto& c:channels)c="";
      loadHistory();pending=Pending::NONE;uint8_t q=5;
      if(!sendCommand(&q,1,Pending::CLOCK))failLink("Clock query failed.");
      linkScreen("MeshCore connected","Loading your channels...");
    } else if(job==Pending::CLOCK&&r.code==9&&r.length>=5) {
      uint32_t value=protocol::read32(r.data+1);
      uint32_t local=nowSeconds();
      if(!clockTrusted&&value>=local&&value<=4102444799u) {clockBase=value;clockAt=millis();clockSource="Heltec clock";clockTrusted=true;}
      else if(value<local)clockWriteWanted=true;
      Serial.printf("Radio clock %lu; terminal clock %lu (%s)\n",static_cast<unsigned long>(value),static_cast<unsigned long>(nowSeconds()),clockSource.c_str());
      pending=Pending::NONE;fetchChannel();
    } else if(job==Pending::CHANNEL&&r.code==18&&r.length>=34&&r.data[1]==channelLoading) {
      channels[channelLoading]=packetText(r.data+2,32);
      if(r.length>=50){memcpy(channelKeys[channelLoading],r.data+34,16);haveChannelKey[channelLoading]=true;}
      pending=Pending::NONE;channelLoading++;fetchChannel();
    } else if(job==Pending::SYNC&&(r.code==7||r.code==8||r.code==16||r.code==17||r.code==27)) {
      pending=Pending::NONE;processMessage(r);lastPoll=millis()-3000;
    } else if(job==Pending::SYNC&&r.code==10) { pending=Pending::NONE;botQueueDrained(); }
    else if(job==Pending::SEND&&(r.code==0||r.code==6)) {
      pending=Pending::NONE;statusLine="Connected";
      if(pendingSend>=0) {
        auto& s=sends[pendingSend];s.accepted=true;updateSendResult(s.timestamp,field::SendState::ACCEPTED);
        if(s.direct&&r.code==6&&r.length>=10) {
          s.ack=protocol::read32(r.data+2);uint32_t timeout=min(protocol::read32(r.data+6),uint32_t(115000))+5000;
          s.deadline=millis()+max(timeout,uint32_t(15000));
          if(!s.done)deliveryFeedback("Radio accepted; waiting for recipient ACK",0x5C7F,max(timeout,uint32_t(15000)));
        } else if(s.direct){s.done=true;updateSendResult(s.timestamp,field::SendState::TRACKING_UNAVAILABLE);deliveryFeedback("Radio accepted; ACK tracking unavailable",TFT_ORANGE);}
        else if(!s.done) {
          if(s.payloadLength)deliveryFeedback("Radio accepted; listening for a repeat",0x5C7F,45000);
          else {s.done=true;updateSendResult(s.timestamp,field::SendState::TRACKING_UNAVAILABLE);deliveryFeedback("Radio accepted; repeat check unavailable",TFT_ORANGE);}
        }
      }
      recordOutgoing(true);
      if(pendingSend>=0){const auto& s=sends[pendingSend];bool sameDestination=composingDirect==s.direct&&(s.direct?!memcmp(directPeer,s.peer,6):currentChannel==s.channel);
        if(!s.automatic&&sameDestination&&draft==s.text){draft="";clearSavedDraft();resetChatView=true;if(composingDirect)drawDirectChat();else drawChat();}}
      pendingSend=-1;refreshMessageScreen();
    }
    else if(job==Pending::SET_CLOCK&&r.code==0){pending=Pending::NONE;Serial.println("Heltec accepted clock synchronisation");}
  }
  if(pending!=Pending::NONE&&int32_t(millis()-replyDeadline)>=0) {
    Pending job=pending;pending=Pending::NONE;
    if(job==Pending::SEND) { statusLine="Not confirmed; check before retry";recordOutgoing(false);if(pendingSend>=0){sends[pendingSend].timedOut=true;updateSendResult(sends[pendingSend].timestamp,field::SendState::UNCONFIRMED);}if(pendingSend<0||!sends[pendingSend].automatic)drawCompose();deliveryFeedback("Send unconfirmed; check before retry",TFT_ORANGE);failLink("Send response timed out; delivery unconfirmed."); }
    else if(job==Pending::BATTERY){batterySupported=false;batteryWanted=false;failLink("Battery response timed out; reconnecting.");}
    else if(job==Pending::SET_CLOCK){statusLine="Local time set; radio update timed out";Serial.println("Radio clock update timed out; local clock retained");}
    else if(job==Pending::MANAGE||job==Pending::VERIFY_CHANNEL){managementResult(false,"Change unconfirmed; reload before retry");failLink("Change unconfirmed. Reconnect and reload.");}
    else if(job==Pending::STATS||job==Pending::CONTACTS||job==Pending::PROBE||job==Pending::DISCOVER||job==Pending::ADVERT)failLink("Network command timed out. Reconnect.");
    else failLink("Radio response timed out.");
  }
}
void setup() {
  Serial.begin(115200);delay(300);
  Serial.println("Mesh Pilot 0.20.0 / E32R28T");
  pinMode(21,OUTPUT);digitalWrite(21,HIGH);
  lcd.init();lcd.setRotation(1);
  touchBus.begin(25,39,32,33);touch.begin(touchBus);touch.setRotation(1);
  prefs.begin("meshterm",false);
  brightness=prefs.getUChar("light",255);brightness=max(uint8_t(40),brightness);autoDim=prefs.getBool("dim",true);
  ledcSetup(7,5000,8);ledcAttachPin(21,7);ledcWrite(7,brightness);lastTouchAt=millis();
  clockBase=max(uint32_t(BUILD_UNIX_TIME),uint32_t(prefs.getULong("timeLast",0)));clockAt=millis();
  fixtureMode=true;
  bool checksPassed=protocol::selfTest()&&diagnostics::selfTest()&&text_render::selfTest()&&clock_tools::selfTest()&&send_tracking::selfTest()&&management::selfTest()&&managementUiSelfTest()&&terminal_tools::selfTest()&&terminalSelfTest()&&draft_tools::selfTest()&&scroll_tools::selfTest()&&scrollUiSelfTest()&&scrollBandSelfTest()&&discovery::selfTest()&&discoveryUiSelfTest()&&field::selfTest()&&antenna::selfTest()&&bot::selfTest()&&botUiSelfTest()&&weather::selfTest()&&weatherJsonSelfTest()&&replyQueueSelfTest();
  fixtureMode=false;
  if(!checksPassed){Serial.println("Boot fixtures FAILED");header("Protocol/text/send check failed");while(true)delay(1000);}
  Serial.println("Message, network, Unicode, clock, delivery, management, terminal, scrolling, nearby discovery, field, antenna and bot fixtures passed");
  storageReady=LittleFS.begin(false);
  if(!storageReady&&!prefs.getBool("fsinit",false))storageReady=LittleFS.begin(true);
  if(storageReady)prefs.putBool("fsinit",true);
  Serial.printf("History storage: %s\n",storageReady?"ready":"unavailable");
  NimBLEDevice::init("Mesh Pilot");
  NimBLEDevice::setMTU(247);
  replies=xQueueCreate(64,sizeof(RadioPacket*));
  client=NimBLEDevice::createClient();
  client->setClientCallbacks(&clientCallbacks,false);
  client->setConnectTimeout(10);
  autoConnectEnabled=prefs.getBool("autolink",true);
  // Migrate an earlier installation only when exactly one stored bond matches
  // this terminal's existing per-radio message journal. Never guess a peer.
  if(!prefs.getString("radioAddr","").length()&&storageReady) {
    String candidate;uint8_t candidateType=0;int matches=0;
    for(int i=0;i<NimBLEDevice::getNumBonds();i++) {
      NimBLEAddress address=NimBLEDevice::getBondedAddress(i);String id=address.toString().c_str(),fileId=id;fileId.replace(":","");
      if(LittleFS.exists("/"+fileId+".log")){candidate=id;candidateType=address.getType();matches++;}
    }
    if(matches==1){prefs.putString("radioAddr",candidate);prefs.putUChar("radioType",candidateType);prefs.putString("radioName","Saved MeshCore radio");Serial.println("Existing radio bond and history matched for migration");}
  }
  initInternet();
  startupReconnect=autoConnectEnabled&&prefs.getString("radioAddr","").length();
  calibrated=prefs.getBool("cal",false);
  if(calibrated) {
    ax=prefs.getFloat("ax");bx=prefs.getFloat("bx");cx=prefs.getFloat("cx");
    ay=prefs.getFloat("ay");by=prefs.getFloat("by");cy=prefs.getFloat("cy");
    home();
  } else calibration();
  Serial.printf("Ready; free heap %u\n",ESP.getFreeHeap());
}
void loop() {
  if(startupReconnect){startupReconnect=false;connectRadio(true);}
  processReplies();
  tickTerminal();
  tickClock();
  tickDelivery();
  tickDiscovery();
  tickField();
  tickAntenna();
  tickNetwork();
  tickInternet();
  tickBot();
  pollMessages();
  bool down=touch.touched();
  if(handleChatDrag(down)){pressed=down;delay(10);return;}
  if(down && !pressed) {
    bool waking=displayDimmed;lastTouchAt=millis();displayDimmed=false;ledcWrite(7,brightness);
    if(waking){refreshMessageScreen();pressed=down;delay(20);return;}
    TS_Point p=touch.getPoint();
    int x=lroundf(ax*p.x+bx*p.y+cx),y=lroundf(ay*p.x+by*p.y+cy);
    // Never log keypad coordinates: they would reveal the pairing PIN.
    if(internetTouch(x,y)) { /* Wi-Fi and weather. */ }
    else if(botTouch(x,y)) { /* Per-channel bot controls. */ }
    else if(terminalTouch(x,y)) { /* Device settings. */ }
    else if(antennaTouch(x,y)) { /* Antenna comparison. */ }
    else if(fieldTouch(x,y)) { /* Dashboard, results and coverage. */ }
    else if(discoveryTouch(x,y)) { /* Discover and announce. */ }
    else if(managementTouch(x,y)) { /* Management screens handled above. */ }
    else if(screen==Screen::CLOCK) {
      if(x>=12&&x<204&&y>=122&&y<234&&(x-12)%64<58&&(y-122)%28<25) {
        int key=((y-122)/28)*3+(x-12)/64;
        if(key==9)clockDigits="";
        else if(key==11){if(clockDigits.length())clockDigits.remove(clockDigits.length()-1);}
        else if(clockDigits.length()<12)clockDigits+=char(key==10?'0':'1'+key);
        drawClock();
      } else if(x>=212&&x<=308&&y>=130&&y<=166) {
        uint32_t value;
        if(clock_tools::parse(clockDigits.c_str(),clockDigits.length(),value))setClock(value,"Set offline (UTC)");
        else {clockSource="Enter valid YYYYMMDDhhmm";drawClock();}
      } else if(x>=212&&x<=308&&y>=180&&y<=216)drawNetwork();
    } else if(screen==Screen::PIN) {
      if(x>=212 && x<=308 && y>=112 && y<=152) connectRadio();
      else if(x>=12 && x<204 && y>=88 && y<232) {
        int col=(x-12)/64,row=(y-88)/36;
        if((x-12)%64<58 && (y-88)%36<30) {
          int key=row*3+col;
          if(key==9) { pinText="";drawRadios(); }
          else { if(key==11) { if(pinText.length()) pinText.remove(pinText.length()-1); }
                 else if(pinText.length()<6) pinText+=char(key==10?'0':'1'+key);
                 drawPin(); }
        }
      }
    } else if(screen==Screen::COMPOSE) {
      if(pending==Pending::SEND) { /* Ignore edits until the send result. */ }
      else if(y>=83&&y<179&&x>=12&&x<308) {
        int row=(y-83)/32;
        const char* normal[]={"qwertyuiop","asdfghjkl","zxcvbnm.,?"};
        const char* special[]={"1234567890","!@#$%&*()","-_=+/:;?'"};
        String keys=symbols?special[row]:normal[row];if(upperCase&&!symbols)keys.toUpperCase();
        int width=296/keys.length(),col=(x-12)/width;
        if(col<int(keys.length())&&(x-12)%width<width-2&&(y-83)%32<29&&draft.length()<120) { draft+=keys[col];drawCompose(); }
      } else if(y>=180&&y<210) {
        if(x>=12&&x<=58)upperCase=!upperCase;
        else if(x>=62&&x<=108)symbols=!symbols;
        else if(x>=112&&x<=212&&draft.length()<120)draft+=' ';
        else if(x>=216&&x<=308&&draft.length())draft.remove(draft.length()-1);
        drawCompose();
      } else if(y>=214&&y<=237) {
        if(x>=12&&x<=80){if(composingDirect)drawDirectChat();else drawChat();}
        else if(x>=228&&x<=308)sendDraft();
      }
    } else if(screen==Screen::RADIO_STATS) {
      if(x>=164&&x<=224&&y>=4&&y<=30){clockDigits="";drawClock();}
      else if(x>=230&&x<=308&&y>=4&&y<=30){autoConnectEnabled=!autoConnectEnabled;prefs.putBool("autolink",autoConnectEnabled);if(!autoConnectEnabled)cancelReconnect();else reconnectSuppressed=false;drawRadioStats();}
      else if(x>=12&&x<=80&&y>=147&&y<=172){
        if(pending==Pending::NONE&&!remoteWaiting&&!nearby.active){interruptSendResults();for(auto& s:sends)s.active=false;cancelReconnect();saveReadState();saveDraftNow();resumeAfterReconnect=false;pending=Pending::NONE;ready=false;linkActive=false;commandChar=nullptr;networkAction=NetworkAction::NONE;client->disconnect();drawRadios();}
      }
      else if(x>=86&&x<=196&&y>=147&&y<=172)requestStats();
      else if(y>=177&&y<=205) {
        if(x>=12&&x<=104)drawRadioTools();
        else if(x>=110&&x<=202){drawNodes();if(contacts.empty())requestContacts();}
        else if(x>=208&&x<=308)drawNetwork();
      }
    } else if(screen==Screen::CHANNELS) {
      if(x>=108&&x<=168&&y>=4&&y<=30){drawSettings();batteryWanted=true;}
      else if(x>=174&&x<=252&&y>=4&&y<=30)drawChannelSlots();
      else if(x>=258&&x<=308&&y>=4&&y<=30){drawNetwork();requestStats();}
      else if(y>=42&&y<170&&x>=12&&x<=308) {
        int row=(y-42)/32;auto list=activeChannels();int item=channelPage*4+row;
        if((y-42)%32<28&&item<int(list.size())) {
          if(composingDirect||currentChannel!=list[item])draft="";
          composingDirect=false;currentChannel=list[item];resetChatView=true;drawChat();
        }
      } else if(y>=177&&y<=204) {
        if(x>=12&&x<=80){channelPage--;drawChannels();}
        else if(x>=84&&x<=152){channelPage++;drawChannels();}
        else if(x>=156&&x<=224){resetChatView=true;drawInbox();}
        else if(x>=228&&x<=308){drawDirectPicker();if(!contactsLoaded)requestContacts(true);}
      }
    } else if(screen==Screen::CHAT||screen==Screen::INBOX||screen==Screen::DIRECT_CHAT) {
      bool inbox=screen==Screen::INBOX,direct=screen==Screen::DIRECT_CHAT;
      if(inbox&&x>=230&&x<=278&&y>=4&&y<=30){drawDirectPicker();if(contacts.empty())requestContacts(true);}
      else if(y>=177&&y<=204) {
        if(x>=12&&x<=152)jumpToLatest();
        else if(x>=156&&x<=224) { if(inbox)drawChannels();else {if(composingDirect!=direct)draft="";composingDirect=direct;statusLine="";drawCompose();} }
        else if(x>=228&&x<=308){if(direct)drawDirectPicker();else drawChannels();}
      }
    } else if(screen==Screen::LINK) {
      if(x>=212&&x<=308&&y>=184&&y<=224&&pending==Pending::NONE&&!client->isConnected()){reconnectSuppressed=false;connectRadio(true);}
      else if(x>=12 && x<=202 && y>=184 && y<=224) {
        cancelReconnect();resumeAfterReconnect=false;waitingFor=0;pending=Pending::NONE;ready=false;linkActive=false;commandChar=nullptr;
        if(client->isConnected()) client->disconnect();
        drawRadios();
      }
    } else if(screen==Screen::RADIOS && x>=12 && x<=308 && y>=42 && y<178) {
      int row=(y-42)/34;
      if((y-42)%34<30 && row<int(radios.size())) { selected=row;pinText="";drawPin(); }
    } else if(y>=184 && y<=224) { if(x>=12 && x<=202) scan();else if(x>=212 && x<=308) calibration(); }
  }
  pressed=down;delay(20);
}
