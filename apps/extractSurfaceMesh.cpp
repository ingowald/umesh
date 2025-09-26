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

/* no computations at all - just dumps the mesh that's implicit in the
   input umesh, and dumps it in obj format (other prims get
   ignored) */ 

#include "umesh/io/ugrid64.h"
#include "umesh/io/UMesh.h"
#include "umesh/io/IO.h"
#include "umesh/RemeshHelper.h"
#include "umesh/extractSurfaceMesh.h"
#include <algorithm>

namespace umesh {

  typedef enum { INVALID, UMESH, OBJ, HSMESH } Format;

  Format formatFromFileName(const std::string &fileName)
  {
    if (fileName.substr(fileName.size()-4) == ".obj") return OBJ;
    if (fileName.substr(fileName.size()-6) == ".umesh") return UMESH;
    if (fileName.substr(fileName.size()-7) == ".hsmesh") return HSMESH;
    return INVALID;
  }
  
  void saveToOBJ(const std::string &outFileName, UMesh::SP mesh)
  {
    std::cout << "... saving (in OBJ format) to " << outFileName << std::endl;
    std::ofstream out(outFileName);
    for (auto vtx : mesh->vertices)
      out << "v " << vtx.x << " " << vtx.y << " " << vtx.z << std::endl;
    for (auto idx : mesh->triangles)
      out << "f " << (idx.x+1) << " " << (idx.y+1) << " " << (idx.z+1) << std::endl;
    std::cout << "... done" << std::endl;
  }

  void saveToHSMESH(const std::string &outFileName, UMesh::SP mesh)
  {
    std::cout << "... saving (in HSMESH format) to " << outFileName << std::endl;
    std::ofstream out(outFileName);

    io::writeVector(out,mesh->vertices);
    std::vector<vec3f> noNormals;
    io::writeVector(out,noNormals);
    std::vector<vec3f> noColors;
    io::writeVector(out,noColors);

    std::vector<vec3i> indices;
    for (auto tri : mesh->triangles)
      indices.push_back({tri.x,tri.y,tri.z});
    for (auto quad : mesh->quads) {
      indices.push_back({quad.x,quad.y,quad.z});
      indices.push_back({quad.x,quad.z,quad.w});
    }
    io::writeVector(out,indices);
    io::writeVector(out,mesh->perVertex->values);
  }
  
  UMesh::SP load(const std::string &fileName)
  {
    if (fileName.substr(fileName.size()-6) == ".umesh")
      return UMesh::loadFrom(fileName);
    
    if (fileName.substr(fileName.size()-8) == ".ugrid64")
      return io::UGrid64Loader::load(fileName);

    throw std::runtime_error("could not determine input format"
                             " (only supporting ugrid64 or umesh for now)");
  }
  
  extern "C" int main(int ac, char **av)
  {
    try {
      std::string inFileName;
      std::string outFileName;
      Format format = INVALID;

      for (int i = 1; i < ac; i++) {
        const std::string arg = av[i];
        if (arg == "-o")
          outFileName = av[++i];
        else if (arg == "--obj")
          format = OBJ;
        else if (arg == "--hsmesh")
          format = HSMESH;
        else if (arg == "--umesh")
          format = UMESH;
        else if (arg[0] != '-')
          inFileName = arg;
        else {
          throw std::runtime_error("./umeshDumpSurfaceMesh <in.umesh> [--obj|--umesh|--hsmesh] -o <out.obj|.hsmesh|.umesh>");
        }
      }

      if (format == INVALID)
        format = formatFromFileName(outFileName);
      
      std::cout << "loading umesh from " << inFileName << std::endl;
      UMesh::SP inMesh = load(inFileName);
      if (inMesh->triangles.empty() &&
          inMesh->quads.empty())
        throw std::runtime_error("umesh does not contain any surface elements...");

      UMesh::SP outMesh = extractSurfaceMesh(inMesh);

      std::cout << "extracted surface of " << outMesh->toString() << std::endl;
      switch (format) {
      case OBJ:
        saveToOBJ(outFileName,outMesh);
        break;
      case HSMESH:
        saveToHSMESH(outFileName,outMesh);
        break;
      case UMESH:
        outMesh->saveTo(outFileName);
        break;
      default:
        throw std::runtime_error("invalid/unsupported format!?");
      }
    }
    catch (std::exception &e) {
      std::cerr << "fatal error " << e.what() << std::endl;
      exit(1);
    }
    return 0;
  }  
} // ::umesh
