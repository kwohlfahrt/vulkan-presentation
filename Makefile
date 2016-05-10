OBJECTS = info.o

rasterize : rasterize.c $(OBJECTS) rasterize.frag.spv rasterize.vert.spv
	$(CC) -std=gnu11 $< $(OBJECTS) -DVK_USE_PLATFORM_XCB_KHR -lvulkan -lxcb -o $@ -g

%.o : %.c
	$(CC) -std=gnu11 -c $< -o $@ -g

%.spv : %
	glslangValidator -V -o $@ $<

.PHONY : clean
clean :
	rm -f *.spv *.o rasterize
