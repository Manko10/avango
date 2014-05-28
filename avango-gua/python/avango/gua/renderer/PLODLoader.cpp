#include "PLODLoader.hpp"

#include <boost/python.hpp>
#include <avango/python/register_field.h>
#include <avango/gua/renderer/PLODLoader.hpp>

using namespace boost::python;
using namespace av::python;

namespace boost
 {
  namespace python
   {
    template <class T> struct pointee<av::Link<T> >
     {
      typedef T type;
     };
   }
 }

av::Link<av::gua::Node> load(
    av::gua::PLODLoader const& loader,
    std::string const& nodename,
    std::string const& fileName) {
   return loader.load(nodename, fileName);
}

bool is_supported(av::gua::PLODLoader const& loader, std::string const& file) {
   return loader.is_supported(file);
}

void init_PLODLoader()
{
  class_<av::gua::PLODLoader,
         av::Link<av::gua::PLODLoader>,
         bases<av::FieldContainer>, boost::noncopyable> ("PLODLoader", "docstring", no_init)
         .def("load", &load)
         .def("create_geometry_from_file", &load)
         .def("is_supported", &is_supported)
         ;

  register_field<av::gua::SFPLODLoader>("SFPLODLoader");
  register_multifield<av::gua::MFPLODLoader>("MFPLODLoader");
}
