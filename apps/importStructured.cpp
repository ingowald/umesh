// ======================================================================== //
// Copyright 2018-2020 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "umesh/io/UMesh.h"
#include "umesh/tetrahedralize.h"

namespace umesh {

#ifndef PRINT
#ifdef __CUDA_ARCH__
# define PRINT(va) /**/
# define PING /**/
#else
# define PRINT(var) std::cout << #var << "=" << var << std::endl;
#ifdef __WIN32__
# define PING std::cout << __FILE__ << "::" << __LINE__ << ": " << __FUNCTION__ << std::endl;
#else
# define PING std::cout << __FILE__ << "::" << __LINE__ << ": " << __PRETTY_FUNCTION__ << std::endl;
#endif
#endif
#endif

  void usage(const std::string error="")
  {
    if (error != "")
      std::cerr << "Error : " << error  << "\n\n";
    
    std::cout << "Usage: ./umeshImportUGrid64 <in.ugrid64> <scalarsFile.bin> -o <out.umesh>" << std::endl;;
    std::cout << "  --doubles : input vertices are in double precision" << std::endl;
    exit (error != "");
  };

  std::vector<float> load(const std::string &fileName,
                          const std::string &format,
                          vec3i dims)
  {
    size_t count = size_t(dims.x)*size_t(dims.y)*dims.z;
    std::ifstream in(fileName.c_str(),std::ios::binary);
    std::vector<float> floats(count);
    if (format == "float") {
      in.read((char *)floats.data(),count*sizeof(float));
    } 
    else if (format == "uint8") {
      std::cout << "reading uint8's" << std::endl;
      std::vector<uint8_t> bytes(count);
      in.read((char *)bytes.data(),count);
      for (size_t i=0;i<count;i++)
        floats[i] = bytes[i]/255.f;
    }
    else
      throw std::runtime_error("unsupported format '"+format+"'");
    return floats;
  }
  

  extern "C" int main(int ac, char **av)
  {
    std::string rawFileName;
    std::string outFileName;
    /*! if enabled, we'll only save the tets that _we_ created, not
        those that were in the file initially */
    bool skipActualTets = false;

    vec3i brickCount(1,1,1);
    vec3i dims(1,1,1);
    vec3i brickIndex(0,0,0);
    std::string format = "float";
    for (int i=1;i<ac;i++) {
      const std::string arg = av[i];
      if (arg == "-h")
        usage();
      else if (arg == "-o")
        outFileName = av[++i];
      else if (arg == "-if" || arg == "--input-format" || arg == "-f") {
        format = av[++i];
      }
      else if (arg == "-id" || arg == "--input-dims" || arg == "-dims") {
        dims.x = std::stoi(av[++i]);
        dims.y = std::stoi(av[++i]);
        dims.z = std::stoi(av[++i]);
      }
      else if (arg == "--brick-index") {
        brickIndex.x = std::stoi(av[++i]);
        brickIndex.y = std::stoi(av[++i]);
        brickIndex.z = std::stoi(av[++i]);
      }
      else if (arg == "--brick-count") {
        brickCount.x = std::stoi(av[++i]);
        brickCount.y = std::stoi(av[++i]);
        brickCount.z = std::stoi(av[++i]);
      }
      else if (arg[0] != '-') {
        rawFileName = arg;
      } else
        usage("unknown cmd-line arg '"+arg+"'");
    }
    
    if (rawFileName == "") usage("no input raw file specified");
    if (outFileName == "") usage("no output file specified");
    std::vector<float> scalars = load(rawFileName,format,dims);
    std::cout << "done loading raw scalars" << std::endl;

    vec3i my_begin = (brickIndex * (dims-vec3i(1))) / brickCount;
    vec3i my_end = ((brickIndex+vec3i(1)) * (dims-vec3i(1))) / brickCount;

    PRINT(my_begin);
    PRINT(my_end);
    UMesh::SP umesh = std::make_shared<UMesh>();
    umesh->perVertex = std::make_shared<Attribute>();
    std::cout << "generating vertices" << std::endl;
    for (int64_t iz=my_begin.z;iz<=my_end.z;iz++)
      for (int64_t iy=my_begin.y;iy<=my_end.y;iy++) {
        for (int64_t ix=my_begin.x;ix<=my_end.x;ix++) {
          vec3f v;
          v.x = ix;
          v.y = iy;
          v.z = iz;
          umesh->vertices.push_back(v);
          float f = scalars[ix+iy*dims.x+iz*dims.x*dims.y];
          umesh->perVertex->values.push_back(f);
        }
      }
    std::cout << "generating hexes" << std::endl;
    vec3i my_dims = my_end - my_begin + 1;
    PRINT(my_dims.x*my_dims.y*my_dims.z);
    PRINT(umesh->vertices.size());
    for (int64_t iz=0;iz<my_dims.z-1;iz++)
      for (int64_t iy=0;iy<my_dims.y-1;iy++)
        for (int64_t ix=0;ix<my_dims.x-1;ix++) {
          int64_t i000 = (ix+0)+(iy+0)*my_dims.x+(iz+0)*my_dims.x*my_dims.y;
          int64_t i001 = (ix+1)+(iy+0)*my_dims.x+(iz+0)*my_dims.x*my_dims.y;
          int64_t i010 = (ix+0)+(iy+1)*my_dims.x+(iz+0)*my_dims.x*my_dims.y;
          int64_t i011 = (ix+1)+(iy+1)*my_dims.x+(iz+0)*my_dims.x*my_dims.y;
          int64_t i100 = (ix+0)+(iy+0)*my_dims.x+(iz+1)*my_dims.x*my_dims.y;
          int64_t i101 = (ix+1)+(iy+0)*my_dims.x+(iz+1)*my_dims.x*my_dims.y;
          int64_t i110 = (ix+0)+(iy+1)*my_dims.x+(iz+1)*my_dims.x*my_dims.y;
          int64_t i111 = (ix+1)+(iy+1)*my_dims.x+(iz+1)*my_dims.x*my_dims.y;

          UMesh::Hex hex;
          hex[0] = i000;
          hex[1] = i001;
          hex[2] = i011;
          hex[3] = i010;
          hex[4] = i100;
          hex[5] = i101;
          hex[6] = i111;
          hex[7] = i110;
          umesh->hexes.push_back(hex);
        }
    umesh->finalize();
    umesh->saveTo(outFileName);
    std::cout << "done ..." << std::endl;
  }
} // ::umesh
