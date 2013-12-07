#ifndef AVANGO_GUA_VOLUME_NODE_HPP
#define AVANGO_GUA_VOLUME_NODE_HPP

/**
 * \file
 * \ingroup av_gua
 */

#include <gua/scenegraph/VolumeNode.hpp>
#include <gua/math/math.hpp>

#include <avango/gua/scenegraph/Node.hpp>

namespace av
{
  namespace gua
  {
    /**
     * Wrapper for ::gua::VolumeNode
     *
     * \ingroup av_gua
     */
    class AV_GUA_DLL VolumeNode : public av::gua::Node
    {
      AV_FC_DECLARE();

    public:

      /**
       * Constructor. When called without arguments, a new ::gua::VolumeNode is created.
       * Otherwise, the given ::gua::VolumeNode is used.
       */
      VolumeNode(std::shared_ptr< ::gua::VolumeNode> guanode = std::shared_ptr< ::gua::VolumeNode>(new ::gua::VolumeNode("")));

    protected:

      /**
       * Destructor made protected to prevent allocation on stack.
       */
//      virtual ~VolumeNode();

    public:


      //SFString Geometry;

      /**
       * Get the wrapped ::gua::VolumeNode.
       */
      std::shared_ptr< ::gua::VolumeNode> getGuaNode() const;

    public:

      //virtual void getGeometryCB(const SFString::GetValueEvent& event);
      //virtual void setGeometryCB(const SFString::SetValueEvent& event);

    private:

      std::shared_ptr< ::gua::VolumeNode> m_guaNode;

      VolumeNode(const VolumeNode&);
      VolumeNode& operator=(const VolumeNode&);
    };

    typedef SingleField<Link<VolumeNode> > SFVolumeNode;
    typedef MultiField<Link<VolumeNode> > MFVolumeNode;

  }

#ifdef AV_INSTANTIATE_FIELD_TEMPLATES
  template class AV_GUA_DLL SingleField<Link<gua::VolumeNode> >;
  template class AV_GUA_DLL MultiField<Link<gua::VolumeNode> >;
#endif

}

#endif //AVANGO_GUA_VOLUME_NODE_HPP