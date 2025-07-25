//Mitrovic Andrej
//SV81-2021

#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <stdlib.h>
#include <iomanip>
#include <thread>
#include <chrono>

#include <GL/glew.h>   
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION

#include "headers/camera.h"
#include "headers/shader.h"
#include "headers/model.h"

using namespace std;

// function declarations
// ---------------------
static unsigned loadImageToTexture(const char* filePath);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window, bool alive);
void capFPS();
GLuint loadCubemap(std::vector<std::string> faces);
bool rayIntersectsSphere(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
    const glm::vec3& sphereCenter, float sphereRadius, float& t);

// Planet struct and functions
// ---------------------------
struct Planet {
    Model model;
    glm::vec3 position;
    glm::vec3 rotationAxis;
    float rotationSpeed;
    glm::vec3 scale;
    string name;
    float radius;
	bool explored = false;

    void computeRaduis() {
        radius = 0.0f;
        for (Vertex& v : model.meshes[0].vertices)
            radius = glm::max(radius, glm::distance(glm::vec3(0), v.Position) * scale.x); // Find max distance
    }
};
std::vector<Planet> planets;
std::vector<Planet> asteroids;
Planet* selectPlanet(const glm::vec3& rayOrigin, const glm::vec3& rayDirection);
float distanceToPlanet(glm::vec3 planetPos, glm::vec3 cameraPos, float radius);

// Text rendering struct
// ---------------------
struct Character {
    unsigned int TextureID;  // ID handle of the glyph texture
    glm::ivec2   Size;       // Size of glyph
    glm::ivec2   Bearing;    // Offset from baseline to left/top of glyph
    unsigned int Advance;    // Offset to advance to next glyph
};
void renderText(Shader shader, string text, float x, float y, float scale, glm::vec3 color, map<char, Character>& Characters, unsigned int VAO, unsigned int VBO);

// settings
// ---------
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

