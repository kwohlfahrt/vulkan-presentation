{ stdenv, gnumake, vulkan-loader, vulkan-validation-layers, vulkan-headers, glslang, libtiff }:

stdenv.mkDerivation {
  name = "vulkan-presentation";
  version = "0.0.1";
  src = ./src;

  nativeBuildInputs = [ gnumake vulkan-loader vulkan-validation-layers vulkan-headers glslang libtiff ];
}
