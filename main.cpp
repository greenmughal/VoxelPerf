
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <functional>
#include <stdlib.h>
#include <stdio.h>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <fstream>
#include <iostream>

#include <windows.h>
#include <psapi.h>

#include "PerfTimer.h"

using namespace glm;
using namespace std;

#pragma pack(push, 1)
struct Vertex {
    vec3 position;
    vec3 color;
};
#pragma pack(pop)

static const char* vertex_shader_text =
"uniform mat4 MVP;\n"
"attribute vec3 vColor;\n"
"attribute vec3 vPos;\n"
"varying vec3 color;\n"
"void main()\n"
"{\n"
"    gl_Position = MVP * vec4(vPos, 1.0);\n"
"    color = vColor;\n"
"}\n";

static const char* fragment_shader_text =
"varying vec3 color;\n"
"void main()\n"
"{\n"
"    gl_FragColor = vec4(color, 1.0);\n"
"}\n";

GLFWwindow* window;

void CheckGLErrors() {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        cerr << "OpenGL error: " << err << endl;
    }
}

mat4 MakeModelView() {
    mat4 view = lookAt(vec3(-16, 0, -16), vec3(0, 0, 0), vec3(0, 1, 0));
        //* glm::rotate((float)glfwGetTime(), vec3(0, 1, 0));
    return view;
}

mat4 MakeProjection() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float fovY = radians(45.0f);
    mat4 projection = perspective(fovY, (float)width / (float)height, 1.0f, 10000.0f);
    return projection;
}

mat4 MakeMvp() {
    return MakeProjection() * MakeModelView();
}

struct VoxelSet {
    VoxelSet(ivec3 size) {
        int elemCount = size.x * size.y * size.z;
        this->size = size;
        colors.resize(elemCount);
        for (int i = 0; i < elemCount; ++i) {
            colors[i] = vec4(0, 0, 0, 0);
        }
    }

    ivec3 size;
    vector<vec4> colors;

    inline bool IsValid(ivec3 idx) {
        return idx.x >= 0 && idx.y >= 0 && idx.z >= 0
            && idx.x < size.x && idx.y < size.y && idx.z < size.z;
    }

    inline bool IsSolid(ivec3 idx) {
        return IsValid(idx) && At(idx).a > 0.1f;
    }

    inline vec4& At(ivec3 idx) {
        return colors[idx.z * (size.x * size.y) + idx.y * size.x + idx.x];
    }
};

GLuint MakeShader() {
    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vShader, 1, &vertex_shader_text, NULL);
    glCompileShader(vShader);

    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fShader, 1, &fragment_shader_text, NULL);
    glCompileShader(fShader);

    GLuint program = glCreateProgram();
    glAttachShader(program, vShader);
    glAttachShader(program, fShader);
    glLinkProgram(program);

    return program;
}

float voxelSize = 0.25f;

