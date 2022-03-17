#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include<glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include<chrono>
#include <cmath>

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
float scale_coef = 5.f;

GLfloat x0 = -10.0;
GLfloat z0 = -10.0;
GLfloat g_uColors;
GLfloat colors[258];
GLfloat dx = 0.1;
GLfloat dz = 0.1;
GLfloat angle = 0.0f;
mat4 ModelMatrix = mat4(1.0f);


chrono::system_clock::time_point old_time = chrono::system_clock::now();
chrono::system_clock::time_point cur_time = chrono::system_clock::now();

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
        "uniform float g_time;"
        ""
        "vec3 grad(vec2 pos)"
        "{"
        "   return vec3(3.14/5 * cos(pos[0]*3.14/5)*cos(pos[1]*3.14/5), 1.0, -1 * 3.14/5 * sin(pos[0]*3.14/5) * sin(pos[1]*3.14/5));"
        "}"

        ""
        "float f(vec2 a_position)"
        "{"
        "   return sin(a_position[0]*3.14/5) * cos(a_position[1]*3.14/5);"
        "}"
        ""
        "void main()"
        "{"
        "   vec4 pos = vec4(a_position[0], f(a_position), a_position[1], 1.0);"
        "   v_pos = vec3(u_mv * pos);"
        "   vec3 n = grad(a_position);"
        "   v_normal = transpose(inverse(mat3(u_mv))) * normalize(n);"
        "   gl_Position = u_mvp * pos;"
        "}"
        ;

    const GLchar fsh[] =
        "#version 330\n"
        ""
        "in vec3 v_pos;"
        "in vec3 v_normal;"
        "uniform float g_time;"//перекачиваю время с основной программы для создания псевдо случайности
        ""// но по хорошему это работает на случайность кадра а не точки
        "layout(location = 0) out vec4 o_color;"
        "float rand(vec3 seed)" // функция рандомизации
        "{"
        "   return atan(dot(vec2(sin(3.14  *seed[0])*4 + 1, cos(3.14  * seed[0])*4+1), vec2(atan(seed[1]), tan(seed[2])))) * 12;"
        "}"//на само деле какая то плоха рандомизация, но как тестовая должна вполне себе работать
        ""
        ""
        "void main()"
        "{"
        "   vec3 n = normalize(v_normal);"
        ""
        "   vec3 L = vec3(0,5,0);" // источник света
        "   vec3 l = normalize(L - v_pos);" // поток света
        "   vec3 e = normalize(-1 * v_pos);" // вектор в сторону наблюдателя
        ""
        "   float d = dot(l, n);" // дифузное освещение(так же можно интерпритировать как sin угла падения к нормали)
        ""
        "   vec3 h = normalize(l + e);"
        "   float s = dot(h, n);"
        "   s = pow(s, 20);" // блик
        "   float lambda = 520.0;" // длинна волны которая прилетела
        "   float d_film = rand(vec3(g_time, s, d));" // толщина пленки
        "   float delta = 2 * d_film * sqrt(1.25*1.25- d * d) - (lambda / 2.0);" //разность хода 
        "   float lambda1 = 2 * lambda * (1.0 + cos(delta));" //закон сохранения энергии 

        ""
        "   vec3 color = vec3(0, 0, 0);"// базовый цвет
        ""
        "   if(lambda1 >= 380 && lambda1 <= 450)" // фиолетовый
        "       color = vec3((lambda1 - 380) * 0.014, 0, 0.014 * (lambda1 - 380));"//[0, 0, 0] -> [255, 0, 255]
        ""
        "   if(lambda1 > 450 && lambda1 <= 500)" // синий
        "       color = vec3(1.0 - (lambda - 450) * 0.02, 0, 255);" // [255, 0, 255] -> [0, 0, 255]
        ""
        "   if(lambda1 > 500 && lambda1 <= 520)" // циановый
        "       color = vec3(0, (lambda1 - 500) * 0.05, 255);" // [0, 0, 255] -> [0, 255, 255]
        ""
        "   if(lambda1 > 520 && lambda1 <= 565)" // зеленый
        "       color = vec3(0, 255, 1.0 - (lambda1 - 520) * 0.0222);" // [0, 255, 255] -> [0, 255, 0]
        ""
        "   if(lambda1 > 565 && lambda1 <= 590)" //желтый
        "       color = vec3((lambda1 - 565) * 0.04, 255, 0);" //[0, 255, 0] -> [255, 255, 0]
        ""
        "   if(lambda1 > 590 && lambda1 <= 635)" //оранжевый
        "       color = vec3(255, 1.0 - 0.0111 * (lambda1 - 590), 0);" // [255, 255, 0] -> [255, 125, 0]
        ""
        "   if(lambda1 > 635 && lambda1 < 770)" // красный
        "       color = vec3(255, 0.5 - (lambda1 - 635)*0.0037, 0);" // [255, 125, 0] -> [255, 0, 0]
        ""
        "   vec4 phonk = vec4(color, 1);"
        "   o_color = phonk;" //беда с фонгом он сьедает плавный переход и делает его утра грубым
        "" //хз почему, мб потому что  перекатывает в целое
        ""
        "}"
        ;

    /* Шейдер света типо зашит в шейдере цвета*/
    /* Вектор от точки к источнику света - L, нормаль - N, точка на которую падает свет - P, вектор отраженного света к наблюдателю - E* /
    /* С = Cd * cos a + Cs * (cos B)^n*/
    /* P L E N - "дано", но нужно все равно вычислять*/
    /* В пространсве наблюдателя E = {0,0,0}*/




    /*
    in vec2 a_pos;

    out vec3 v_pos; точка
    out vec3 v_normal; вектор нормаль           нужно знать градиент функции y - sin(X)*cos(z) = 0 или любой другой плоскости которую имеем

    uniform mat4 u_mvp; mvp
    uniform mat4 u_mv; mv

    vec3 grad(vec pos)
    {
        return vec3(-cos(pos[0])*cos(pos[1]), 1.0, sin(pos[0]) * sin(pos[1])); 
    }

    void main()
    {
        vec4 pos = vec4(a_pos[0], f(a_pos), a_pos[1], 1);
        v_pos = u_mv * pos;
        vec3 n = grad(a_pos);
        v_normal = transpos(inverse(mat3(u_mv))) * normalaze(n);
        gl_Position = u_mvp * pos;
    }
    */
    // / \
    //  |
    //
    /*N = ((MV[3*3])^-1)^T*/

    /*L = {5, 5, -2};       в пространстве наблюдателя*/



    /*фрагментный шейдер*/
    /*
    in vec3 v_pos;
    in vec3 v_normal;
    
    void main()
    {
        vec3 n = normalize(v_normal);

        vec3 l = normalize(L - v_pos);
        vec3 e = normalize(-1 * v_pos);

        float d = dot(l, n); // диф освещение

        vec3 h = normalize(l + e);
        float s = dot(h, n); // блик
        s = pow(s, 20);

        o_color = vec4(vec3(0,1,0) * d + vec3(s), 1);


        o_color = vec4(vec3(1, 0, 0) * d, 1);

        o_color = vec4(abs(n), 1); дебажим нормали 
    }
    */

    /*Thin Film Interferecies*/

    GLuint vertexShader, fragmentShader;

    vertexShader = createShader(vsh, GL_VERTEX_SHADER);
    fragmentShader = createShader(fsh, GL_FRAGMENT_SHADER);

    g_shaderProgram = createProgram(vertexShader, fragmentShader);

    /*запрос матрицы из шейдера*/
    g_uMVP = glGetUniformLocation(g_shaderProgram, "u_mvp");
    g_uMV = glGetUniformLocation(g_shaderProgram, "u_mv");
    g_time = glGetUniformLocation(g_shaderProgram, "g_time");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return g_shaderProgram != 0;
}

