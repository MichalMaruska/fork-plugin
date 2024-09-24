# How forking interacts with auto-repetition

 Keyboard keys autorepeat. This feature^1 -- taken for granted -- is where we can 'steal'. I.e. we can use the prolonged
 key press for other semantics. Many key combinations don't even need this feature, or if, very rarely.

Examples of situation where we don't need autorepeat (AR):

* Idempotent actions (beginning of line, saving some info); or maybe (x^2 = x^3, we still prefer to press twice)
* similarly toggles
* actions which cannot run more that once in a row.

### Rules for assigning forks:

* as we activate forking on keys (w/ various modifier combinations) by giving-up their usual autorepetition, it is
  helpful to find those (keys/combinations) which don't need it anyway. This way we don't lose anything.

* Forkable keys multiplicate the number of possible inputs, so it's good to have them on easiest positions. That is the
  main line. So look at the keys "a s d f g - h j k l ?", consider their combinations with the "control-", "alt-",
  "meta-" modifiers and think: do i need repetition here? As an example take "C-f". This is a very frequently used (move)
  forward-char.

   So. This is a conflict.

possible (implemented) solutions to this conflict:

#### explicit repeat: (find some artificial way to invoke the AR)

* press the key, release and re-press quickly.
* with another special modifier key. which starts the repetition of the previously pressed (forkable) key.

####   conditional AR

we could forbid the forking (and prefer the AR) for certain combinations. At the level, where we implement the forking,
we don't know what the keySyms are bound to keycodes. So, the combination is just the set of pressed keys. We could,
for example, forbid forking of "f" when we press down the right Control key.


This idea suggests a general rule:


|when we use a fork on the right, the left hand ones are forbidden (so AR is available). And vice versa. |
|------------|


Why this? Maybe b/c it's easier to make the chord of modifiers with one hand, and then press the remaining (action) key with the other one. I think i'll have the same modifiers on both sides, and therefore there's no need to press some of them on one and some on the other side. In
   other words, the 'modifier chord' is possible with any hand alone.

#### preventing the need:

   > just move the key combinations which require AR out of the main line. As a simple example, "C-x", used heavily in emacs, doesn't need AR, so put it on main line, and put a command which requires AR under C-x.

### Suitable functions for the thumbs keys

These are orthogonal to all other keys. Pressing a thumb key doesn't limit the 'range' of any other finger. This is nice,
when we need to type continously with both hands. There are no other keys like that. So, this lends them suitable for
Shift-like functions: upcasing, maybe changing alphabet. On the other hand this can be less frequent for now, so find new
ways. This other alphabet can include putting digits, and other ascii non-letter glyphs on the letter keys (even if such
alphabet is not used for long sequences, and possibly 1 hand only is involved in a word).

Changing the modifier chord is expensive

Pressing a different modifier combination and subsequently the command key is slow. rule: different modifs = release + press + command press, same modifs = command press.

So grouping the commands likely to be used togerther, should be with the same modifier chord


^[1] The semantics of key repeation on the PC keyboard is already changed in Xfree. I.e. the hardware feature is switched off, and software method is used, which is more flexible. demo: try to press a letter, "x" on MS windows, then press "Shift", and release it. The X won't repeat
   anymore. Try on Xfree.
