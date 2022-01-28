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
    // ---------------------------------
    // loop rendering
    // ---------------------------------


    while (!glfwWindowShouldClose(window)) {

        // Calculate the length of a frame to make the frame drawing speed even
        setDeltaTime();
        // Change light source position over time
                // changeLightPosAsTime();

                // listen for keystrokes
        handleKeyInput(window);
        // render background
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ---------------------------------
        // Render to get the depth information of the scene
        // ---------------------------------

        // Define the view volume of the light source, that is, the orthographic projection matrix of the shadow generation range
        glm::mat4 lightProjection = glm::ortho(
            -200.0f, 200.0f,
            -200.0f, 200.0f,
            -200.0f, 200.0f);

        // TODO lightPos moves with the camera position, so that shadows are always generated around the camera
        glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), WORLD_UP);
        lightSpaceMatrix = lightProjection * lightView;

        // render the entire scene from the light source
        depthShader.use();
        depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

        // Resize the viewport for depth rendering
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);

        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        // Use the depth shader to render the generated scene
        glClear(GL_DEPTH_BUFFER_BIT);
        renderCarAndCamera(carModel, cameraModel, depthShader);
        renderRaceTrack(raceTrackModel, depthShader);
        renderStopSign(stopSignModel, depthShader);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // restore viewport
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ---------------------------------
         // model rendering
         // ---------------------------------

        shader.use();

        // Set lighting related properties
        renderLight(shader);

        car.UpdateDelayYaw();
        car.UpdateDelayPosition();


        // When switching to camera fixed, you need to modify the camera state every frame
        if (isCameraFixed) {
            updateFixedCamera();
        }

        // Use shader to render car and Camera (hierarchical model)
        renderCarAndCamera(carModel, cameraModel, shader);

        // Render the Stop card
        renderStopSign(stopSignModel, shader);


        // render the track
        renderRaceTrack(raceTrackModel, shader);

        // --------------
        // Finally render the skybox

        // Change the depth test to infinity when depth equals 1.0
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        renderSkyBox(skyboxShader);
        // restore depth test
        glDepthFunc(GL_LESS);

        // swap buffers and investigate IO events (key pressed, mouse movement, etc.)
        glfwSwapBuffers(window);

        // poll for events
        glfwPollEvents();
    }

    // close glfw
    glfwTerminate();
    return 0;
}

// ------------------------------------------------------
// other functions
// ------------------------------------------------------


// ---------------------------------
// initialize
// ---------------------------------

GLFWwindow* windowInit()
{
    // initialize configuration
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


    // create window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, u8"Race car game", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        system("pause");
        return NULL;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Make GLFW capture the user's mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    return window;
}

bool init()
{
    // Load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        system("pause");
        return false;
    }

    // Configure global openGL state
    glEnable(GL_DEPTH_TEST);

    return true;
}


// depth map configuration
void depthMapFBOInit()
{
    glGenFramebuffers(1, &depthMapFBO);

    // create depth texture
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Use the generated depth texture as the depth buffer of the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// skybox configuration
void skyboxInit()
{
    // skybox VAO
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);


    // texture loading
    cubemapTexture = loadCubemap(faces);
}


// ---------------------------------
// time related function
// ---------------------------------

// Calculate the duration of a frame
void setDeltaTime()
{
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
}

void changeLightPosAsTime()
{
    float freq = 0.1;
    lightPos.x = 1.0 + cos(glfwGetTime() * freq) * -0.5f;
    lightPos.z = -1.0 + sin(glfwGetTime() * freq) * 0.5f;
    lightPos.y = 1.0 + cos(glfwGetTime() * freq) * 0.5f;
    lightDirection = glm::normalize(lightPos);
}


// ---------------------------------
// camera position update
// ---------------------------------

