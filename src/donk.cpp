#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define GLFW_INCLUDE_ES3
#include <GLFW/glfw3.h>

#define GLES3W_IMPLEMENTATION glfwGetProcAddress
#include <GLES3/gles3w.h>

static char vs[] =
"// Advect.VS\n"
"\n"
"in vec3 Position;\n"
"in float BirthTime;\n"
"in vec3 Velocity;\n"
"\n"
"out vec3 vPosition;\n"
"out float vBirthTime;\n"
"out vec3 vVelocity;\n"
"\n"
"uniform sampler3D Sampler;\n"
"uniform vec3 Size;\n"
"uniform vec3 Extent;\n"
"uniform float Time;\n"
"uniform float TimeStep = 5.0;\n"
"uniform float InitialBand = 0.1;\n"
"uniform float SeedRadius = 0.25;\n"
"uniform float PlumeCeiling = 3.0;\n"
"uniform float PlumeBase = -3;\n"
"\n"
"const float TwoPi = 6.28318530718;\n"
"const float UINT_MAX = 4294967295.0;\n"
"\n"
"uint randhash(uint seed)\n"
"{\n"
"    uint i=(seed^12345391u)*2654435769u;\n"
"    i^=(i<<6u)^(i>>26u);\n"
"    i*=2654435769u;\n"
"    i+=(i<<5u)^(i>>12u);\n"
"    return i;\n"
"}\n"
"\n"
"float randhashf(uint seed, float b)\n"
"{\n"
"    return float(b * randhash(seed)) / UINT_MAX;\n"
"}\n"
"\n"
"vec3 SampleVelocity(vec3 p)\n"
"{\n"
"    vec3 tc;\n"
"    tc.x = (p.x + Extent.x) / (2 * Extent.x);\n"
"    tc.y = (p.y + Extent.y) / (2 * Extent.y);\n"
"    tc.z = (p.z + Extent.z) / (2 * Extent.z);\n"
"    return texture(Sampler, tc).xyz;\n"
"}\n"
"\n"
"void main()\n"
"{\n"
"    vPosition = Position;\n"
"    vBirthTime = BirthTime;\n"
"\n"
"    // Seed a new particle as soon as an old one dies:\n"
"    if (BirthTime == 0.0 || Position.y > PlumeCeiling) {\n"
"        uint seed = uint(Time * 1000.0) + uint(gl_VertexID);\n"
"        float theta = randhashf(seed++, TwoPi);\n"
"        float r = randhashf(seed++, SeedRadius);\n"
"        float y = randhashf(seed++, InitialBand);\n"
"        vPosition.x = r * cos(theta);\n"
"        vPosition.y = PlumeBase + y;\n"
"        vPosition.z = r * sin(theta);\n"
"        vBirthTime = Time;\n"
"    }\n"
"\n"
"    // Move the particles forward using a half-step to reduce numerical issues:\n"
"    vVelocity = SampleVelocity(Position);\n"
"    vec3 midx = Position + 0.5f * TimeStep * vVelocity;\n"
"    vVelocity = SampleVelocity(midx);\n"
"    vPosition += TimeStep * vVelocity;\n"
"}\n";

static char fs[] =
"#version 300 es\n"
"// Dummy\n"
"\n"
"out vec4 FragColor;\n"
"\n"
"void main()\n"
"{\n"
"    FragColor = vec4(0);\n"
"}\n";

static const char* attribs[] = { "Position", "TexCoord", "Normal", "BirthTime", "Velocity" };
static const char* varyings[] = { "vPosition", "vBirthTime", "vVelocity" };

#ifdef _MSC_VER
extern "C" {
__declspec(dllimport) void __stdcall
OutputDebugStringA(_In_opt_ const char* lpOutputString);
}
#endif

static void _fatalError(const char* pStr, va_list a) {
    char msg[1024] = {0};
#ifdef _MSC_VER
    _vsnprintf_s(msg, _countof(msg), _TRUNCATE, pStr, a);
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
    __debugbreak();
#else
    vsnprintf(msg, 1024, pStr, a);
    fputs(msg, stderr);
#endif
    exit(1);
}

static void fatalError(const char* pStr, ...) {
    va_list a;
    va_start(a, pStr);
    _fatalError(pStr, a);
}

static void
checkCondition(int condition, ...) {
    va_list a;
    const char* pStr;

    if (condition)
        return;

    va_start(a, condition);
    pStr = va_arg(a, const char*);
    _fatalError(pStr, a);
}

static void
errorCB(int error, const char* description) {
    fputs(description, stderr);
    fatalError("%s", description);
}

static void
keyCB(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

static void
resizeCB(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

#if 0
static GLuint loadProgram(const char* pVS, const char* pFS,
                          int aCount, const char* attribs[],
                          int vCount, const char* varyings[])
{
    GLint compileSuccess, linkSuccess;
    GLchar compilerSpew[256];

    GLuint programHandle = glCreateProgram();

    GLuint vsHandle = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vsHandle, 1, &pVS, 0);
    glCompileShader(vsHandle);
    glGetShaderiv(vsHandle, GL_COMPILE_STATUS, &compileSuccess);
    glGetShaderInfoLog(vsHandle, sizeof(compilerSpew), 0, compilerSpew);
    checkCondition(compileSuccess, ("Can't compile VS:\n%s", compilerSpew));
    glAttachShader(programHandle, vsHandle);

    GLuint fsHandle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fsHandle, 1, &pFS, 0);
    glCompileShader(fsHandle);
    glGetShaderiv(fsHandle, GL_COMPILE_STATUS, &compileSuccess);
    glGetShaderInfoLog(fsHandle, sizeof(compilerSpew), 0, compilerSpew);
    checkCondition(compileSuccess, ("Can't compile FS:\n%s", compilerSpew));
    glAttachShader(programHandle, fsHandle);

    for (int a = 0; a < aCount; a++)
        glBindAttribLocation(programHandle, 0, attribs[a]);

    if (vCount) {
        glTransformFeedbackVaryings(programHandle, vCount, varyings, GL_INTERLEAVED_ATTRIBS);
        checkCondition(glGetError() == GL_NO_ERROR, "OpenGL error.");
    }

    glLinkProgram(programHandle);
    glGetProgramiv(programHandle, GL_LINK_STATUS, &linkSuccess);
    glGetProgramInfoLog(programHandle, sizeof(compilerSpew), 0, compilerSpew);
    checkCondition(linkSuccess, compilerSpew);

    int total = -1;
    glGetProgramiv(programHandle, GL_ACTIVE_UNIFORMS, &total);
    for(int i = 0; i < total; ++i) {
        int name_len=-1, num=-1;
        GLenum type = GL_ZERO;
        char name[100];
        glGetActiveUniform(programHandle, GLuint(i), sizeof(name)-1, &name_len, &num, &type, name);
        name[name_len] = '\0';
        GLuint location = glGetUniformLocation(programHandle, name);

        printf("uniform[%d]: %s\n", location, name);
    }

    glUseProgram(programHandle);
    return programHandle;
}
#endif

int
main(int argc, char* argv[]) {
    int result = 0;

    printf("donk.\n");

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    glfwSetErrorCallback(errorCB);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(640, 480, "donk.", NULL, NULL);
    if (!window) {
        result = EXIT_FAILURE;
        goto terminate;
    }

    glfwSetKeyCallback(window, keyCB);
    glfwSetFramebufferSizeCallback(window, resizeCB);

    glfwMakeContextCurrent(window);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(1, 0, 0, 1);

//    GLuint program = loadProgram(vs, fs, 5, attribs, 3, varyings);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

terminate:
    glfwTerminate();

exit:
    return result;
}
