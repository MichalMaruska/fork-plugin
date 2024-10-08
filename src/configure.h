#ifndef CONFIGURE_H
#define CONFIGURE_H

extern "C" {
#include <xorg/inputstr.h>
}

int machine_configure_get(PluginInstance* plugin, int values[5], int return_config[3]);
int machine_configure(PluginInstance* plugin, int values[5]);
void machine_command(ClientPtr client, PluginInstance* plugin, int cmd, int data1,
                     int data2, int data3, int data4);

#endif
