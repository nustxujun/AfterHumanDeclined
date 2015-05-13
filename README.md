# AfterHumanDeclined 
A very fast GPU-based voxelizer, request d3d11 and shader model 5.0

 ![naive rasterization](doc/cow.png)  
 ![naive rasterization](doc/sponza.png)  

--

### how to use
  
  - simple sample
 ```C++
 #include "AHD.h"

 AHD::Voxelizer voxelizer;
 
 //resource is using to create or store vertexbuffer and indexbuffer
 VoxelResource* resource = voxelizer.create();

  //original data or ID3D11Buffer are both ok
 resource->setVertex(vertexData, vertexDataSize, vertexStride);
 resource->setIndex(indexData, indexDataSize, indexStride);

 //effect is using to setup render state, every resource needs one effect
 DefaultEffect effect;
 voxelizer.addEffect(&effect);
 resource->setEffect(&effect);

 //setup the output format
 voxelizer.setUAVParameters( DXGI_FORMAT_R8G8B8A8_UNORM, 1, 4);

 //calculate and get result
 voxelizer.voxelize(result, 0, &resource);

 //release the hardware resource in effect
 voxelizer.remove(&effect);


 ```