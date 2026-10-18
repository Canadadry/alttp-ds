#include <stddef.h>
#include <stdint.h>
#include "spc_player.h"
__attribute__((used)) const unsigned ram_off = offsetof(SpcPlayer, ram);
__attribute__((used)) const unsigned ch_off = offsetof(SpcPlayer, channel);