// camera
// ------
Camera camera(glm::vec3(0.0f, 0.0f, 40.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
// ------
float deltaTime = 0.0f;
float lastFrame = 0.0f;

const double targetFPS = 60.0;
const double frameDuration = 1.0 / targetFPS;
static auto lastFrameTime = chrono::high_resolution_clock::now();


int main(void)
{

    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++ INICIJALIZACIJA ++++++++++++++++++++++++++++++++++++++++++++++++++++++
    if (!glfwInit())
    {
        cout << "GLFW Biblioteka se nije ucitala! :(\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


    GLFWwindow* window;
    unsigned int wWidth = SCR_WIDTH;
    unsigned int wHeight = SCR_HEIGHT;
    const char wTitle[] = "3D NLO";
    window = glfwCreateWindow(wWidth, wHeight, wTitle, glfwGetPrimaryMonitor(), NULL);
    if (window == NULL)
    {
        cout << "Prozor nije napravljen! :(\n";
        glfwTerminate();
        return 2;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    if (glewInit() != GLEW_OK)
    {
        cout << "GLEW nije mogao da se ucita! :'(\n";
        return 3;
    }

    //kod preuzet sa https://learnopengl.com/In-Practice/Text-Rendering
    //===============================FREE TYPE LIB==============================================================
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        cout << "ERROR::FREETYPE: Could not init FreeType Library" << endl;
        return -1;
    }

    FT_Face face;
    if (FT_New_Face(ft, "fonts/WeirdScienceNbpRegular-qmBr.ttf", 0, &face))
    {
        cout << "ERROR::FREETYPE: Failed to load font" << endl;
        return -1;
    }

    FT_Set_Pixel_Sizes(face, 0, 18);

    map<char, Character> Characters;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

    for (unsigned char c = 0; c < 128; c++)
    {
        // load character glyph 
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            cout << "ERROR::FREETYTPE: Failed to load Glyph" << endl;
            continue;
        }
        // generate texture
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_ALPHA,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_ALPHA,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );
        // set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        //Linux Specific
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
       
        
        // now store character for later use
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            face->glyph->advance.x
        };
        Characters.insert(pair<char, Character>(c, character));
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // vertices
    // --------

    // skybox
	// ------
    float skyboxVertices[] = {
        // Positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    unsigned int stride = (2 + 2) * sizeof(float);

    // dashboard
    // ---------
    float vertices[] =
    {
        //Pos          //Tex
        -1.0f,  1.0f,    0.0f, 1.0f, // Top-left
        -1.0f, -1.0f,    0.0f, 0.0f, // Bottom-left
         1.0f, -1.0f,    1.0f, 0.0f, // Bottom-right
         1.0f,  1.0f,    1.0f, 1.0f  // Top-right
    };

	// signature
    // ---------
    float signatureVertices[] = {
        //Pos             //Tex
         0.33f,  -0.75f,    0.0f, 1.0f, // Top-left
         0.33f,  -0.98f,    0.0f, 0.0f, // Bottom-left
         0.63f,  -0.98f,    1.0f, 0.0f, // Bottom-right
         0.63f,  -0.75f,    1.0f, 1.0f  // Top-right
    };

    // crosshair
    // ---------
    float crosshairVertices[] = {
        //Pos          //Tex
        -0.14f,  0.20f,    0.0f, 1.0f, // Top-left
        -0.14f, -0.20f,    0.0f, 0.0f, // Bottom-left
         0.14f, -0.20f,    1.0f, 0.0f, // Bottom-right
         0.14f,  0.20f,    1.0f, 1.0f  // Top-right
    };

    unsigned int indices[] = {
    0,1,3,
    1,2,3
    };

    // generate buffers
    // ----------------
    unsigned int VAO[5];
    glGenVertexArrays(5, VAO);
    unsigned int VBO[5];
    glGenBuffers(5, VBO);
    unsigned int EBO[4];
    glGenBuffers(4, EBO);

    // dashboard
    // ---------
    glBindVertexArray(VAO[0]);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // signature
    // ---------
    glBindVertexArray(VAO[1]);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(signatureVertices), signatureVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // crosshair
    // ---------
    glBindVertexArray(VAO[2]);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[2]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(crosshairVertices), crosshairVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // overlay
    // -------
    glBindVertexArray(VAO[3]);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[3]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO[3]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // text
    // ----
    glBindVertexArray(VAO[4]);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[4]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glEnableVertexAttribArray(0);

    // skybox
    // ------
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);

    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // build and compile shaders
    // -------------------------
    Shader modelShader("shaders/model.vert", "shaders/model.frag");
	Shader skyboxShader("shaders/skybox.vert", "shaders/skybox.frag");
	Shader controlShader("shaders/dashboard.vert", "shaders/dashboard.frag");
	Shader textureShader("shaders/dashboard.vert", "shaders/texture.frag");
	Shader overlayShader("shaders/window.vert", "shaders/window.frag");
	Shader textShader("shaders/text.vert", "shaders/text.frag");

    // load models
    // -----------
    std::vector<Planet> models = {
        // Planets
        { Model("models/planets/p1/Planets.obj"),       glm::vec3(0.0f, 0.0f, -52.0f),      glm::normalize(glm::vec3(0.32f, -0.88f, 0.34f)),    0.21f,  glm::vec3(3.0f) ,   "Echo Earth"},
        { Model("models/planets/p2/Planets.obj"),       glm::vec3(34.4f, 0.0f, 9.0f),       glm::normalize(glm::vec3(-0.59f, 0.22f, 0.77f)),    0.67f,  glm::vec3(9.0f) ,   "Frost One Foxtrot"},
        { Model("models/planets/p3/Planets.obj"),       glm::vec3(0.4f, 34.5f, 0.0f),       glm::normalize(glm::vec3(0.11f, 0.98f, -0.17f)),    0.09f,  glm::vec3(3.5f) ,   "Magma Mike Zero"},
        { Model("models/planets/p4/Planets.obj"),       glm::vec3(-32.4f, -25.5f, 7.0f),    glm::normalize(glm::vec3(-0.75f, 0.43f, 0.50f)),    0.73f,  glm::vec3(4.5f) ,   "Snowstorm One"},
        { Model("models/planets/p5/Planets.obj"),       glm::vec3(8.4f, -22.5f, -12.0f),    glm::normalize(glm::vec3(0.67f, 0.36f, -0.65f)),    0.26f,  glm::vec3(3.5f) ,   "Bravo Veldt"},
        { Model("models/planets/p6/Planets.obj"),       glm::vec3(-38.4f, 20.5f, -32.0f),   glm::normalize(glm::vec3(-0.21f, -0.97f, 0.10f)),   0.58f,  glm::vec3(4.5f) ,   "Desert Delta"},
        { Model("models/planets/p7/Planets.obj"),       glm::vec3(-8.4f, 1.5f, 17.0f),      glm::normalize(glm::vec3(0.46f, -0.17f, 0.87f)),    0.83f,  glm::vec3(3.5f) ,   "Romeo Ruin"},
        { Model("models/planets/p8/Planets.obj"),       glm::vec3(13.4f, 16.5f, -29.0f),    glm::normalize(glm::vec3(-0.31f, 0.63f, -0.71f)),   0.34f,  glm::vec3(5.5f) ,   "Obsidian Veil Omega"},
        { Model("models/planets/p9/Planets.obj"),       glm::vec3(-20.4f, -3.5f, 49.0f),    glm::normalize(glm::vec3(0.81f, -0.49f, 0.31f)),    0.78f,  glm::vec3(4.0f) ,   "Tidefall Tango-12"},

        // Asteroids 
        { Model("models/asteroids/a1/Asteroids.obj"),   glm::vec3(22.0f, 9.0f, -15.5f),     glm::normalize(glm::vec3(0.53f, -0.14f, 0.84f)),    0.74f,  glm::vec3(1.0f) },
        { Model("models/asteroids/a2/Asteroids.obj"),   glm::vec3(8.0f, 10.0f, -39.0f),     glm::normalize(glm::vec3(-0.65f, 0.26f, 0.71f)),    0.59f,  glm::vec3(2.0f) },
        { Model("models/asteroids/a3/Asteroids.obj"),   glm::vec3(17.0f, 17.0f, 4.5f),      glm::normalize(glm::vec3(0.23f, 0.67f, -0.71f)),    0.91f,  glm::vec3(1.6f) },
        { Model("models/asteroids/a1/Asteroids.obj"),   glm::vec3(-16.0f, 8.0f, 3.5f),      glm::normalize(glm::vec3(-0.44f, -0.77f, 0.45f)),   0.48f,  glm::vec3(0.7f) },
        { Model("models/asteroids/a2/Asteroids.obj"),   glm::vec3(-12.0f, -20.0f, -2.0f),   glm::normalize(glm::vec3(0.28f, 0.78f, -0.37f)),    0.95f,  glm::vec3(1.0f) },
        { Model("models/asteroids/a2/Asteroids.obj"),   glm::vec3(-18.0f, 7.0f, 5.4f),      glm::normalize(glm::vec3(0.88f, 0.28f,  0.57f)),    1.45f,  glm::vec3(0.4f) },
        { Model("models/asteroids/a2/Asteroids.obj"),   glm::vec3(-15.0f, 5.0f, 5.4f),      glm::normalize(glm::vec3(-0.58f, 0.78f, -0.37f)),   1.55f,  glm::vec3(0.3f) },
        { Model("models/asteroids/a3/Asteroids.obj"),   glm::vec3(-35.0f, -2.0f, -12.0f),   glm::normalize(glm::vec3(0.24f, -0.88f, -0.41f)),   0.76f,  glm::vec3(1.6f) },
        { Model("models/asteroids/a1/Asteroids.obj"),   glm::vec3(-10.0f, 29.0f, -10.0f),   glm::normalize(glm::vec3(0.71f, -0.50f, 0.49f)),    0.38f,  glm::vec3(1.0f) },
        { Model("models/asteroids/a2/Asteroids.obj"),   glm::vec3(-12.0f, 26.0f, -11.4f),   glm::normalize(glm::vec3(-0.58f, 0.78f, -0.37f)),   1.55f,  glm::vec3(0.3f) },
        { Model("models/asteroids/a3/Asteroids.obj"),   glm::vec3(-7.0f, 26.0f, -12.0f),    glm::normalize(glm::vec3(0.24f, -0.88f, -0.41f)),   1.76f,  glm::vec3(0.4f) },
        { Model("models/asteroids/a2/Asteroids.obj"),   glm::vec3(-5.0f, 5.0f, 8.0f),       glm::normalize(glm::vec3(-0.14f, 0.93f, -0.34f)),   0.64f,  glm::vec3(1.2f) },
        { Model("models/asteroids/a2/Asteroids.obj"),   glm::vec3(0.0f, 8.0f, 5.0f),        glm::normalize(glm::vec3(-0.94f, 0.03f,  0.74f)),   1.74f,  glm::vec3(0.6f) },
        { Model("models/asteroids/a3/Asteroids.obj"),   glm::vec3(4.0f, 0.0f, 15.0f),       glm::normalize(glm::vec3(-0.63f, 0.41f, 0.65f)),    0.71f,  glm::vec3(1.0f) },
        { Model("models/asteroids/a1/Asteroids.obj"),   glm::vec3(-4.0f, 18.0f, -29.0f),    glm::normalize(glm::vec3(0.52f, -0.65f, -0.55f)),   1.92f,  glm::vec3(1.0f) },
        { Model("models/asteroids/a2/Asteroids.obj"),   glm::vec3(20.0f, -12.0f, -3.0f),    glm::normalize(glm::vec3(-0.41f, -0.16f, 0.34f)),   1.58f,  glm::vec3(0.3f) },
        { Model("models/asteroids/a2/Asteroids.obj"),   glm::vec3(23.0f, -12.0f, -2.0f),    glm::normalize(glm::vec3(0.71f, 0.56f, 0.27f)),     3.58f,  glm::vec3(0.5f) },
        { Model("models/asteroids/a2/Asteroids.obj"),   glm::vec3(20.0f, -15.0f, -1.6f),    glm::normalize(glm::vec3(-0.41f, -0.26f, 0.67f)),   4.58f,  glm::vec3(0.3f) },
        { Model("models/asteroids/a2/Asteroids.obj"),   glm::vec3(22.0f, -9.0f, -2.2f),     glm::normalize(glm::vec3(0.41f, 0.66f, -0.87f)),    0.58f,  glm::vec3(0.9f) },
        { Model("models/asteroids/a2/Asteroids.obj"),   glm::vec3(22.0f, -16.0f, -2.0f),    glm::normalize(glm::vec3(-0.91f, -0.96f, 0.87f)),   2.58f,  glm::vec3(0.4f) },
        { Model("models/asteroids/a1/Asteroids.obj"),   glm::vec3(-15.0f, -4.0f, 33.0f),    glm::normalize(glm::vec3(-0.41f, -0.16f, 0.34f)),   1.58f,  glm::vec3(0.3f) },
        { Model("models/asteroids/a3/Asteroids.obj"),   glm::vec3(-18.0f, -1.0f, 35.0f),    glm::normalize(glm::vec3(0.71f, 0.56f, 0.27f)),     3.58f,  glm::vec3(0.5f) },
        { Model("models/asteroids/a3/Asteroids.obj"),   glm::vec3(-16.0f, -6.0f, 32.6f),    glm::normalize(glm::vec3(-0.41f, -0.26f, 0.67f)),   4.58f,  glm::vec3(0.3f) },
        { Model("models/asteroids/a1/Asteroids.obj"),   glm::vec3(-14.0f, 1.0f, 36.2f),     glm::normalize(glm::vec3(0.41f, 0.66f, -0.87f)),    0.58f,  glm::vec3(0.9f) },
        { Model("models/asteroids/a2/Asteroids.obj"),   glm::vec3(-19.0f, -3.0f, 34.0f),    glm::normalize(glm::vec3(-0.91f, -0.96f, 0.87f)),   2.58f,  glm::vec3(0.4f) },
    };
	for (Planet& planet : models) {
		planet.computeRaduis();
	}
	// two seperate vectors for planets and asteroids collisions
    // ---------------------------------------------------------
    planets.assign(models.begin(), models.begin() + 9);
    asteroids.assign(models.begin() + 9, models.end());

	// load textures
	// -------------
    unsigned bottom_control_tex = loadImageToTexture("textures/bottom_control.png");
    unsigned top_control_tex = loadImageToTexture("textures/top_control.png");
    unsigned signature = loadImageToTexture("textures/signature.png");
	unsigned crosshair = loadImageToTexture("textures/crosshair.png");

	// load cubemap texture (skybox)
	// --------------------------
    std::vector<std::string> faces = {
        "textures/skybox/right.png",
        "textures/skybox/left.png",
        "textures/skybox/top.png",
        "textures/skybox/bottom.png",
        "textures/skybox/front.png",
        "textures/skybox/back.png"
    };

    // setup uniforms and mipmaps
	// --------------------------
    unsigned int cubemapTexture = loadCubemap(faces);
    skyboxShader.use();
    glUniform1i(glGetUniformLocation(skyboxShader.ID, "skybox"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bottom_control_tex);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, top_control_tex);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, signature);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, crosshair);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    controlShader.use();
    unsigned bottomTexLoc = glGetUniformLocation(controlShader.ID, "bottomTex");
    unsigned topTexLoc = glGetUniformLocation(controlShader.ID, "topTex");
    glUniform1i(bottomTexLoc, 1);
    glUniform1i(topTexLoc, 2);

	textShader.use();
    glm::mat4 projection = glm::ortho(0.0f, (float)wWidth, 0.0f, (float)wHeight);
    unsigned int projectionLoc = glGetUniformLocation(textShader.ID, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUseProgram(0);

    // draw in wireframe
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++ RENDER LOOP - PETLJA ZA CRTANJE +++++++++++++++++++++++++++++++++++++++++++++++++

    // game variables
	// --------------
    bool gameEnd = false;
    bool alive = true;
    string state = "Success";
    Planet* selected = nullptr;
    float distanceToSelected = 0.0f;

    // loop
	// -----
    while (!glfwWindowShouldClose(window)) {

        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window, alive);
       
        // reset game
		// ----------
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            camera.Position = glm::vec3(0.0f, 0.0f, 40.0f);
            alive = true;
            gameEnd = false;
            selected = nullptr;
            distanceToSelected = 0.0f;
            state = "Success";
            for (Planet& planet : planets) {
                planet.explored = false;
            }
        }

        // render
        // ------
        glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // view/projection transformations
		// --------------------------------
        modelShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f);
        glm::mat4 view = camera.GetViewMatrix();
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);
        
		// render the skybox first
        // ----------------------
        skyboxShader.use();

        glDepthFunc(GL_LEQUAL);
        glm::mat4 viewSky = glm::mat4(glm::mat3(camera.GetViewMatrix()));
        skyboxShader.setMat4("view", viewSky);
        skyboxShader.setMat4("projection", projection);
        glBindVertexArray(skyboxVAO);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        // render planets and asteroids
		// ----------------------------
        modelShader.use();
        
        for (Planet& planet : models) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, planet.position);
            model = glm::rotate(model, (float)glfwGetTime() * planet.rotationSpeed, planet.rotationAxis);
            model = glm::scale(model, planet.scale);
            modelShader.setMat4("model", model);
            planet.model.Draw(modelShader);
        }

        // depth and blend setup for overlay
        // ---------------------------------
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // crosshair
        // ---------
        textureShader.use();

        glUniform1i(glGetUniformLocation(textureShader.ID, "sampTex"), 4);
        glBindVertexArray(VAO[2]);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, crosshair);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		// overlay
		// -------
        overlayShader.use();

        glBindVertexArray(VAO[3]);
        unsigned colorLoc = glGetUniformLocation(overlayShader.ID, "overlay");
        glm::vec3 col = glm::vec3(0.5f, 0.8f, 1.0f);
        if (!alive) {
            col = glm::vec3(0.8f, 0.4f, 0.4f);
        }
        glUniform3f(colorLoc, col.x, col.y, col.z);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glDisable(GL_BLEND);

        // dashboard
		// ---------
        controlShader.use();

        glBindVertexArray(VAO[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bottom_control_tex);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, top_control_tex);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // signature
		// ----------
        textureShader.use();

        unsigned signatureLoc = glGetUniformLocation(textureShader.ID, "sampTex");
        glUniform1i(signatureLoc, 3);
        glBindVertexArray(VAO[1]);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, signature);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // planet selection
		// ----------------
        glm::vec3 rayOrigin = camera.Position; 
        glm::vec3 rayDirection = glm::normalize(camera.Front);

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            selected = selectPlanet(rayOrigin, rayDirection);
        }

        // planet exploration
		// ------------------
        if (selected) {
            distanceToSelected = distanceToPlanet(selected->position, rayOrigin, selected->radius);
            if (distanceToSelected < 3.0f) {
				selected->explored = true;
            }
        }

        // asteroid collision
		// ------------------
        for (Planet& asteroid : asteroids) {
            float distance = distanceToPlanet(asteroid.position, rayOrigin, asteroid.radius);
            if (distance < 0.5f) {
                alive = false;
            }
        }

        // check for end of game
		// ---------------------
        int explCounter = 0;
		for (Planet& planet : planets) {
            if (planet.explored) {
                explCounter++;
            }
		}
        if (explCounter >= planets.size()) gameEnd = true;
        
        // render text 
		// -----------
        ostringstream oss;
        oss << std::fixed << std::setprecision(3) << distanceToSelected * 100;
        string displayDistance = oss.str();

        textShader.use();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (!gameEnd && alive) {

            // TARGET: PlanetName
            // ------------------
            renderText(textShader,
                "TARGET:",
                730.0f, 140.0f, 1.0f,
                glm::vec3(1.0f, 1.0f, 1.0f),
                Characters,
                VAO[4], VBO[4]);
            renderText(textShader,
                selected ? selected->name : "No Target",
                890.0f, 140.0f, 1.0f,
                glm::vec3(1.0f, 1.0f, 1.0f),
                Characters,
                VAO[4], VBO[4]);

            // DISTANCE: 1000.00
            // -----------------
            renderText(textShader,
                "DISTANCE:",
                730.0f, 100.0f, 1.0f,
                glm::vec3(1.0f, 1.0f, 1.0f),
                Characters,
                VAO[4], VBO[4]);
            renderText(textShader,
                displayDistance,
                890.0f, 100.0f, 1.0f,
                glm::vec3(1.0f, 1.0f, 1.0f),
                Characters,
                VAO[4], VBO[4]);

            // STATUS: Unknown
            // ---------------
            renderText(textShader,
                "STATUS:",
                730.0f, 60.0f, 1.0f,
                glm::vec3(1.0f, 1.0f, 1.0f),
                Characters,
                VAO[4], VBO[4]);
            renderText(textShader,
                selected ? selected->explored ? "Explored" : "Unknown" : "No Target",
                890.0f, 60.0f, 1.0f,
                glm::vec3(1.0f, 1.0f, 1.0f),
                Characters,
                VAO[4], VBO[4]);
        }
        else {

            // MISION: Fail/Success
            // --------------------
            glm::vec3 colR = glm::vec3(1.0f, 1.0f, 1.0f);
            float x = 810.f;
            if (!alive) {
                state = "Fail";
                colR = glm::vec3(1.0f, 0.0f, 0.0f);
                x = 850.0f;
            }
            renderText(textShader,
                "MISSION: " + state,
                x, 90.0f, 1.5f,
                colR,
                Characters,
                VAO[4], VBO[4]);
        }

		glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        
        // glfw: swap buffers and poll IO events
        // -------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
		capFPS();
    }

    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++ POSPREMANJE +++++++++++++++++++++++++++++++++++++++++++++++++
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteBuffers(4, EBO);
    glDeleteBuffers(5, VBO);
    glDeleteVertexArrays(5, VAO);
    glDeleteProgram(controlShader.ID);
    glDeleteProgram(modelShader.ID);
    glDeleteProgram(skyboxShader.ID);
    glDeleteProgram(textureShader.ID);
    glDeleteProgram(overlayShader.ID);
    glDeleteProgram(textShader.ID);

    glfwTerminate();
    return 0;
}