void updateFixedCamera()
{
    // Automatically gradually restore Zoom to default
    camera.ZoomRecover();

    // Process the vector coordinates of the camera relative to the vehicle coordinate system and convert it to a vector in the world coordinate system
    float angle = glm::radians(-car.getMidValYaw());
    glm::mat4 rotateMatrix(
        cos(angle), 0.0, sin(angle), 0.0,
        0.0, 1.0, 0.0, 0.0,
        -sin(angle), 0.0, cos(angle), 0.0,
        0.0, 0.0, 0.0, 1.0);
    glm::vec3 rotatedPosition = glm::vec3(rotateMatrix * glm::vec4(fixedCamera.getPosition(), 1.0));

    camera.FixView(rotatedPosition + car.getMidValPosition(), fixedCamera.getYaw() + car.getMidValYaw());
}
// ---------------------------------
// render function
// ---------------------------------

// Set lighting related properties
void renderLight(Shader& shader)
{
    shader.setVec3("viewPos", camera.Position);
    shader.setVec3("lightDirection", lightDirection);
    shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

    glActiveTexture(GL_TEXTURE15);
    glBindTexture(GL_TEXTURE_2D, depthMap);
}

void renderCarAndCamera(Model& carModel, Model& cameraModel, Shader& shader)
{
    // view transition
    glm::mat4 viewMatrix = camera.GetViewMatrix();
    shader.setMat4("view", viewMatrix);
    // Projection transformation
    glm::mat4 projMatrix = camera.GetProjMatrix((float)SCR_WIDTH / (float)SCR_HEIGHT);
    shader.setMat4("projection", projMatrix);

    // -------
    // Hierarchical modeling

    // model conversion
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, car.getMidValPosition());
    modelMatrix = glm::rotate(modelMatrix, glm::radians(car.getDelayYaw() / 2), WORLD_UP);

    // render the car
    renderCar(carModel, modelMatrix, shader);

    // Since mat4 is passed by value as a function parameter, there is no need to back up modelMatrix

    // render camera
    renderCamera(cameraModel, modelMatrix, shader);
}

// render the car
void renderCar(Model& model, glm::mat4 modelMatrix, Shader& shader)
{
    modelMatrix = glm::rotate(modelMatrix, glm::radians(car.getYaw() - car.getDelayYaw() / 2), WORLD_UP);
    // offset the original rotation of the model
    modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), WORLD_UP);
    // resize the model
    modelMatrix = glm::scale(modelMatrix, glm::vec3(0.004f, 0.004f, 0.004f));

    // apply transformation matrix
    shader.setMat4("model", modelMatrix);

    model.Draw(shader);
}

void renderCamera(Model& model, glm::mat4 modelMatrix, Shader& shader)
{
    modelMatrix = glm::rotate(modelMatrix, glm::radians(fixedCamera.getYaw() + car.getYaw() / 2), WORLD_UP);
    modelMatrix = glm::translate(modelMatrix, cameraPos);
    modelMatrix = glm::scale(modelMatrix, glm::vec3(0.01f, 0.01f, 0.01f));


    // apply transformation matrix
    shader.setMat4("model", modelMatrix);

    model.Draw(shader);
}

void renderStopSign(Model& model, Shader& shader)
{
    // view transition
    glm::mat4 viewMatrix = camera.GetViewMatrix();
    shader.setMat4("view", viewMatrix);

    // model conversion
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, glm::vec3(3.0f, 1.5f, -4.0f));
    modelMatrix = glm::rotate(modelMatrix, glm::radians(-120.0f), WORLD_UP);
    shader.setMat4("model", modelMatrix);
    // Projection transformation
    glm::mat4 projMatrix = camera.GetProjMatrix((float)SCR_WIDTH / (float)SCR_HEIGHT);
    shader.setMat4("projection", projMatrix);

    model.Draw(shader);
}

void renderRaceTrack(Model& model, Shader& shader)
{
    // view transition
    glm::mat4 viewMatrix = camera.GetViewMatrix();
    shader.setMat4("view", viewMatrix);
    // model conversion
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    shader.setMat4("model", modelMatrix);
   
   
