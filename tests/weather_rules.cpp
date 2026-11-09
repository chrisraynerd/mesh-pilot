#include "weather_tools.h"
#include "diagnostics.h"
#include <cstdio>
int main(){bool ok=weather::selfTest()&&diagnostics::selfTest();std::puts(ok?"Weather matching, bounds and cache labels passed":"Weather checks FAILED");return ok?0:1;}