static unsigned loadImageToTexture(const char* filePath) {
    int TextureWidth;
    int TextureHeight;
    int TextureChannels;
    unsigned char* ImageData = stbi_load(filePath, &TextureWidth, &TextureHeight, &TextureChannels, 0);
    if (ImageData != NULL)
    {
        //Slike se osnovno ucitavaju naopako pa se moraju ispraviti da budu uspravne
        stbi__vertical_flip(ImageData, TextureWidth, TextureHeight, TextureChannels);

        // Provjerava koji je format boja ucitane slike
        GLint InternalFormat = -1;
        switch (TextureChannels) {
        case 1: InternalFormat = GL_RED; break;
        case 2: InternalFormat = GL_RG; break;
        case 3: InternalFormat = GL_RGB; break;
        case 4: InternalFormat = GL_RGBA; break;
        default: InternalFormat = GL_RGB; break;
        }

        unsigned int Texture;
        glGenTextures(1, &Texture);
        glBindTexture(GL_TEXTURE_2D, Texture);
        glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, TextureWidth, TextureHeight, 0, InternalFormat, GL_UNSIGNED_BYTE, ImageData);
        glBindTexture(GL_TEXTURE_2D, 0);
        // oslobadjanje memorije zauzete sa stbi_load posto vise nije potrebna
        stbi_image_free(ImageData);
        return Texture;
    }
    else
    {
        cout << "Textura nije ucitana! Putanja texture: " << filePath << endl;
        stbi_image_free(ImageData);
        return 0;
    }
}

