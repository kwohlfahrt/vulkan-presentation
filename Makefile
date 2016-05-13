OBJECTS = info.o tiff.o util.o vkutil.o

%.bin : %.c $(OBJECTS)
	$(CC) -std=gnu11 $^ -lvulkan -ltiff -o $@ -g

%.o : %.c
	$(CC) -std=gnu11 -c $< -o $@ -g

%.frag.spv : %.frag
	glslangValidator -V -o $@ $<

%.vert.spv : %.vert
	glslangValidator -V -o $@ $<

.PRECIOUS : %.frag.spv %.vert.spv %.bin %.o
%.tif : %.bin %.frag.spv %.vert.spv
	./$<

%.ogg : %.bin %.frag.spv %.vert.spv
	./$< && ffmpeg -i $(basename $@)%03d.tif -q:v 9 -y $@
	rm -f $(basename $@)???.tif

.PHONY : clean
clean :
	rm -f *.spv *.o *.tif *.ogg *.bin

.PHONY : all
all : rasterize.tif device_coords.tif vertex_shader.ogg fragment_shader.ogg
