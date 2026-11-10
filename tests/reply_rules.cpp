#include "reply_tools.h"
#include "text_render.h"
#include <cstdio>
int main(){bool ok=reply_tools::selfTest()&&text_render::selfTest();std::puts(ok?"Quoted replies, bounds and newline decoding passed":"Reply checks FAILED");return ok?0:1;}
