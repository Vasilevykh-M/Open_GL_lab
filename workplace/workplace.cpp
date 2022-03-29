#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include<glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include<chrono>
#include <cmath>
#include "SOIL.h"

#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "opengl32.lib")

using namespace std;
using namespace glm;

GLFWwindow* g_window;

GLuint g_shaderProgram;
GLint g_uMVP;
GLint g_uMV;
GLint g_time;
GLint g_M33;
float scale_coef = 5.f;

GLfloat x0 = -10.0;
GLfloat z0 = -10.0;
GLfloat g_uColors;
GLfloat colors[258];
GLfloat dx = 0.1;
GLfloat dz = 0.1;
GLfloat angle = 0.0f;
mat4 ModelMatrix = mat4(1.0f);

GLuint texID;
GLint mapLocation;


chrono::system_clock::time_point old_time = chrono::system_clock::now();
chrono::system_clock::time_point cur_time = chrono::system_clock::now();

const char* filename = "C:\\Users\\Михаил\\Desktop\\Графика и вычислительная геометрия\\текстура\\shrek.png";

class Model
{
public:
    GLuint vbo;
    GLuint ibo;
    GLuint vao;
    GLsizei indexCount;
};

Model g_model;

GLuint createShader(const GLchar* code, GLenum type)
{
    GLuint result = glCreateShader(type);

    glShaderSource(result, 1, &code, NULL);
    glCompileShader(result);

    GLint compiled;
    glGetShaderiv(result, GL_COMPILE_STATUS, &compiled);

    if (!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0)
        {
            char* infoLog = (char*)alloca(infoLen);
            glGetShaderInfoLog(result, infoLen, NULL, infoLog);
            cout << "Shader compilation error" << endl << infoLog << endl;
        }
        glDeleteShader(result);
        return 0;
    }

    return result;
}

GLuint createProgram(GLuint vsh, GLuint fsh)
{
    GLuint result = glCreateProgram();

    glAttachShader(result, vsh);
    glAttachShader(result, fsh);

    glLinkProgram(result);

    GLint linked;
    glGetProgramiv(result, GL_LINK_STATUS, &linked);

    if (!linked)
    {
        GLint infoLen = 0;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0)
        {
            char* infoLog = (char*)alloca(infoLen);
            glGetProgramInfoLog(result, infoLen, NULL, infoLog);
            cout << "Shader program linking error" << endl << infoLog << endl;
        }
        glDeleteProgram(result);
        return 0;
    }

    return result;
}

bool createShaderProgram()
{
    g_shaderProgram = 0;


    /*матрица преобразований в виде юниформ переменной*/

    /*варинг переменные - общение между шейдерами*/

    /*юниформ - связывает шейдер и прогу которая её запускает*/
    /*можем менять её между перерисовками на вызывающей стороне*/

    const GLchar vsh[] =
        "#version 330\n"
        ""
        "layout(location = 0) in vec2 a_position;"
        ""
        "out vec3 v_pos;"
        "out vec3 v_normal;"
        ""
        "uniform mat4 u_mvp;"
        "uniform mat4 u_mv;"
        "uniform mat3 u_m33;"
        ""
        "vec3 grad(vec2 pos)"
        "{"
        "   return vec3(-3.14/4 * cos(pos[0]*3.14/4)*cos(pos[1]*3.14/4), 1.0, 3.14/4 * sin(pos[0]*3.14/4) * sin(pos[1]*3.14/4));"
        "}"

        ""
        "float f(vec2 a_position)"
        "{"
        "   return sin(a_position[0]*3.14/4) * cos(a_position[1]*3.14/4);"
        "}"
        ""
        "void main()"
        "{"
        "   vec4 pos = vec4(a_position[0], f(a_position), a_position[1], 1.0);"
        "   v_pos = vec3(u_mv * pos);"
        "   vec3 n = grad(a_position);"
        "   v_normal = u_m33 * normalize(n);" // вынес тк по идее эта матричка считается один раз за весь кадр
        "   gl_Position = u_mvp * pos;"
        "}"
        ;

    const GLchar fsh[] =
        "#version 330\n"
        ""
        "in vec3 v_pos;"
        "in vec3 v_normal;"
        ""// но по хорошему это работает на случайность кадра, а не точки
        "layout(location = 0) out vec4 o_color;"
        ""
        "void main()"
        "{"
        "   vec3 n = normalize(v_normal);"
        ""
        "   vec3 L = vec3(0, 5, 0);" // источник света
        "   vec3 l = normalize(L - v_pos);" // поток света
        "   vec3 e = normalize(-1 * v_pos);" // вектор в сторону наблюдателя
        ""
        "   float d = dot(l, n);" // дифузное освещение
        ""
        ""
        "   vec3 h = normalize(l + e);"
        "   float s = dot(h, n);"
        "   s = pow(s, 20);" // блик
        ""
        "   vec3 _color = vec3(0.5, 0.5, 0);"
        ""
        "   o_color = vec4(_color * d + vec3(s), 1);"
        ""
        "}"
        ;
    /*Thin Film Interferecies*/

    GLuint vertexShader, fragmentShader;

    vertexShader = createShader(vsh, GL_VERTEX_SHADER);
    fragmentShader = createShader(fsh, GL_FRAGMENT_SHADER);

    g_shaderProgram = createProgram(vertexShader, fragmentShader);

    /*запрос матрицы из шейдера*/
    g_uMVP = glGetUniformLocation(g_shaderProgram, "u_mvp");
    g_uMV = glGetUniformLocation(g_shaderProgram, "u_mv");
    /*время*/
    g_M33 = glGetUniformLocation(g_shaderProgram, "u_m33");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return g_shaderProgram != 0;
}

