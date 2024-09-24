# 'key-forking' extension to Xfree86 server

skip to | other docs:

history & motivation  ... <a href="http://technophobe.net/keyboards_modifiers_and_chords.html">keyboards&#44;&#32;modifiers&#32;and&#32;chords</a>
<li>  How to <a href="fork-install.html">install</a> (<a href="fork-installation.html">downloads</a>)
<li>  future <a href="fork-future.html">development</a> (w/ some  screenshots)
<li> <a href="fork-license.html">license</a> !



## Status:

   [17 jannuary 05] first attempt to get the code into xfree86 cvs
   [02 june 05] after further 4 months of "modularizing", i think i collected enough code to retry my assault in another series of mails to the mailing list. (see also medved)
   2005: I wrote a lot of emails about my reimplementation of input "pipeline"

## Motivation
   The driving force for this extension is my long-lasting interest for good interaction via keyboard. I went through a course of typing (against my will, though), and since then I started to apply the "typing without looking at keys" at computer keyboard. With years i found new ways to improve my typing ability.

   I don't have connection w/ keyboard manufactures, so I hardly could improve the physical/hardware side. But I would certainly like to, see keyboard.

## Forking


> The basic idea is: detect when you use a (letter) key as if it was a modifier key, and switch the keycode it produces to that of the real modifier key.


   > So, what is the different, when I press a modifier key? Well, sometimes I press a modifier (Hyper for example), and then think what I want to do. Or I change my mind and don't do anything and release the modifier. I want the 'forked' keys to have such do-nothing-if-i-think behaviour. That rules out repetition, though. If I press "a" for a long
   > period, and want it to do nothing, I can't generate anything, (there is no way to go back, undo).

   > So, the only way to actually write "a" is to press the key and release it quickly (which is what you do already, so nothing changes). After a timeout it becomes a modifier. And we arrive at the first timing parameter.

   > Yet worse, we produce the keypress event delayed. That means, that the letter will appear on your xterm with a small delay. But double-click is likewise, isn't it?

   > Another situation which distinguishes the modifier keys is that we press other keys while they're down. And that is exactly the other way to trigger a fork: pressing another key.

   > Here some design decisions background, and the theory on how to configure. See here for a detailed description or the algorithm.

## Implementation

   This software consists of 3 parts:
     o a patch for the XFree86 server: this introduces a new request to the X protocol: XkbSetPlugin and makes it possible to plug-in code in the key-event processing pipeline inside the X server.
     o a plugin itself. There can be more different ones, loadable at run-time, mutually exclusive. The loaded (shared library) code can be invoked with 7 calls from the X server:
         1. init function
         2. processEvent .... every keyboard event is pushed, and it is up to the code to pass them through for further processing.
         3. process requests (unknown to the X server), this way I can add new requests to the client side library without the need to recompile the X server.
         4. hook on mouse (pointer) events
         5. hook on XAllowEvents
         6. hook on "sync grab"
         7. end function

   The code uses X server's timers.

     o client-side library (to send the extended requests) and programs to configure the plugin(s).

## History (me vs keyboard)

     o DOS era: I feared the Alt/Ctrl stuff. using Fx keys was much easier, and seemed cleaner.
     o back in '96 when I started learing/using emacs, I realized that there could be a system in the 'control keys', in fact entire keymaps, prefixes.
     o I read about people moving Ctrl modifier to the caps-lock key. I try it, and after a week move it to AltGr. That starts off my systematic using of "C-f/b/p/n" combinations instead of cursor keys. I learn all other movement commands (C-a/e etc). That was after years of "C-x C-f" pain (thumb moving to the left completely). I start to notice
       some problems w/ the combinations Alt+AltGr + several letters: they don't work, they work only when I release one of the modifier keys .... after years i figure out precisely  why .
     o I start to learn XKB, and get the idea to abandon the 1st row (digits + non-alphanumeric glyphs), by using more 'groups'. I emulate the numeric keypad (the 3x3 matrix of digits) under the right hand zone: (u i o = 789, jkl, m-., space =0). Left hand is 'full' of the other glyphs. fixme: I plan to/should make a picture of it.
     o I also started to use sawfish WM (window manager) at that time, and finaly can systematically assign WM - operations to keys (before that I used fvwm). That implies a need for new modifiers, because my strategy is to avoid simply any conflict (between emacs & sawfish).
     o a long standing problem is my need to accomodate keys for outline mode command. I don't accept the idea of prefix sequence, and use combinations w/ uppercase, like M-P/N. That means pressing Meta and Shift contemporarily.

   At that time I was already using some ergonomic keyboards (still flat, though), w/ 'windows' keys. Then MS natural pro (no more flat) becomes my obsession.

     o Having improved my technique, I dream of another idea: to press more keys in parallel. Use the 'communication channel' from the brain to the computer in a wider way. But how?
     o a guy on IRC, named merriam, outlines a very simple idea. His problem was lack of sufficient number of key combinations, and uncomfort with the position of modifier keys (on the keyboard extremes). He introduces me to the idea of detecting 'rollovers' between letter keys (with some guard times) and transforming chords into events with (plain
       X) modifiers. This seems the missing piece of the puzzle to more expresiveness of the keyboard. That sparks further research. I had had no clear idea of how those combinations/chords could be delivered to applications. Now some idea is there, and the development of tools can start.

   The crucial role of tuning the right timings is noticed during a simple test with modified xev(1) sources.

   He sums up the timings in this table

     o In december 2003 I start to hack on the Xfree server sources, with the idea to implement something usable by all applications. I start to call it Forking extension. I had already some knoledge about the XKB sources due to my first excursion to fix a keyRepetitionBug.html.

   

