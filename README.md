lavalamp-js
===========

I wanted to try out [Emscripten](https://github.com/kripken/emscripten), which compiles C++ to JavaScript, so I decided to port this lavalamp graphics effect I made many years ago as a test.

[You can see it in action here.](http://vector.io/lavalamp/lavalamp.html)

`lavalamp.cpp` is the source, Emscripten creates `lavalamp.js`, which is wrapped and connected to a `canvas` in `lavalamp.html`.

To compile (using emcc, the Emscripten command-line front-end):

`emcc -O2 -s ASM_JS=1 lavalamp.cpp -o lavalamp.js`
