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

#include "umesh/io/ugrid32.h"
#include "umesh/io/UMesh.h"
#include "umesh/extractIsoSurface.h"

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


namespace umesh {

  void usage(const std::string error="")
  {
    if (error != "")
      std::cerr << "Error : " << error  << "\n\n";

    std::cout << "Usage: ./umeshExtractIsoSurface <in.umesh> -iso scalarValue (-o <out.umesh> | --obj file.obj)" << std::endl;;
    exit (error != "");
  };


          // = io::wholeFile::readVectorOf<float>(mappedScalarsFileName);
  std::vector<float> readFloatScalars(const std::string &fileName,
                                      vec3i brickIndex,
                                      vec3i brickCount,
                                      vec3i dims)
  {
    std::vector<float> scalars
      = io::wholeFile::readVectorOf<float>(fileName);
    if (brickCount == vec3i(1,1,1))
      return scalars;

    vec3i my_begin = (brickIndex * (dims-vec3i(1))) / brickCount;
    vec3i my_end = ((brickIndex+vec3i(1)) * (dims-vec3i(1))) / brickCount;

    std::vector<float> slice;
    for (int64_t iz=my_begin.z;iz<=my_end.z;iz++)
      for (int64_t iy=my_begin.y;iy<=my_end.y;iy++) {
        for (int64_t ix=my_begin.x;ix<=my_end.x;ix++) {
          float f = scalars[ix+iy*dims.x+iz*dims.x*dims.y];
          slice.push_back(f);
        }
      }
    return slice;
  }
  
  extern "C" int main(int ac, char **av)
  {
    float isoValue = std::numeric_limits<float>::infinity();
    std::string inFileName;
    std::string isoScalarsFileName;
    std::string mappedScalarsFileName;
    std::string outFileName;
    std::string objFileName;
    std::string npFilePrefix;

    /*! @{ for mapping when using slicing */
    vec3i brickCount(1,1,1);
    vec3i dims(1,1,1);
    vec3i brickIndex(0,0,0);
    /*! @} */
    
    /*! if enabled, we'll only save the tets that _we_ created, not
      those that were in the file initially */
    for (int i=1;i<ac;i++) {
      const std::string arg = av[i];
      if (arg == "-h")
        usage();
      else if (arg == "-o")
        outFileName = av[++i];
      else if (arg == "-o:np")
        npFilePrefix = av[++i];
      else if (arg == "-ms" || arg == "--mapped-scalars")
        mappedScalarsFileName = av[++i];
      else if (arg == "-is" || arg == "--iso-scalars")
        isoScalarsFileName = av[++i];
      else if (arg == "-iv" || arg == "--iso-value" || arg == "--iso")
        isoValue = std::stof(av[++i]);
      else if (arg == "--obj")
        objFileName = av[++i];
      else if (arg[0] != '-')
        inFileName = arg;
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
      else
        usage("unknown cmd-line arg '"+arg+"'");
    }
    
    if (inFileName == "")
      usage("no input file specified");
    if (outFileName == "" && objFileName == "" && npFilePrefix == "")
      usage("neither obj nor umesh output file specified");
    if (isoValue == std::numeric_limits<float>::infinity())
      usage("no iso-value specified");
    
    std::cout << "loading umesh from " << inFileName << std::endl;
    UMesh::SP in = UMesh::loadFrom(inFileName);
    if (!isoScalarsFileName.empty()) {
      in->perVertex = std::make_shared<Attribute>();
      in->perVertex->values
        // = io::wholeFile::readVectorOf<float>(isoScalarsFileName);
        = readFloatScalars(isoScalarsFileName,
                           brickIndex,brickCount,dims);
    }
    std::vector<float> mappedScalars;
    if (mappedScalarsFileName == ":y") {
      for (auto v : in->vertices)
        mappedScalars.push_back(v.y);
    } else if (mappedScalarsFileName == ":z") {
      for (auto v : in->vertices)
        mappedScalars.push_back(v.z);
    } else
      if (!mappedScalarsFileName.empty())
        mappedScalars
          // = io::wholeFile::readVectorOf<float>(mappedScalarsFileName);
          = readFloatScalars(mappedScalarsFileName,
                             brickIndex,brickCount,dims);

    if (mappedScalarsFileName.empty())
      std::cout << "no mapping of any scalar to egnerated iso-surface" << std::endl;
    else {
      if (mappedScalars.size() != in->perVertex->values.size())
        throw std::runtime_error("mapped scalars size doesn't match per vertex scalars.size");
    }
    if (in->vertices.size() != in->perVertex->values.size())
      throw std::runtime_error("vertices size doesn't match per vertex scalars.size");

    std::cout << "done loading, found " << in->toString()
              << " ... now extracting iso-surface" << std::endl;
    if (in->pyrs.empty() &&
        in->wedges.empty() &&
        in->hexes.empty()) {
      std::cout << UMESH_TERMINAL_RED << std::endl;
      std::cout << "*******************************************************" << std::endl;
      std::cout << "WARNING: umesh already contains only tets..." << std::endl;
      std::cout << "*******************************************************" << std::endl;
      std::cout << UMESH_TERMINAL_DEFAULT << std::endl;
    }
    
    UMesh::SP result = extractIsoSurface(in,isoValue,mappedScalars);
    std::cout << "done extracting isovalue, found " << result->toString() << std::endl;
    if (npFilePrefix != "") {
      std::cout << "writing in raw numpy arrays format to " << npFilePrefix
                << "{.vertex_coords.f3,.vertex_scalar.f1,.triangle_indices.i3}" << std::endl;
      if (!in->perVertex)
        throw std::runtime_error("no color mapping variable specified");
      std::cout << UMESH_TERMINAL_RED << "# WARNING - this can take a while!"
                << UMESH_TERMINAL_DEFAULT << std::endl;
      std::ofstream vertices(npFilePrefix+".vertex_coords.f3");
      std::ofstream scalars(npFilePrefix+".vertex_scalars.f1");
      std::ofstream indices(npFilePrefix+".triangle_indices.i3");
      for (auto t : result->triangles) 
        indices.write((const char *)&t,3*sizeof(int));
      for (int i=0;i<result->vertices.size();i++) {
        vec3f v = result->vertices[i];
        vertices.write((const char *)&v,sizeof(v));
        float f = in->perVertex->values[i];
        scalars.write((const char *)&f,sizeof(f));
      }
    }
    if (outFileName != "") {
      std::cout << "saving to " << outFileName << std::endl;
      result->finalize();
      result->saveTo(outFileName);
    }
    if (objFileName != "") {
      std::cout << "writing in OBJ format to " << objFileName << std::endl;
      std::cout << UMESH_TERMINAL_RED << "# WARNING - this can take a while!"
                << UMESH_TERMINAL_DEFAULT << std::endl;
      std::ofstream out(objFileName);
      for (auto v : result->vertices)
        out << "v " << v.x << " " << v.y << " " << v.z << std::endl;
      for (auto t : result->triangles)
        out << "f " << (t.x+1) << " " << (t.y+1) << " " << (t.z+1) << std::endl;
    }
      
    std::cout << "done all ..." << std::endl;
    
  }
} // ::umesh
