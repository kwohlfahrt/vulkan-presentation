OBJECTS = info.o tiff.o util.o vkutil.o

%.bin : %.c $(OBJECTS)
	$(CC) -std=gnu11 $^ -lvulkan -ltiff -o $@ -g

%.o : %.c
	$(CC) -std=gnu11 -c $< -o $@ -g

%.frag.spv : %.frag
	glslangValidator -V -o $@ $<

%.vert.spv : %.vert
	glslangValidator -V -o $@ $<

%.geom.spv : %.geom
	glslangValidator -V -o $@ $<

BLEND_IMAGES = $(foreach img,no-z-test z-test add,$(subst IMG,$(img),blend_IMG.tif))
$(BLEND_IMAGES) : blend.bin blend.frag.spv blend.vert.spv
	./$<

.PRECIOUS : %.frag.spv %.vert.spv %.geom.spv %.bin %.o
.SECONDEXPANSION:
%.tif : %.bin %.frag.spv %.vert.spv $$(addsuffix .spv,$$(wildcard $$*.geom))
	./$<

cube.tif : cube.bin cube.vert.spv cube.geom.spv cube.frag.spv wireframe.geom.spv device_coords.frag.spv
	./$<

.PHONY : clean
clean :
	rm -f *.spv *.o *.tif *.bin

.PHONY : all
all : rasterize.tif device_coords.tif vertex_shader.tif fill.tif texture.tif cube.tif $(BLEND_IMAGES)
