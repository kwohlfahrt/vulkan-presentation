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

# Use pattern rule to correctly make multiple files
BLEND_VARS = no-z-test z-test add
BLEND_IMAGES = $(foreach img,$(BLEND_VARS),$(subst IMG,$(img),blend_IMG.tif))
BLEND_TARGETS = $(foreach img,$(BLEND_VARS),$(subst IMG,$(img),blend%IMG.tif))
$(BLEND_TARGETS) : blend.bin blend.frag.spv blend.vert.spv
	./$<

CUBE_VARS = persp ortho
CUBE_IMAGES = $(foreach img,$(CUBE_VARS),$(subst IMG,$(img),cube_IMG.tif))
CUBE_TARGETS = $(foreach img,$(CUBE_VARS),$(subst IMG,$(img),cube%IMG.tif))
$(CUBE_TARGETS) : cube.bin cube.vert.spv cube.geom.spv cube.frag.spv wireframe.geom.spv device_coords.frag.spv
	./$<

LIGHTING_VARS = color normal
LIGHTING_IMAGES = $(foreach img,$(LIGHTING_VARS),$(subst IMG,$(img),lighting_IMG.tif))
LIGHTING_TARGETS = $(foreach img,$(LIGHTING_VARS),$(subst IMG,$(img),lighting%IMG.tif))
$(LIGHTING_TARGETS) : lighting.bin lighting.vert.spv lighting.geom.spv lighting.frag.spv
	./$<

.PRECIOUS : %.frag.spv %.vert.spv %.geom.spv %.bin %.o
.SECONDEXPANSION:
%.tif : %.bin %.frag.spv %.vert.spv $$(addsuffix .spv,$$(wildcard $$*.geom))
	./$<

.PHONY : clean
clean :
	rm -f *.spv *.o *.tif *.bin

.PHONY : all
all : rasterize.tif device_coords.tif vertex_shader.tif fill.tif texture.tif $(BLEND_IMAGES) $(CUBE_IMAGES) $(LIGHTING_IMAGES)