void renderText(Shader shader, string text, float x, float y, float scale, glm::vec3 color, map<char, Character>& Characters, unsigned int VAO, unsigned int VBO) {
    // activate corresponding render state	
    shader.use();
    glActiveTexture(GL_TEXTURE10);
    glUniform3f(glGetUniformLocation(shader.ID, "textColor"), color.x, color.y, color.z);
    glUniform1i(glGetUniformLocation(shader.ID, "text"), 10);
    glBindVertexArray(VAO);

    // iterate through all characters
    string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++)
    {
        Character ch = Characters[*c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;
        // update VBO for each character
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };
        // render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        // update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
        x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64)
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void capFPS()
{
    auto currentTime = chrono::high_resolution_clock::now();
    double elapsedTime = chrono::duration<double>(currentTime - lastFrameTime).count();

    if (elapsedTime < frameDuration)
    {
        this_thread::sleep_for(chrono::duration<double>(frameDuration - elapsedTime));
    }
    lastFrameTime = chrono::high_resolution_clock::now();
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window, bool alive)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (alive) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.ProcessKeyboard(RIGHT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            camera.ProcessKeyboard(DOWN, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            camera.ProcessKeyboard(UP, deltaTime);
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes - for debugging
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// load cubemap texture and create mipmaps
// ---------------------------------------
unsigned int loadCubemap(std::vector<std::string> faces) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

// intersection formula derived from parametrci ray equation and sphere equation
// -----------------------------------------------------------------------------
bool rayIntersectsSphere(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
    const glm::vec3& sphereCenter, float sphereRadius, float& t) {
    glm::vec3 oc = rayOrigin - sphereCenter;
    float a = glm::dot(rayDir, rayDir);
    float b = 2.0f * glm::dot(oc, rayDir);
    float c = glm::dot(oc, oc) - sphereRadius * sphereRadius;
    float discriminant = b * b - 4 * a * c;

    if (discriminant < 0) return false;  // No intersection

    t = (-b - sqrt(discriminant)) / (2.0f * a);  // Closest hit
    return (t > 0);
}

// loop over all planets and select the closes one pierced by the ray
// ------------------------------------------------------------------
Planet* selectPlanet(const glm::vec3& rayOrigin, const glm::vec3& rayDirection) {
    Planet* selectedPlanet = nullptr;
    float closestT = FLT_MAX;

    for (Planet& planet : planets) {
        float t;
        if (rayIntersectsSphere(rayOrigin, rayDirection, planet.position, planet.radius, t)) {
            if (t < closestT) {
                closestT = t;
                selectedPlanet = &planet;
            }
        }
    }
    return selectedPlanet; // Returns nullptr if no planet was hit
}

// distance between a planet and the camera | if we enter the planet the distance becomes negative and we set it to 0
// ------------------------------------------------------------------------------------------------------------------
float distanceToPlanet(glm::vec3 planetPos, glm::vec3 cameraPos, float radius) {
    glm::vec3 diff = cameraPos - planetPos;
    float centerDistance = glm::length(diff);
    float surfaceDistance = centerDistance - radius;
    surfaceDistance = glm::max(0.0f, surfaceDistance);

    return surfaceDistance;
}