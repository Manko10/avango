#include <Python.h> // has to be first in order to prevent warnings

#include <avango/gua/viewer/Viewer.hpp>
#include <avango/gua/Types.hpp>

#include <avango/Base.h>
#include <avango/Application.h>
#include <avango/Logger.h>
#include <boost/bind.hpp>

#include <chrono>

namespace
{
  av::Logger& logger(av::getLogger("av::gua::Viewer"));
}

AV_FC_DEFINE(av::gua::Viewer);

AV_FIELD_DEFINE(av::gua::SFViewer);
AV_FIELD_DEFINE(av::gua::MFViewer);

av::gua::Viewer::Viewer()
    : m_renderer(nullptr),
      m_loop(),
      m_ticker(m_loop, 1.f/60.f)
{
    AV_FC_ADD_FIELD(Pipelines, MFPipeline::ContainerType());
    AV_FC_ADD_FIELD(SceneGraphs, MFSceneGraph::ContainerType());
#if defined(AVANGO_PHYSICS_SUPPORT)
    AV_FC_ADD_FIELD(Physics, nullptr);
#endif

    AV_FC_ADD_ADAPTOR_FIELD(DesiredFPS,
                    boost::bind(&Viewer::getDesiredFPSCB, this, _1),
                    boost::bind(&Viewer::setDesiredFPSCB, this, _1));
}


//av::gua::Viewer::~Viewer()
//{}

void
av::gua::Viewer::initClass()
{
    if (!isTypeInitialized())
    {
        av::FieldContainer::initClass();

        AV_FC_INIT(av::FieldContainer, av::gua::Viewer, true);

        SFViewer::initClass("av::gua::SFViewer", "av::Field");
        MFViewer::initClass("av::gua::MFViewer", "av::Field");
    }
}

void
av::gua::Viewer::getDesiredFPSCB(const av::SFFloat::GetValueEvent& event)
{
  *(event.getValuePtr()) = 1.f/m_ticker.get_tick_time();
}

void
av::gua::Viewer::setDesiredFPSCB(const av::SFFloat::SetValueEvent& event)
{
  m_ticker.set_tick_time(1.f/event.getValue());
}

void
av::gua::Viewer::run() const {
  if (!m_renderer) {
    std::vector< ::gua::Pipeline*> pipes;

    for (auto pipe : Pipelines.getValue()) {
      pipes.push_back(pipe->getGuaPipeline());
    }
    m_renderer = new av::gua::Renderer(new ::gua::Renderer(pipes));
  }


#if defined(AVANGO_PHYSICS_SUPPORT)
  if (Physics.getValue().isValid()) {
    Physics.getValue()->State.setValue(static_cast<int>(av::gua::Physics::RunningState::RUNNING));
  }
#endif

  PyThreadState* save_state(PyEval_SaveThread());

  m_ticker.on_tick.connect([&,this]() {
    PyEval_RestoreThread(save_state);

    av::ApplicationInstance::get().evaluate();

    if (SceneGraphs.getValue().size() > 0) {
      std::vector<av::gua::SceneGraph const*> graphs;

      for (auto graph : SceneGraphs.getValue()) {
        graphs.push_back(reinterpret_cast<gua::SceneGraph*> (graph.getBasePtr()));
      }

      m_renderer->queue_draw(graphs);
    }

#if defined(AVANGO_PHYSICS_SUPPORT)
    if (this->Physics.getValue().isValid()) {
      this->Physics.getValue()->synchronize(true);
    }
#endif

    save_state = PyEval_SaveThread();

  });

  m_loop.start();

}
