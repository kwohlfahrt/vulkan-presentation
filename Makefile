OBJECTS = info.o tiff.o

rasterize : rasterize.c $(OBJECTS) rasterize.frag.spv rasterize.vert.spv
	$(CC) -std=gnu11 $< $(OBJECTS) -lvulkan -ltiff -o $@ -g

%.o : %.c
	$(CC) -std=gnu11 -c $< -o $@ -g

%.spv : %
	glslangValidator -V -o $@ $<

%.tif : %
	./$<

.PHONY : clean
clean :
	rm -f *.spv *.o *.tif rasterize
