#include "bot_tools.h"
#include <cstdio>
int main(){bool ok=bot::selfTest();std::puts(ok?"Bot trigger and limits passed":"Bot checks FAILED");return ok?0:1;}
