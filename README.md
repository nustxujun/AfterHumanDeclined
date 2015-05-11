# AfterHumanDeclined 
A very fast GPU-based voxelizer, request d3d11 and shader model 5.0

 ![naive rasterization](doc/preview.png)  

### how to use
  
  - simple sample
 ```C++
 #include "AHD.h"

 AHD::Voxelizer voxelizer;
 
 //need resource to save vertexbuffer and indexbuffer
 VoxelResource* resource = voxelizer.create();

 //original data or ID3D11Buffer are both ok
 resource->setVertex(vertexData, vertexDataSize, vertexStride);
 resource->setIndex(indexData, indexDataSize, indexStride);

 //calculate
 voxelizer.voxelize(resource);

 //get result
 voxelizer.exportVoxels(result);

 ```