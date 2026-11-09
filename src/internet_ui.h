#pragma once
#include "weather_ca.h"
String wifiName,wifiPassword,internetInfo="Wi-Fi off",localWeatherInfo;
bool wifiEnabled=false,wifiStarted=false,weatherBusy=false,weatherWaiting=false;
uint32_t wifiAttemptAt=0,wifiPaintAt=0,weatherToken=0,localWeatherToken=0;
QueueHandle_t weatherResults=nullptr;
struct WeatherJob {char place[33];uint32_t token,epoch;bool forBot;};
struct WeatherResult {weather::Data data;char error[65];uint32_t token,stackFree;bool forBot,ok;};
struct WeatherCache {weather::Data data;uint32_t at=0;bool used=false;} weatherCache[4];
uint8_t weatherCacheNext=0;
String safePlace(const char* name){return String(text_render::ascii(reinterpret_cast<const uint8_t*>(name),strlen(name)).c_str());}
bool parseGeocoding(const String& body,const char* query,float& lat,float& lon,char* name){
  DynamicJsonDocument doc(2048);if(deserializeJson(doc,body,DeserializationOption::NestingLimit(5)))return false;
  JsonArray results=doc["results"].as<JsonArray>();if(results.size()==0||results.size()>3)return false;
  int chosen=-1,exact=0;std::string requested=query;requested=requested.substr(0,requested.find(','));requested=weather::normalize(requested);
  for(size_t i=0;i<results.size();i++){
    if(!results[i]["name"].is<const char*>())return false;
    if(weather::normalize(results[i]["name"].as<const char*>())==requested){chosen=i;exact++;}
  }
  if(results.size()==1)chosen=0;else if(exact!=1)return false;
  JsonObject item=results[chosen];if(!item["country_code"].is<const char*>()||strcmp(item["country_code"],"GB")||!item["latitude"].is<float>()||!item["longitude"].is<float>())return false;
  lat=item["latitude"];lon=item["longitude"];if(!isfinite(lat)||!isfinite(lon)||lat < -90||lat>90||lon < -180||lon>180)return false;
  safePlace(item["name"]).substring(0,24).toCharArray(name,25);return name[0];
}
bool parseWeather(const String& body,weather::Data& data,uint32_t now){
  DynamicJsonDocument doc(2048);if(deserializeJson(doc,body,DeserializationOption::NestingLimit(5)))return false;
  JsonObject c=doc["current"];JsonObject units=doc["current_units"];
  if(!c["time"].is<uint32_t>()||!c["temperature_2m"].is<float>()||!c["wind_speed_10m"].is<float>()||!c["wind_direction_10m"].is<float>()||!c["weather_code"].is<int>())return false;
  if(!units["temperature_2m"].is<const char*>()||strcmp(units["temperature_2m"],"\xC2\xB0" "C")||!units["wind_speed_10m"].is<const char*>())return false;
  const char* windUnits=units["wind_speed_10m"];if(strcmp(windUnits,"mp/h")&&strcmp(windUnits,"mph"))return false;
  data.epoch=c["time"];data.temperature=c["temperature_2m"];data.wind=c["wind_speed_10m"];data.direction=c["wind_direction_10m"];data.code=c["weather_code"];
  return weather::valid(data)&&data.epoch<=now+1800&&now<=data.epoch+5400;
}
bool weatherHttp(const String& url,String& body,char* error){
  WiFiClientSecure secure;secure.setCACert(WEATHER_CA);secure.setHandshakeTimeout(6);secure.setTimeout(3);
  HTTPClient http;http.useHTTP10(true);http.setReuse(false);http.setConnectTimeout(4000);http.setTimeout(3000);
  if(!http.begin(secure,url)){strcpy(error,"Secure weather setup failed");return false;}
  int status=http.GET();if(status!=200){strcpy(error,status<0?"Secure weather connection failed":"Weather service unavailable");http.end();return false;}
  int length=http.getSize();if(length>4096){strcpy(error,"Weather response too large");http.end();return false;}
  bool ok=body.reserve(4096);WiFiClient* stream=http.getStreamPtr();uint32_t started=millis();int received=0;
  while(ok&&(http.connected()||stream->available())&&(length<0||received<length)){
    if(millis()-started>=8000){ok=false;break;}
    int available=stream->available();if(available){char bytes[128];int n=stream->readBytes(bytes,min(available,128));if(n<=0||received+n>4096){ok=false;break;}body.concat(bytes,n);received+=n;}else vTaskDelay(pdMS_TO_TICKS(10));
  }
  ok=ok&&received>0&&(length<0||received==length);http.end();if(!ok)strcpy(error,"Weather response incomplete");return ok;
}
void weatherWorker(void* parameter){
  WeatherJob job=*static_cast<WeatherJob*>(parameter);delete static_cast<WeatherJob*>(parameter);
  WeatherResult result{};result.token=job.token;result.forBot=job.forBot;strcpy(result.data.query,job.place);
  {String body;float lat=0,lon=0;
    String url="https://geocoding-api.open-meteo.com/v1/search?name="+String(weather::encode(job.place).c_str())+"&count=3&language=en&countryCode=GB";
    if(weatherHttp(url,body,result.error)){
      if(!parseGeocoding(body,job.place,lat,lon,result.data.name))strcpy(result.error,"Place unknown or ambiguous; add county");
      else{body="";url="https://api.open-meteo.com/v1/forecast?latitude="+String(lat,5)+"&longitude="+String(lon,5)+"&current=temperature_2m,weather_code,wind_speed_10m,wind_direction_10m&temperature_unit=celsius&wind_speed_unit=mph&timeformat=unixtime&timezone=GMT&forecast_days=1";
        if(weatherHttp(url,body,result.error)){result.ok=parseWeather(body,result.data,job.epoch);if(!result.ok)strcpy(result.error,"Weather data invalid or out of date");}}
    }
  }
  result.stackFree=uxTaskGetStackHighWaterMark(nullptr);xQueueSend(weatherResults,&result,0);vTaskDelete(nullptr);
}
int cachedWeather(const char* query,uint32_t maxAge){for(int i=0;i<4;i++)if(weatherCache[i].used&&!strcmp(query,weatherCache[i].data.query)&&weather::cacheAllowed(nowSeconds(),weatherCache[i].data.epoch,millis()-weatherCache[i].at,maxAge))return i;return -1;}
String cachedReply(int index){auto& data=weatherCache[index].data;return String(weather::render(data,(nowSeconds()-data.epoch)/60,true).c_str());}
void deliverWeather(const String& message,bool forBot){
  if(forBot){weatherWaiting=false;if(botQueued)botReply="[bot] "+message;}
  else {localWeatherInfo=message;if(screen==Screen::WEATHER_RESULT){header("Weather check / no radio send");wrapped(localWeatherInfo,12,45,11);smallButton(12,177,100,28,"Back");}}
}
void cancelBotWeather(){if(weatherWaiting){weatherToken++;weatherWaiting=false;botQueued=false;}}
bool botWeatherWaiting(){return weatherWaiting;}
bool requestWeather(const String& input,bool forBot){
  std::string place=weather::normalize(input.c_str());
  if(!weather::location(place)){deliverWeather("Use a UK place name, up to 32 characters.",forBot);return false;}
  int cached=cachedWeather(place.c_str(),900000);if(cached>=0){deliverWeather(cachedReply(cached),forBot);return true;}
  String error;
  if(!wifiEnabled||WiFi.status()!=WL_CONNECTED)error="Internet unavailable";
  else if(!clockTrusted)error="Set the clock before secure weather lookup";
  else if(weatherBusy)error="Weather lookup busy; try later";
  else if(ESP.getFreeHeap()<52000||heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)<26000)error="Weather unavailable: low memory";
  if(error.length()){cached=cachedWeather(place.c_str(),21600000);deliverWeather(cached>=0?cachedReply(cached):error,forBot);return false;}
  if(!weatherResults)weatherResults=xQueueCreate(1,sizeof(WeatherResult));
  WeatherJob* job=new(std::nothrow) WeatherJob{};if(!job||!weatherResults){delete job;deliverWeather("Weather unavailable: low memory",forBot);return false;}
  timeval clock={time_t(nowSeconds()),0};settimeofday(&clock,nullptr);
  strcpy(job->place,place.c_str());job->token=++weatherToken;job->epoch=nowSeconds();job->forBot=forBot;
  if(!forBot)localWeatherToken=job->token;weatherWaiting=forBot;weatherBusy=true;
  if(xTaskCreate(weatherWorker,"weather",12288,job,1,nullptr)!=pdPASS){delete job;weatherBusy=false;weatherWaiting=false;deliverWeather("Weather unavailable: low memory",forBot);return false;}
  return true;
}
void initInternet(){wifiName=prefs.isKey("wifiSsid")?prefs.getString("wifiSsid"):String("");wifiPassword=prefs.isKey("wifiPass")?prefs.getString("wifiPass"):String("");wifiEnabled=prefs.getBool("wifiOn",false);}
void tickInternet(){
  if(fixtureMode)return;
  if(wifiEnabled&&ready&&!wifiStarted){WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);WiFi.begin(wifiName.c_str(),wifiPassword.c_str());wifiStarted=true;wifiAttemptAt=millis();}
  WeatherResult result{};
  if(weatherResults&&xQueueReceive(weatherResults,&result,0)==pdTRUE){
    weatherBusy=false;
    if(result.ok){auto& slot=weatherCache[weatherCacheNext++%4];slot.data=result.data;slot.at=millis();slot.used=true;}
    bool relevant=result.forBot?weatherWaiting&&botQueued&&result.token==weatherToken:result.token==localWeatherToken;
    if(relevant){String reply=result.ok?String(weather::render(result.data).c_str()):String(result.error);if(!result.ok){int cached=cachedWeather(result.data.query,21600000);if(cached>=0)reply=cachedReply(cached);}deliverWeather(reply,result.forBot);}
    Serial.printf("Weather lookup finished: success=%d; stackMin=%u; heap=%u; largest=%u\n",result.ok,unsigned(result.stackFree),ESP.getFreeHeap(),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
  }
  if(screen==Screen::WIFI_SETUP&&millis()-wifiPaintAt>=1500){wifiPaintAt=millis();lcd.fillRect(12,149,296,22,BG);lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(!wifiEnabled?"Wi-Fi off":WiFi.status()==WL_CONNECTED?"Wi-Fi connected; internet checked on lookup":wifiStarted&&millis()-wifiAttemptAt>20000?"Not connected. Check name/password or retry.":"Connecting to Wi-Fi...",12,153,1);}
}
void saveWifiText(const String& value,bool password){if(password)wifiPassword=value;else wifiName=value;}
void drawWifi(){
  screen=Screen::WIFI_SETUP;header("Wi-Fi / 2.4 GHz");
  smallButton(12,42,296,28,("Name: "+wifiName).substring(0,33).c_str());smallButton(12,77,296,28,wifiPassword.length()?"Password: saved / hidden":"Password: tap to enter");
  smallButton(12,112,142,28,"Save + connect");smallButton(160,112,148,28,wifiEnabled?"Turn Wi-Fi off":"Wi-Fi off");
  smallButton(12,177,80,28,"Back");smallButton(98,177,134,28,"Check weather");smallButton(238,177,70,28,"Forget");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(internetInfo.substring(0,49),12,215,1);wifiPaintAt=millis()-1500;
}
void localWeatherTest(){
  if(fixtureMode)return;screen=Screen::WEATHER_RESULT;header("Weather check / no radio send");localWeatherInfo="Looking up weather...";wrapped(localWeatherInfo,12,45,4);smallButton(12,177,100,28,"Back");requestWeather(String(botChannels[botSelected].place),false);
}
void drawWeatherSettings(){
  screen=Screen::WEATHER_SETTINGS;header("Weather bot / this channel");auto& c=botChannels[botSelected];
  smallButton(12,42,296,30,c.flags&16?"Weather ON - tap to disable":"Weather OFF - tap to enable");
  lcd.setTextColor(TFT_WHITE,BG);lcd.drawString("Triggers: wx or weather, any case",12,85,1);lcd.drawString(("Default: "+String(c.place)).substring(0,49),12,104,1);lcd.drawString("Example: wx Ramsbottom (UK places)",12,123,1);lcd.drawString("Open-Meteo model data; time shown in UTC.",12,142,1);
  smallButton(12,177,80,28,"Back");smallButton(98,177,96,28,"Wi-Fi");smallButton(200,177,108,28,"Check here");lcd.setTextColor(TFT_WHITE,BG);lcd.drawString(botInfo.substring(0,49),12,215,1);
}
bool internetTouch(int x,int y){
  if(screen==Screen::WEATHER_RESULT){if(x>=12&&x<=112&&y>=177&&y<=205)drawWifi();return true;}
  if(screen==Screen::WEATHER_SETTINGS){
    if(x>=12&&x<=308&&y>=42&&y<=72){auto& c=botChannels[botSelected];bool conflict=!strcmp(c.test,"wx")||!strcmp(c.test,"weather");for(int i=0;i<2;i++)if(!strcmp(c.words[i],"wx")||!strcmp(c.words[i],"weather"))conflict=true;
      if(!(c.flags&16)&&conflict)botInfo="Rename your existing wx/weather keyword first.";else{c.flags^=16;stopEditedBot();saveBotSettings();}drawWeatherSettings();}
    else if(y>=177&&y<=205){if(x>=12&&x<=92)drawBotCommands();else if(x>=98&&x<=194)drawWifi();else if(x>=200&&x<=308)localWeatherTest();}return true;
  }
  if(screen!=Screen::WIFI_SETUP)return false;
  if(x>=12&&x<=308&&((y>=42&&y<=70)||(y>=77&&y<=105))){textPurpose=y>=77?13:12;editorReturn=Screen::WIFI_SETUP;editText=textPurpose==13?wifiPassword:wifiName;textLimit=textPurpose==13?63:32;upperCase=symbols=false;credentialSymbols=0;drawTextEditor();}
  else if(y>=112&&y<=140){
    if(x>=12&&x<=154){if(!wifiName.length())internetInfo="Enter a Wi-Fi network name first.";else if(wifiPassword.length()&&wifiPassword.length()<8)internetInfo="Password needs at least 8 characters.";
      else{bool saved=prefs.putString("wifiSsid",wifiName)==wifiName.length()&&prefs.putString("wifiPass",wifiPassword)==wifiPassword.length();if(saved){wifiEnabled=true;prefs.putBool("wifiOn",true);WiFi.disconnect();wifiStarted=false;internetInfo="Saved. Connecting in the background.";}else internetInfo="Wi-Fi details could not be saved.";}}
    else if(x>=160&&x<=308){wifiEnabled=false;prefs.putBool("wifiOn",false);WiFi.disconnect(true);wifiStarted=false;cancelBotWeather();internetInfo="Wi-Fi off; details kept.";}drawWifi();
  }else if(y>=177&&y<=205){if(x>=12&&x<=92)drawSettings();else if(x>=98&&x<=232)localWeatherTest();else if(x>=238&&x<=308){wifiEnabled=false;wifiStarted=false;WiFi.disconnect(true);prefs.remove("wifiSsid");prefs.remove("wifiPass");prefs.remove("wifiOn");wifiName=wifiPassword="";cancelBotWeather();internetInfo="Wi-Fi details forgotten.";drawWifi();}}return true;
}
void internetStatus(){Serial.printf("Internet status: enabled=%d; connected=%d; worker=%d; pendingBot=%d; heap=%u; largest=%u\n",wifiEnabled,WiFi.status()==WL_CONNECTED,weatherBusy,weatherWaiting,ESP.getFreeHeap(),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));}
bool weatherJsonSelfTest(){
  float lat=0,lon=0;char name[25]={};
  bool ok=parseGeocoding("{\"results\":[{\"name\":\"Ramsbottom\",\"country_code\":\"GB\",\"latitude\":53.6,\"longitude\":-2.3}]}","ramsbottom",lat,lon,name)&&!strcmp(name,"Ramsbottom");
  ok=ok&&!parseGeocoding("{\"results\":[]}","unknown",lat,lon,name)&&!parseGeocoding("{\"results\":[{\"name\":\"A\",\"country_code\":\"GB\",\"latitude\":null,\"longitude\":1}]}","a",lat,lon,name);
  weather::Data data{};String good="{\"current_units\":{\"temperature_2m\":\"\\u00b0C\",\"wind_speed_10m\":\"mp/h\"},\"current\":{\"time\":1794225600,\"temperature_2m\":8,\"weather_code\":3,\"wind_speed_10m\":12,\"wind_direction_10m\":270}}";
  ok=ok&&parseWeather(good,data,1794225600)&&!parseWeather(good,data,1794232000);good.replace("\"temperature_2m\":8","\"temperature_2m\":null");ok=ok&&!parseWeather(good,data,1794225600);
  bool savedWaiting=weatherWaiting,savedQueued=botQueued;uint32_t savedToken=weatherToken;weatherWaiting=true;botQueued=true;weatherToken=41;cancelBotWeather();
  ok=ok&&!weatherWaiting&&!botQueued&&weatherToken==42;weatherWaiting=savedWaiting;botQueued=savedQueued;weatherToken=savedToken;
  return ok;
}
