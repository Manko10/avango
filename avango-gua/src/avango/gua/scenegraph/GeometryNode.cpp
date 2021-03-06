#include <avango/gua/scenegraph/GeometryNode.hpp>
#include <avango/Base.h>
#include <boost/bind.hpp>

AV_FC_DEFINE_ABSTRACT(av::gua::GeometryNode);

AV_FIELD_DEFINE(av::gua::SFGeometryNode);
AV_FIELD_DEFINE(av::gua::MFGeometryNode);

av::gua::GeometryNode::GeometryNode(std::shared_ptr< ::gua::node::GeometryNode> guanode)
  : Node(guanode),
    m_guaNode(std::dynamic_pointer_cast< ::gua::node::GeometryNode>(Node::getGuaNode()))
{
  AV_FC_ADD_ADAPTOR_FIELD(Geometry,
                        boost::bind(&GeometryNode::getGeometryCB, this, _1),
                        boost::bind(&GeometryNode::setGeometryCB, this, _1));

  AV_FC_ADD_ADAPTOR_FIELD(Material,
                        boost::bind(&GeometryNode::getMaterialCB, this, _1),
                        boost::bind(&GeometryNode::setMaterialCB, this, _1));

  AV_FC_ADD_ADAPTOR_FIELD(ShadowMode,
                        boost::bind(&GeometryNode::getShadowModeCB, this, _1),
                        boost::bind(&GeometryNode::setShadowModeCB, this, _1));
}

void
av::gua::GeometryNode::initClass()
{
  if (!isTypeInitialized()) {
    av::gua::Node::initClass();

    AV_FC_INIT_ABSTRACT(av::gua::Node, av::gua::GeometryNode, true);

    SFGeometryNode::initClass("av::gua::SFGeometryNode", "av::Field");
    MFGeometryNode::initClass("av::gua::MFGeometryNode", "av::Field");

    //sClassTypeId.setDistributable(true);
  }
}

std::shared_ptr< ::gua::node::GeometryNode>
av::gua::GeometryNode::getGuaNode() const
{
  return m_guaNode;
}

void
av::gua::GeometryNode::getGeometryCB(const SFString::GetValueEvent& event)
{
  *(event.getValuePtr()) = m_guaNode->get_filename();
}

void
av::gua::GeometryNode::setGeometryCB(const SFString::SetValueEvent& event)
{
  m_guaNode->set_filename(event.getValue());
}

void
av::gua::GeometryNode::getMaterialCB(const SFString::GetValueEvent& event)
{
  *(event.getValuePtr()) = m_guaNode->get_material();
}

void
av::gua::GeometryNode::setMaterialCB(const SFString::SetValueEvent& event)
{
  m_guaNode->set_material(event.getValue());
}

void
av::gua::GeometryNode::getShadowModeCB(const SFUInt::GetValueEvent& event)
{
  *(event.getValuePtr()) = static_cast<unsigned>(m_guaNode->get_shadow_mode());
}

void
av::gua::GeometryNode::setShadowModeCB(const SFUInt::SetValueEvent& event)
{
  m_guaNode->set_shadow_mode(static_cast< ::gua::ShadowMode>(event.getValue()));
}
