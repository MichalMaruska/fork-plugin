# How I enabled forking in Weston

It took a week.

## I started to look where I could interrupt the flow of key-events and insert the pluging/forking machine.

I wanted to target libinput because it does emulate middle-button (by postponing 2 mouse clicks), so
this is similar.

* I found `libinput_udev_create_context` which seems the point when Weston starts to use libinput.
Then it customizes the log priority and log-handler.

So I added         libinput_setup_fork(xxx); call.

https://github.com/MichalMaruska/weston/blob/mmc/libweston/libinput-seat.c#L373

### on the libinput side....
I found out how events are put onto a circular-buffer.

That is Weston upon select(2), makes 2 requests... 
*** to read from device nodes and populate the circular buffer
*** to get the events (from the buffer), to process.

So I needed to act before keyboard event is put into the buffer:
https://github.com/MichalMaruska/libinput/blob/mmc/src/libinput.c#L2579

But to get the plugin loaded, I needed some dlopen() and somehow register.
I got a module loading from Weston, since it's under my sight, and maybe license permits?

Result is this: weston_load_module and my  `libinput_setup_fork`
https://github.com/MichalMaruska/libinput/blob/9ad50bb391b559d37aa600a80a14c8763fe3ac74/src/libinput.c#L1940

## Create the plugin itself!!! 
So... I need something to do the registration, and create the forkingMachine.
And then the platform-abstraction class specific for libinput environment.
* https://github.com/MichalMaruska/fork-plugin/blob/clion/libinput/fork-plugin.cpp
* https://github.com/MichalMaruska/fork-plugin/blob/clion/libinput/libinput_environment.h

The plugin itself hardcoded a forking for Space key, to act as Shift. And voila!

## Activating XKB -- my XKB keybmap
To get a working weston terminal, I need much more than Space-as-shift. And certainly my XKB,
at least Control on right Alt key.

But Weston uses libxkbcommon, and only the RMLVO specs. But I use my assembled xkb file. So
* I changed Weston the use the other Api -- xkb_keymap_new_from_file():
 https://github.com/MichalMaruska/weston/blob/mmc/libweston/input.c#L4061

then I could change the hardcoded forking and got the same features as under Xorg!


