#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/camera.h>
#include <learnopengl/filesystem.h>
#include <learnopengl/model.h>
#include <learnopengl/shader_m.h>

#include <my/car.h>
#include <my/fixed_camera.h>

#include <iostream>

#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "assimp.lib")


// function declaration
GLFWwindow* windowInit();
bool init();
void depthMapFBOInit();
void skyboxInit();

void setDeltaTime();
void changeLightPosAsTime();
void updateFixedCamera();

// use "&" for better performance
void renderLight(Shader& shader);
void renderCarAndCamera(Model& carModel, Model& cameraModel, Shader& shader);
void renderCar(Model& model, glm::mat4 modelMatrix, Shader& shader);
void renderCamera(Model& model, glm::mat4 modelMatrix, Shader& shader);
void renderStopSign(Model& model, Shader& shader);
void renderRaceTrack(Model& model, Shader& shader);
void renderSkyBox(Shader& shader);

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void handleKeyInput(GLFWwindow* window);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

unsigned int loadCubemap(vector<std::string> faces);

// ------------------------------------------
// global variable
// ------------------------------------------

// window size
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Window resolution when rendering shadows (affects the jaggedness of shadows)
const unsigned int SHADOW_WIDTH = 1024 * 10;
const unsigned int SHADOW_HEIGHT = 1024 * 10;

// Whether it is in wireframe mode
bool isPolygonMode = false;

// Y-axis unit vector of the world coordinate system
glm::vec3 WORLD_UP(0.0f, 1.0f, 0.0f);

// car
Car car(glm::vec3(0.0f, 0.05f, 0.0f));

// camera
glm::vec3 cameraPos(0.0f, 2.0f, 5.0f);
Camera camera(cameraPos);
FixedCamera fixedCamera(cameraPos);
bool isCameraFixed = false;

// Lighting related properties
glm::vec3 lightPos(-1.0f, 1.0f, -1.0f);
glm::vec3 lightDirection = glm::normalize(lightPos);
glm::mat4 lightSpaceMatrix;

// ID of the depth map
unsigned int depthMap;
unsigned int depthMapFBO;

// Set the mouse to the center of the screen
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing is used to balance the speed changes caused by different computer rendering levels
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// skybox
unsigned int cubemapTexture;
unsigned int skyboxVAO, skyboxVBO;


// skybox vertex data
const float skyboxVertices[] = {
    // positions
    -1.0f, 1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f, 1.0f, -1.0f,
    -1.0f, 1.0f, -1.0f,

    -1.0f, -1.0f, 1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f, 1.0f, -1.0f,
    -1.0f, 1.0f, -1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f, -1.0f, 1.0f,

    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, -1.0f, 1.0f,
    -1.0f, -1.0f, 1.0f,

    -1.0f, 1.0f, -1.0f,
    1.0f, 1.0f, -1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f, 1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f, 1.0f,
    1.0f, -1.0f, 1.0f
};

// face data of the skybox
const vector<std::string> faces{
    FileSystem::getPath("asset/textures/skybox/right.tga"),
    FileSystem::getPath("asset/textures/skybox/left.tga"),
    FileSystem::getPath("asset/textures/skybox/top.tga"),
    FileSystem::getPath("asset/textures/skybox/bottom.tga"),
    FileSystem::getPath("asset/textures/skybox/front.tga"),
    FileSystem::getPath("asset/textures/skybox/back.tga")
};

// ------------------------------------------
//main function
// ------------------------------------------

int main()
{
    // ------------------------------
    //initialization
    // ------------------------------

    // window initialization
    GLFWwindow* window = windowInit();

    //OpenGL initialization
    bool isInit = init();
    if (window == NULL || !isInit) {
        return -1;
    }
    // FBO configuration of Depth Map
    depthMapFBOInit();
    // skybox configuration
    skyboxInit();

    // ------------------------------
     // build and compile the shader
     // ------------------------------

     // shader that adds lighting and shadows to all objects
    Shader shader("shader/light_and_shadow.vs", "shader/light_and_shadow.fs");
    // A shader that generates depth information from the angle of the sun's parallel light
    Shader depthShader("shader/shadow_mapping_depth.vs", "shader/shadow_mapping_depth.fs");
    // skybox shader
    Shader skyboxShader("shader/skybox.vs", "shader/skybox.fs");

    // ------------------------------
    // model loading
    // -------------------------------

    // car model
    Model carModel(FileSystem::getPath("asset/models/obj/Lamborghini/Lamborghini.obj"));

    // camera model
    Model cameraModel(FileSystem::getPath("asset/models/obj/camera-cube/camera-cube.obj"));

    // track model
    Model raceTrackModel(FileSystem::getPath("asset/models/obj/race-track/race-track.obj"));

    // STOP card model
    Model stopSignModel(FileSystem::getPath("asset/models/obj/StopSign/StopSign.obj"));

    // ---------------------------------
      // shader texture configuration
      // ---------------------------------

    shader.use();
    shader.setInt("diffuseTexture", 0);
    shader.setInt("shadowMap", 15); // The 15 here refers to "GL_TEXTURE15", which needs to correspond to the following

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);
   
