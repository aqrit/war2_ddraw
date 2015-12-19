The game mixes gdi and ddraw drawing in an "unsupport" way.

DWM Desktop Composition breaks things by redirecting gdi and ddraw to different composition layers...
example: 
  step 1. draw text using gdi
  step 2. paint a red box over the text area using ddraw

after step 2 the game expects gdi, ddraw, and the user to see only a red box. 
but gdi sees only text
ddraw sees only a red box
and the user sees text over a red box.

we try to work-around this by detecting all pixels set by ddraw,
then parity those pixels on the gdi surfaces. 

D3D must be in NON-exclusive mode for
gdi drawing to be visible over the d3d surface. 
NON-exclusive mode d3d doesn't allow the resolution to be changed,
it just stretches the image to fit the display resolution.
however, all of the game's "gdi" sibling windows expect a 640x480 resolution.
so we temporarily set 640x480 using ChangeDisplaySettings 
 