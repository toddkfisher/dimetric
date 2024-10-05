#clang dimetric.c -o gdim -lm `pkg-config --cflags --libs gtk+-3.0` 
#clang compositing.c -o comp -lm `pkg-config --cflags --libs gtk+-3.0`
#clang mask.c -o msk -lm `pkg-config --cflags --libs gtk+-3.0`
clang xlib-dimetric.c -o xdim -lm -lX11 -lcairo
