#ifndef ZPKG_H
#define ZPKG_H

#include "../drivers/volume.h"

void zpkg_cli(volume_t* vol, const char* args);
void zap_run(volume_t* vol, const char* app_name);

#endif