GLint getNumVertex(int j, int i, int n) {
    return (GLint)(i + j * (n + 1));
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

    for (int k = 0; k < index_size; k+=6)
    {
        if (i == (n * l - 1))
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

    return g_model.vbo != 0 && g_model.ibo != 0 && g_model.vao != 0;
}

bool init()
{
    // Set initial color of color buffer to white.
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

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


    /*обьявляем матрицу преобразования*/
    /*для примера надо будет автоматом получать*/


    mat4 Projection = perspective(
        radians(45.0f), // Угол обзора
        1.0f,   // Соотншение сторон окна
        z0-2,             // Ближняя плоскость отсечения
        abs(z0-2)         // Дальняя плоскость отсечения
    );

    mat4 View = lookAt(
        vec3(20, 17, 20), // Координаты камеры
        vec3(0, 0, 0), // Направление камеры в начало координат
        vec3(0, 1, 0)  // Координаты наблюдателя
    );


    // Матрица MVP
    cur_time = chrono::system_clock::now();
    angle = fmod(3.14, pow(1 + chrono::duration_cast<chrono::microseconds>(cur_time - old_time).count(), 3.0))/300;
    old_time = cur_time;

    ModelMatrix = rotate(ModelMatrix, radians(angle), vec3(0, 1, 0));
    mat4 MV = View * ModelMatrix;

    // Матрица MVP
    mat4 MVP = Projection * MV;
    float time = chrono::duration_cast<chrono::microseconds>(cur_time - old_time).count();

    /*отправляем её в шейдер*/
    glUniformMatrix4fv(g_uMV, 1, GL_FALSE, &MV[0][0]);
    glUniformMatrix4fv(g_uMVP, 1, GL_FALSE, &MVP[0][0]);
    glUniform1f(g_time, time);

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