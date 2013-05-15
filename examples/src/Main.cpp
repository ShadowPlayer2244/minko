#include <time.h>

#include "minko/Minko.hpp"

using namespace minko::scene;
using namespace minko::math;
using namespace minko::render;
using namespace minko::render::context;

RenderingController::ptr renderingController;
clock_t start;
unsigned int numFrames = 0;
auto mesh = Node::create("mesh");
auto group = Node::create("group", {mesh});

void
renderScene()
{
  //group->controller<TransformController>()->transform()->appendTranslation(1, 0, 0);
  //mesh->controller<TransformController>()->transform()->appendRotation(.1, Vector3::yAxis());
  
  renderingController->render();

  glutPostRedisplay();

  if (numFrames % 100 == 0)
    std::cout << (float)numFrames / ((float)(clock() - start) / CLOCKS_PER_SEC) << " fps." << std::endl;
  ++numFrames;

  //std::cout << std::to_string(group->controller<TransformController>()->transform()) << std::endl;
  //std::cout << std::to_string(mesh->controller<TransformController>()->modelToWorldMatrix()) << std::endl;
}

int main(int argc, char** argv)
{
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_SINGLE);
  glutInitWindowSize(500, 500);
  glutInitWindowPosition(100, 100);
  glutCreateWindow("OpenGL - First window demo");
  glutDisplayFunc(renderScene);

  auto oglContext   = OpenGLESContext::create();
  auto camera       = Node::create("camera");
  auto root         = Node::create("root", {group, camera});

  for (auto i = 0; i < 10000; ++i)
    group->addChild(Node::create()->addController(TransformController::create()));

  camera->addController(renderingController = RenderingController::create(oglContext));

  auto shader = GLSLProgram::fromFiles(
    oglContext,
    "../shaders/Basic.vertex.glsl",
    "../shaders/Red.fragment.glsl"
  );

  auto fx = Effect::create(mesh->bindings(), {shader})
    ->bind("diffuseMaterial/rgba",          "diffuseColor")
    ->bind("transform/modelToWorldMatrix",  "modelToWorldMatrix")
    ->bind("transform/worldToScreenMatrix", "worldToScreenMatrix");

  mesh->addController(TransformController::create());
  group->addController(TransformController::create());

  std::cout << std::to_string(mesh->bindings()->getProperty<Matrix4x4::ptr>("transform/modelToWorldMatrix")) << std::endl;

  /*mesh->addController(SurfaceController::create(
    CubeGeometry::create(),
    data::DataProvider::create(),
    fx
  ));*/

  start = clock();

  //renderScene();
  glutMainLoop();

  return 0;
}
