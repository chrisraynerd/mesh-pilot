#pragma once
#include <string>
#include <stddef.h>
namespace reply_tools {
// Plain text on the mesh; no private-message addressing or hidden reply IDs.
inline size_t prefix(const std::string& text){if(text.compare(0,2,"> "))return 0;size_t end=text.find('\n');return end!=std::string::npos&&end>=4&&end<48?end+1:0;}
inline std::string compact(const std::string& text){std::string out;bool space=false;for(unsigned char c:text){if(c<=32||c==127){space=!out.empty();continue;}if(space){out+=' ';space=false;}out+=c<127?char(c):'?';}return out;}
inline std::string clip(const std::string& text,size_t maximum){return text.size()<=maximum?text:text.substr(0,maximum-3)+"...";}
inline std::string quote(const std::string& sender,const std::string& message){auto name=compact(sender);if(name.empty())name="Unknown";auto body=compact(message.substr(prefix(message)));if(body.empty())body="(empty)";return "> "+clip(name,14)+": "+clip(body,27)+"\n";}
inline bool compose(const std::string& sender,const std::string& message,const std::string& existing,std::string& out){auto body=existing.substr(prefix(existing));auto result=quote(sender,message)+body;if(result.size()>120)return false;out=result;return true;}
inline bool hasBody(const std::string& text){return !compact(text.substr(prefix(text))).empty();}
inline bool selfTest(){std::string out="unchanged";
  if(!compose("Alice","Meet at the gate","",out)||out!="> Alice: Meet at the gate\n"||hasBody(out))return false;
  if(!compose("Bob","New message",out+"On my way",out)||out!="> Bob: New message\nOn my way"||!hasBody(out))return false;
  if(quote("Alice","> Bob: Older quote\nActual reply")!="> Alice: Actual reply\n")return false;
  auto longQuote=quote(std::string(80,'A'),std::string(120,'x'));if(longQuote.size()>48||prefix(longQuote)!=longQuote.size()||longQuote.find("...")==std::string::npos)return false;
  if(quote("A\r\nB","hello\tworld\nnext")!="> A B: hello world next\n")return false;
  if(compose("Alice","hello",std::string(120,'x'),out)||out!="> Bob: New message\nOn my way")return false;
  auto q=quote("Alice","hello");if(!compose("Alice","hello",std::string(120-q.size(),'x'),out)||out.size()!=120)return false;
  return prefix("ordinary text")==0&&prefix("> no newline")==0&&hasBody("hello")&&!hasBody("  ");
}
}
