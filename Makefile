OBJECTS = info.o tiff.o

rasterize : rasterize.c $(OBJECTS)
	$(CC) -std=gnu11 $< $(OBJECTS) -lvulkan -ltiff -o $@ -g

%.o : %.c
	$(CC) -std=gnu11 -c $< -o $@ -g

%.frag.spv : %.frag
	glslangValidator -V -o $@ $<

%.vert.spv : %.vert
	glslangValidator -V -o $@ $<

.PRECIOUS : %.frag.spv %.vert.spv
%.tif : % %.frag.spv %.vert.spv
	./$<

.PHONY : clean
clean :
	rm -f *.spv *.o *.tif rasterize
