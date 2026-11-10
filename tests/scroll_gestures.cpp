#include "scroll_tools.h"
#include "touch_tools.h"
#include <cstdio>
int main(){
  if(!scroll_tools::selfTest()||!touch_tools::selfTest()){
    std::fputs("Scroll position / hold-versus-drag fixtures FAILED\n",stderr);
    return 1;
  }
  std::puts("Scroll position / hold-versus-drag fixtures passed");
  return 0;
}