bool createModel()
{

    GLint n = 200;

    GLint vertex_count = n * n * 2;

    GLfloat* vertex_arr = new GLfloat[vertex_count];

    int k = 0;

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            GLfloat x = x0 + dx * j;
            GLfloat z = z0 + dz * i;
            vertex_arr[k] = x;
            k++;

            vertex_arr[k] = z;
            k++;
            //cout <<k-2<< " " << vertex_arr[k - 2] << " " << vertex_arr[k - 1] << endl;
            //cout << vertex_arr[k - 2]<<" ";
        }
        //cout << endl;
    }
    GLint index_size = (n - 1) * (n - 1) * 6;

    GLuint* index_arr = new GLuint[index_size];

    int i = 0;
    int l = 1;

    for (int k = 0; k < index_size; k += 6)
    {
        if (i == (n * l - 1)) // были баги с треугольниками, что он протягивал плоскость снизу из за этого рисовались всякие плохие штуки
        {
            l++;
            i++;
            continue;
        }

        index_arr[k] = i;
        index_arr[k + 1] = i + 1;
        index_arr[k + 2] = n + i + 1;
        index_arr[k + 3] = n + i + 1;
        index_arr[k + 4] = n + i;
        index_arr[k + 5] = i;
        i++;

        //cout << index_arr[k] << " " << index_arr[k + 1] << " " << index_arr[k + 2] << " " << index_arr[k + 3] << " " << index_arr[k + 4] << " " << index_arr[k + 5] << endl;
    }

    glGenVertexArrays(1, &g_model.vao);
    glBindVertexArray(g_model.vao);

    glGenBuffers(1, &g_model.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_model.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(GLfloat), vertex_arr, GL_STATIC_DRAW);

    glGenBuffers(1, &g_model.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_model.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size * sizeof(GLint), index_arr, GL_STATIC_DRAW);

    g_model.indexCount = index_size;

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (const GLvoid*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (const GLvoid*)(2 * sizeof(GLfloat)));

    delete[] vertex_arr;
    delete[] index_arr;

    return g_model.vbo != 0 && g_model.ibo != 0 && g_model.vao != 0;
}

bool init()
{
    // Set initial color of color buffer to white.
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);


    /*SOIL documental*/
    GLsizei texW, texH;
    GLvoid* image = SOIL_load_image(filename, &texW, &texH, 0, SOIL_LOAD_RGB);
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    delete[] image;
    /*я думаю тут можно будет обрезать приколы с пирамидой*/
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    mapLocation = glGetUniformLocation(g_shaderProgram, "u_map");

    glEnable(GL_DEPTH_TEST); // изменение состояний в машине состояний 
    // подключает тест глубины

    return createShaderProgram() && createModel();
}

