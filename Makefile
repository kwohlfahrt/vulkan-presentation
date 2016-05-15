OBJECTS = info.o tiff.o util.o vkutil.o

%.bin : %.c $(OBJECTS)
	$(CC) -std=gnu11 $^ -lvulkan -ltiff -o $@ -g

%.o : %.c
	$(CC) -std=gnu11 -c $< -o $@ -g

%.frag.spv : %.frag
	glslangValidator -V -o $@ $<

%.vert.spv : %.vert
	glslangValidator -V -o $@ $<

BLEND_IMAGES = $(foreach img,no-z-test z-test add,$(subst IMG,$(img),blend_IMG.tif))
$(BLEND_IMAGES) : blend.bin blend.frag.spv blend.vert.spv
	./$<

.PRECIOUS : %.frag.spv %.vert.spv %.bin %.o
%.tif : %.bin %.frag.spv %.vert.spv
	./$<

.PHONY : clean
clean :
	rm -f *.spv *.o *.tif *.bin

.PHONY : all
all : rasterize.tif device_coords.tif vertex_shader.tif fill.tif texture.tif $(BLEND_IMAGES)