void MakeSphere(VoxelSet& voxels) {
    float radius = voxels.size.x / 2.0f - 0.5f;
    vec3 color = 1.0f / (vec3(voxels.size) - vec3(1, 1, 1));
    vec3 center = vec3(voxels.size) / 2.0f;

    for (int z = 0; z < voxels.size.z; ++z) {
        for (int y = 0; y < voxels.size.y; ++y) {
            for (int x = 0; x < voxels.size.x; ++x) {

                ivec3 idx(x, y, z);

                vec3 delta = (vec3(idx) + vec3(0.5f, 0.5f, 0.5f)) - center;

                if ((glm::dot(delta, delta) - 0.1f) <= radius * radius) {
                    // Inside sphere
                    voxels.At(idx) = vec4(color * vec3(idx), 1.0f);
                } else {
                    // Outside sphere
                    voxels.At(idx) = vec4(0, 0, 0, 0);
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// Display list functions
//////////////////////////////////////////////////////////////////////////

void DrawFace(vec3 center, vec3 normal) {
    // Center of voxel to center of face
    center += vec3(normal) * voxelSize / 2.0f;

    vec3 d1 = -vec3(normal.z, normal.x, normal.y);
    vec3 d2 = -vec3(normal.y, normal.z, normal.x);

    vector<vec2> weights = {
        { -1, -1 },
        {  1, -1 },
        {  1,  1 },
        { -1,  1 },
    };

    // Reverse winding order for negative normals
    if (normal.x < 0 || normal.y < 0 || normal.z < 0) {
        std::swap(d1, d2);
    }

    for (int i = 0; i < 4; ++i) {
        vec2 w = weights[i];

        //ivec3 idxOffset1 = d1 * w.x;
        //ivec3 idxOffset2 = d2 * w.y;
        //vec3 position = center + (vec3(idxOffset1 + idxOffset2) / 2.0f) * voxelSize;
        vec3 position = center + (d1 * w.x + d2 * w.y) / 2.0f * voxelSize;

        glVertex3fv((float*)&position);
    }
}

void DrawVoxel(VoxelSet& voxels, vec3 offset, ivec3 idx) {
    if (!voxels.IsSolid(idx)) {
        return;
    }

    vec3 color = voxels.At(idx);
    glColor3fv((float*)&color);
    vec3 center = offset + vec3(idx) * voxelSize + vec3(voxelSize, voxelSize, voxelSize) / 2.0f;

    vector<ivec3> normals = {
        { 1, 0, 0 },
        {-1, 0, 0 },
        { 0, 1, 0 },
        { 0,-1, 0 },
        { 0, 0, 1 },
        { 0, 0,-1 },
    };

    for (auto& n : normals) {
        if (voxels.IsSolid(idx + n)) {
            continue;
        }
        DrawFace(center, vec3(n));
    }
}

GLuint VoxelsToDisplayList(VoxelSet& voxels, vec3 offset) {
    GLuint displayIdx = glGenLists(1);
    glNewList(displayIdx, GL_COMPILE);
    
    glBegin(GL_QUADS);
    for (int z = 0; z < voxels.size.z; ++z) {
        for (int y = 0; y < voxels.size.y; ++y) {
            for (int x = 0; x < voxels.size.x; ++x) {
                DrawVoxel(voxels, offset, ivec3(x, y, z));
            }
        }
    }
    glEnd();
    glEndList();

    return displayIdx;
}

void MakeDisplayListGrid(VoxelSet& model, ivec3 dimensions, vec3 spacing, std::vector<GLuint>& displayLists) {
    displayLists.resize(dimensions.x * dimensions.y * dimensions.z);
    int nextListIdx = 0;

    for (int z = 0; z < dimensions.z; ++z) {
        for (int y = 0; y < dimensions.y; ++y) {
            for (int x = 0; x < dimensions.x; ++x) {
                ivec3 idx(x, y, z);
                vec3 offset = vec3(idx) * spacing;
                offset -= vec3(0, dimensions.y, 0) * spacing / 2.0f;
                displayLists[nextListIdx] = VoxelsToDisplayList(model, offset);
                nextListIdx++;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// VBO functions
//////////////////////////////////////////////////////////////////////////

void BufferFace(vec3 center, vec3 normal, vec3 color, vector<Vertex>& vertices, int& nextIdx) {
    // Center of voxel to center of face
    center += vec3(normal) * voxelSize / 2.0f;

    vec3 d1 = -vec3(normal.z, normal.x, normal.y);
    vec3 d2 = -vec3(normal.y, normal.z, normal.x);

    vector<vec2> weights = {
        { -1, -1 },
        {  1, -1 },
        {  1,  1 },
        { -1,  1 },
    };

    // Reverse winding order for negative normals
    if (normal.x < 0 || normal.y < 0 || normal.z < 0) {
        std::swap(d1, d2);
    }

    for (int i = 0; i < 4; ++i) {
        vec2 w = weights[i];

        Vertex v;
        v.color = color;
        v.position = center + (d1 * w.x + d2 * w.y) / 2.0f * voxelSize;

        if (nextIdx >= vertices.size()) {
            vertices.push_back(v);
        } else {
            vertices[nextIdx] = v;
        }
        nextIdx++;
    }
}

void BufferVoxel(VoxelSet& voxels, vec3 offset, ivec3 idx, vector<Vertex>& vertices, int& nextIdx) {
    if (!voxels.IsSolid(idx)) {
        return;
    }

    vec3 color = voxels.At(idx);
    vec3 center = offset + vec3(idx) * voxelSize + vec3(voxelSize, voxelSize, voxelSize) / 2.0f;

    vector<ivec3> normals = {
        { 1, 0, 0 },
        {-1, 0, 0 },
        { 0, 1, 0 },
        { 0,-1, 0 },
        { 0, 0, 1 },
        { 0, 0,-1 },
    };

    for (auto& n : normals) {
        if (voxels.IsSolid(idx + n)) {
            continue;
        }
        BufferFace(center, vec3(n), color, vertices, nextIdx);
    }
}

void VoxelsToVbo(VoxelSet& voxels, vec3 offset, vector<Vertex>& vertices) {
    int nextIdx = 0;

    for (int z = 0; z < voxels.size.z; ++z) {
        for (int y = 0; y < voxels.size.y; ++y) {
            for (int x = 0; x < voxels.size.x; ++x) {
                BufferVoxel(voxels, offset, ivec3(x, y, z), vertices, nextIdx);
            }
        }
    }
}

size_t MakeVaoGrid(VoxelSet& model, ivec3 dimensions, vec3 spacing, std::vector<GLuint>& vaos, GLuint program) {
    std::vector<GLuint> vbos;
    vbos.resize(dimensions.x * dimensions.y * dimensions.z);
    vaos.resize(dimensions.x * dimensions.y * dimensions.z);

    glGenBuffers(vbos.size(), &vbos[0]);
    CheckGLErrors();

    glGenVertexArrays(vaos.size(), &vaos[0]);
    CheckGLErrors();

    int nextVbo = 0;

    vector<Vertex> vertices;

    for (int z = 0; z < dimensions.z; ++z) {
        for (int y = 0; y < dimensions.y; ++y) {
            for (int x = 0; x < dimensions.x; ++x) {
                ivec3 idx(x, y, z);
                vec3 offset = vec3(idx) * spacing;
                offset -= vec3(0, dimensions.y, 0) * spacing / 2.0f;
                VoxelsToVbo(model, offset, vertices);

                glBindVertexArray(vaos[nextVbo]);
                glBindBuffer(GL_ARRAY_BUFFER, vbos[nextVbo]);
                glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), &vertices[0], GL_STATIC_DRAW);

                GLint vPosLoc = glGetAttribLocation(program, "vPos");
                glEnableVertexAttribArray(vPosLoc);
                glVertexAttribPointer(vPosLoc, 3, GL_FLOAT, GL_FALSE,
                                      sizeof(float) * 6, (void*)0);
                CheckGLErrors();

                GLint vColorPos = glGetAttribLocation(program, "vColor");
                glEnableVertexAttribArray(vColorPos);
                glVertexAttribPointer(vColorPos, 3, GL_FLOAT, GL_FALSE,
                                      sizeof(float) * 6, (void*)(sizeof(float) * 3));
                CheckGLErrors();

                nextVbo++;
            }
        }
    }

    return vertices.size();
}

//////////////////////////////////////////////////////////////////////////
// Perf helpers
//////////////////////////////////////////////////////////////////////////

#define GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX 0x9048
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049

size_t GetFreeMemNvidia() {
    // TODO: AMD version w/ GL_ATI_meminfo
    GLint currMem = 0;
    glGetIntegerv(GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX, &currMem);
    return (size_t)currMem * 1024;
}

size_t GetMainMemUsage() {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    return pmc.PrivateUsage;
}

struct PerfRecord {
    size_t  gpuMemUsed;
    size_t  mainMemUsed;
    double averageFrameTimeMs;
};

void RunPerf(std::function<void()> drawFn, int discardFrames, int frameCount, char* filename) {
    ofstream fout(filename);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.12f, 0.6f, 1.0f);

    PerfTimer timer;
    timer.Start();
    while (frameCount > 0) {
        double frameTime = timer.Stop();
        timer.Start();
        if (discardFrames == 0) {
            if (frameCount > 0) {
                fout << (frameTime * 1000.0f) << endl;
                frameCount--;
                if (frameCount == 0) {
                    fout.close();
                }
            }
        } else {
            discardFrames--;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawFn();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

PerfRecord RunPerf2(std::function<void()> setupFn, std::function<void()> drawFn, int discardFrames, int frameCount) {
    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    window = glfwCreateWindow(1920, 1080, "Voxel Perf", NULL, NULL);

    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    glewInit();
    glfwSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.12f, 0.6f, 1.0f);

    size_t gpuFreeBefore = GetFreeMemNvidia();
    size_t memUsedBefore = GetMainMemUsage();
    CheckGLErrors();

    setupFn();
    CheckGLErrors();

    size_t gpuFreeAfter = GetFreeMemNvidia();
    size_t memUsedAfter = GetMainMemUsage();
    CheckGLErrors();

    double totalFrameTime = 0.0;
    double totalRecordedFrames = frameCount;

    PerfTimer timer;
    timer.Start();
    while (frameCount > 0) {
        double frameTime = timer.Stop();
        timer.Start();
        if (discardFrames == 0) {
            if (frameCount > 0) {
                totalFrameTime += frameTime * 1000.0f;
                frameCount--;
            }
        } else {
            discardFrames--;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawFn();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    PerfRecord record;

    record.averageFrameTimeMs = totalFrameTime / totalRecordedFrames;
    record.gpuMemUsed = gpuFreeBefore - gpuFreeAfter;
    record.mainMemUsed = memUsedAfter - memUsedBefore;

    return record;
}

//////////////////////////////////////////////////////////////////////////
// Main
//////////////////////////////////////////////////////////////////////////

void Usage() {
    cerr << "VoxelPerf [dl|vao] <width> <height> <depth>" << endl;
}

int main(int argc, char** argv) {
    if (argc != 5) {
        Usage();
        exit(EXIT_FAILURE);
    }

    string testType = string(argv[1]);
    if (testType != "dl" && testType != "vao") {
        Usage();
        exit(EXIT_FAILURE);
    }

    int w = atoi(argv[2]);
    int h = atoi(argv[3]);
    int d = atoi(argv[4]);

    if (w <= 0 || h <= 0 || d <= 0) {
        Usage();
        exit(EXIT_FAILURE);
    }

    int discardFrames = 32;
    int recordFrames = 128;

    VoxelSet sphere({ 32,32,32 });
    MakeSphere(sphere);

    bool runDlPerf = (testType == "dl");
    bool runVaoPerf = (testType == "vao");

    ivec3 voxelGrid(w, h, d);

    vec3 voxelSpacing(32, 32, 32);
    voxelSpacing *= voxelSize;

    int numObjects = voxelGrid.x * voxelGrid.y * voxelGrid.z;

    // Test display list perf
    if (runDlPerf) {
        std::vector<GLuint> displayLists;
        PerfRecord record = RunPerf2(
            [&]() {
                MakeDisplayListGrid(sphere, voxelGrid, voxelSpacing, displayLists);
            },
            [&]() {
                mat4 mv = MakeModelView();
                mat4 p = MakeProjection();

                glUseProgram(0);
                glMatrixMode(GL_PROJECTION);
                glLoadMatrixf((float*)&p);
                glMatrixMode(GL_MODELVIEW);
                glLoadMatrixf((float*)&mv);

                //glCallLists(displayLists.size(), GL_UNSIGNED_INT, &displayLists[0]);
                for (auto& list : displayLists) {
                    glCallList(list);
                }
            },
            discardFrames,
            recordFrames);
        cout << "display list, " << numObjects << ", " << record.averageFrameTimeMs << ", " << record.gpuMemUsed << ", " << record.mainMemUsed << endl;
    }

    // Test VAO perf
    if (runVaoPerf) {
        GLuint program;
        GLint mvpLoc;
        vector<GLuint> vaos;
        size_t vertexCount;
            
        PerfRecord record = RunPerf2(
            [&]() {
                program = MakeShader();
                mvpLoc = glGetUniformLocation(program, "MVP");
                vertexCount = MakeVaoGrid(sphere, voxelGrid, voxelSpacing, vaos, program);
            },
            [&]() {
                mat4 mvp = MakeMvp();

                glUseProgram(program);
                glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, (const GLfloat*)&mvp);

                for (GLuint vao : vaos) {
                    glBindVertexArray(vao);
                    glDrawArrays(GL_QUADS, 0, vertexCount);
                }
            },
            discardFrames,
            recordFrames);
        cout << "vao, " << numObjects << ", " << record.averageFrameTimeMs << ", " << record.gpuMemUsed << ", " << record.mainMemUsed << endl;
    }

    exit(EXIT_SUCCESS);
}