void reshape(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void draw()
{
    // Clear color buffer.
    /*очистка обоих буферов*/
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(g_shaderProgram);
    glBindVertexArray(g_model.vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID);
    glUniform1i(mapLocation, 0);


    mat4 Projection = perspective(
        radians(45.0f), // Угол обзора
        1.0f,   // Соотншение сторон окна
        z0 - 2,             // Ближняя плоскость отсечения
        abs(z0 - 2)         // Дальняя плоскость отсечения
    );

    mat4 View = lookAt(
        vec3(20, 17, 20), // Координаты камеры
        vec3(0, 0, 0), // Направление камеры в начало координат
        vec3(0, 1, 0)  // Координаты наблюдателя
    );


    // Матрица MV
    cur_time = chrono::system_clock::now();
    angle = fmod(3.14, pow(1 + chrono::duration_cast<chrono::microseconds>(cur_time - old_time).count(), 3.0)) / 300;
    // тут заложена функция расчета поворота
    //суть в том чем дольше рабтал предыуший шейдер тем меньше будет угол поворота во избежания рывков при анимации
    old_time = cur_time;

    ModelMatrix = rotate(ModelMatrix, radians(angle), vec3(0, 1, 0));
    mat4 MV = View * ModelMatrix;

    // Матрица MVP
    mat4 MVP = Projection * MV;
    float time = chrono::duration_cast<chrono::microseconds>(cur_time - old_time).count();

    mat3 M3_3 = transpose(inverse(mat3(MV)));

    /*отправляем её в шейдер*/
    glUniformMatrix4fv(g_uMV, 1, GL_FALSE, &MV[0][0]);
    glUniformMatrix4fv(g_uMVP, 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix3fv(g_M33, 1, GL_FALSE, &M3_3[0][0]);

    /*вызываем перерисовку*/
    glDrawElements(GL_TRIANGLES, g_model.indexCount, GL_UNSIGNED_INT, NULL);

}

void cleanup()
{
    if (g_shaderProgram != 0)
        glDeleteProgram(g_shaderProgram);
    if (g_model.vbo != 0)
        glDeleteBuffers(1, &g_model.vbo);
    if (g_model.ibo != 0)
        glDeleteBuffers(1, &g_model.ibo);
    if (g_model.vao != 0)
        glDeleteVertexArrays(1, &g_model.vao);
}

bool initOpenGL()
{
    // Initialize GLFW functions.
    if (!glfwInit())
    {
        cout << "Failed to initialize GLFW" << endl;
        return false;
    }

    // Request OpenGL 3.3 without obsoleted functions.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window.
    g_window = glfwCreateWindow(800, 600, "OpenGL Test", NULL, NULL);
    if (g_window == NULL)
    {
        cout << "Failed to open GLFW window" << endl;
        glfwTerminate();
        return false;
    }

    // Initialize OpenGL context with.
    glfwMakeContextCurrent(g_window);

    // Set internal GLEW variable to activate OpenGL core profile.
    glewExperimental = true;

    // Initialize GLEW functions.
    if (glewInit() != GLEW_OK)
    {
        cout << "Failed to initialize GLEW" << endl;
        return false;
    }

    // Ensure we can capture the escape key being pressed.
    glfwSetInputMode(g_window, GLFW_STICKY_KEYS, GL_TRUE);

    // Set callback for framebuffer resizing event.
    glfwSetFramebufferSizeCallback(g_window, reshape);

    return true;
}

void tearDownOpenGL()
{
    // Terminate GLFW.
    glfwTerminate();
}

//не меняется
int main()
{
    // Initialize OpenGL
    if (!initOpenGL())
        return -1;

    // Initialize graphical resources.
    bool isOk = init();

    if (isOk)
    {
        // Main loop until window closed or escape pressed.
        while (glfwGetKey(g_window, GLFW_KEY_ESCAPE) != GLFW_PRESS && glfwWindowShouldClose(g_window) == 0)
        {
            // Draw scene.
            draw();

            // Swap buffers.
            glfwSwapBuffers(g_window);
            // Poll window events.
            glfwPollEvents();
        }
    }

    // Cleanup graphical resources.
    cleanup();

    // Tear down OpenGL.
    tearDownOpenGL();

    return isOk ? 0 : -1;
}