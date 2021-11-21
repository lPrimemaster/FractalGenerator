#include <iostream>
#include "../lib/glad/include/glad/glad.h"

#include "../lib/imgui/imgui.h"
#include "../lib/imgui/imgui_impl_glfw.h"
#include "../lib/imgui/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <gl/GL.h>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <ctime>
#include <cmath>
#include <chrono>
#include <filesystem>

#define CS_NO_ERROR 0x0
#define CS_FILE_NOT_OPENED 0x1
#define CS_SHADER_ERROR 0x2

#define PI 3.1415926535

#define PARTICLE_NUM 50000
#define PARTICLE_NUM_MULT 1
#define SPAWN_S_DENSITY 500

#define NUM_COLORS 3

#define T_SIZE_W 1920
#define T_SIZE_H 1080

// Profile with nsight -> I dont think memory access is very performant 

enum class LinkType
{
    NEW,
    EXISTING
};

static GLuint LoadShaderFromFile(GLuint shadertype, std::string file, GLuint* program, LinkType lt = LinkType::NEW)
{
    std::string content;
    std::ifstream f(file, std::ios::in);

    if(!f.is_open())
    {
        std::cerr << "Could not load file: " << file.c_str() << std::endl;
        return CS_FILE_NOT_OPENED;
    }

    std::string line;
    while(!f.eof())
    {
        std::getline(f, line);
        content.append(line + '\n');
    }

    f.close();

    GLuint cs = glCreateShader(shadertype);

    const char* const source = content.c_str();

    glShaderSource(cs, 1, &source, NULL);
    glCompileShader(cs);

    GLint result = GL_FALSE;
    int logLength;

    // Compile
    glGetShaderiv(cs, GL_COMPILE_STATUS, &result);
    glGetShaderiv(cs, GL_INFO_LOG_LENGTH, &logLength);
    char buffer[1024] = { '\0' };
    glGetShaderInfoLog(cs, logLength, NULL, buffer);
    std::cout << buffer << std::endl;

    if(!result)
        return CS_SHADER_ERROR;

    // Link
    if(lt == LinkType::NEW)
        *program = glCreateProgram();
    
    glAttachShader(*program, cs);
    glLinkProgram(*program);

    glGetProgramiv(*program, GL_LINK_STATUS, &result);
    glGetProgramiv(*program, GL_INFO_LOG_LENGTH, &logLength);

    glGetProgramInfoLog(*program, logLength, NULL, buffer);
    std::cout << buffer << std::endl;

    glDeleteShader(cs);

    return CS_NO_ERROR;
}

struct InitData
{
    GLuint compute_program;
    GLuint render_program;

    GLint tex_w;
    GLint tex_h;
    GLuint texture;
    GLuint fb;
    GLuint rb;

    GLuint rect_vao;
    GLuint rect_vbo;

    GLuint cs_ssbo[2];

    GLint pxl;
    GLint pyl;
    GLint pxld;
    GLint pyld;

    GLint zooml;
    GLint zoomld;
    GLint max_itl;

    GLint d_precl;

    GLint setl;

    GLint cmodel;
    GLint color_gradl;
};

struct BinomialData
{
    unsigned size;
    double* values;
};

static InitData Initialize(GLuint w, GLuint h)
{
    InitData r;
    GLuint p;
    LoadShaderFromFile(GL_COMPUTE_SHADER, "shaders/test.cs.glsl", &p);
    r.compute_program = p;

    LoadShaderFromFile(GL_VERTEX_SHADER, "shaders/test.vert.glsl", &p);
    LoadShaderFromFile(GL_FRAGMENT_SHADER, "shaders/test.frag.glsl", &p, LinkType::EXISTING);
    r.render_program = p;

    r.tex_w = w;
    r.tex_h = h;

    glGenTextures(1, &r.texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Change back to nearest
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // Change back to nearest
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(0, r.texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    // glBindImageTexture(0, r.texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32UI);

    glGenFramebuffers(1, &r.fb);
    glBindFramebuffer(GL_FRAMEBUFFER, r.fb); // Keep always bound!
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r.texture, 0);
    
    glGenRenderbuffers(1, &r.rb);
    glBindRenderbuffer(GL_RENDERBUFFER, r.rb); 
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, T_SIZE_W, T_SIZE_H);  
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, r.rb);

    float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &r.rect_vao);
    glBindVertexArray(r.rect_vao);
    glGenBuffers(1, &r.rect_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r.rect_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint ScreenData[] = {
        w, h
    };

    glGenBuffers(2, &r.cs_ssbo[0]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, r.cs_ssbo[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(GLuint), ScreenData, GL_STATIC_READ);

    glUseProgram(r.compute_program);
    r.pxl = glGetUniformLocation(r.compute_program, "px");
    r.pyl = glGetUniformLocation(r.compute_program, "py");

    r.pxld = glGetUniformLocation(r.compute_program, "pxd");
    r.pyld = glGetUniformLocation(r.compute_program, "pyd");

    r.zooml = glGetUniformLocation(r.compute_program, "zoom");
    r.zoomld = glGetUniformLocation(r.compute_program, "zoomd");
    r.max_itl = glGetUniformLocation(r.compute_program, "iterations");

    r.d_precl = glGetUniformLocation(r.compute_program, "d_prec");

    r.setl = glGetUniformLocation(r.compute_program, "set");

    r.cmodel = glGetUniformLocation(r.compute_program, "cmode");
    r.color_gradl = glGetUniformLocation(r.compute_program, "colorGrad");

    return r;
}

int epoch_min = 0;

void saveFBOImage(InitData& idata)
{
    auto dirname = std::filesystem::current_path() / std::to_string(epoch_min).c_str();

    std::filesystem::create_directory(dirname);
    static int frame = 0;
    FILE* out = fopen((dirname.string() + "/frame" + std::to_string(frame++) + ".ppm").c_str(), "wb");

    float* data = nullptr;
    data = (float*)malloc(T_SIZE_W * T_SIZE_H * 4 * sizeof(float));
    //glNamedFramebufferReadBuffer(idata.fb, GL_COLOR_ATTACHMENT0);
    //glReadPixels(0, 0, T_SIZE_W, T_SIZE_H, GL_RGBA, GL_FLOAT, data);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, idata.texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, data);
    
    //glNamedFramebufferReadBuffer(0, GL_COLOR_ATTACHMENT0);

    // Header
    fprintf(out, "P6\n%d %d\n255\n", T_SIZE_W, T_SIZE_H);

    int k = 0;

    for(int i = 0; i < T_SIZE_W; i++)
    {
        for(int j = 0; j < T_SIZE_H; j++)
        {
            unsigned char local[3] = { (unsigned)(data[k] * 255), (unsigned)(data[k+1] * 255), (unsigned)(data[k+2] * 255) };
            //fprintf(out, "%u %u %u\n", (unsigned)(data[k] * 255), (unsigned)(data[k+1] * 255), (unsigned)(data[k+2] * 255));
            fwrite(local, 1, 3, out);
            //printf("Doing ==> %u %u %u\n", (unsigned)(data[k] * 255), (unsigned)(data[k+1] * 255), (unsigned)(data[k+2] * 255));
            k += 4;
        }
    }

    free(data);

    fclose(out);
}

static void CleanUp(InitData& d)
{
    glDeleteProgram(d.compute_program);
    glDeleteProgram(d.render_program);

    glDeleteTextures(1, &d.texture);

    glDeleteBuffers(1, d.cs_ssbo);
    glDeleteBuffers(1, &d.rect_vbo);

    glDeleteVertexArrays(1, &d.rect_vao);
}

// I like being a bad boy (...)
unsigned set = 0;
double g_scroll = 1;
unsigned iterations = 20;
bool d_prec = false;
bool single_mode = false;
bool dispatch_todo = false;
double lx = 0.0, ly = 0.0;

float single_color[3];
int color_mode = 0;

bool dispatchDone = false;
double curr_mag = 1.0;
double min_mag = 1.0f;
double max_mag = 1.0f;
float mult_frame = 2.0f;
unsigned number_frames = 0;
unsigned c_frame = 0;
std::chrono::steady_clock::time_point tp;
std::chrono::steady_clock::time_point ltp;
double iterations_real = 0.0;

long long runSingleFrameTimed(double mag, InitData& idata)
{
    // Time
    tp = std::chrono::high_resolution_clock::now();
    auto dur = tp - ltp;
    ltp = tp;

    // Setup
    g_scroll = 1.0 / mag;
    iterations_real *= mult_frame;
    //iterations = (unsigned)iterations_real;

    // Dispatch
    dispatch_todo = true;

    // Save last frame
    if(dispatchDone)
    {
        saveFBOImage(idata);
    }
    return dur.count();
}

void scroll_callback(GLFWwindow* w, double sx, double sy)
{
    g_scroll *= exp(-(sy * 0.1f));
}

void ui_window(InitData& idata)
{
    static const char* sets_name[] = {"Mandelbrot", "Burning Ship", "Mandelbrot-3"};
    static const char* color_modes[] = {"Full Hue", "Single Color"};
    static int set_loc = 0;
    static double llx = 0.0, lly = 0.0, llr = 1.0, llm = 0.0;
    static int lls = 10;
    static bool record_win = false;
    static bool poli_win = false;

    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(300, 470));
    ImGui::Begin("Settings", NULL,  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

    ImGui::Text("Alt-F4 to Exit");

    ImGui::Separator();

    ImGui::Text("Iterations [%u]", iterations);

    ImGui::SameLine();
    if (ImGui::Button("+10"))
    {
        iterations += 10;
        if(iterations > 1000000) iterations = 1000000;
    }

    ImGui::SameLine();
    if (ImGui::Button("-10"))
    {
        iterations -= 10;
        if(iterations < 1) iterations = 1;
    }

    ImGui::SameLine();
    if (ImGui::Button("+1"))
    {
        iterations += 1;
    }

    ImGui::SameLine();
    if (ImGui::Button("-1"))
    {
        iterations -= 1;
        if(iterations < 1) iterations = 1;
    }

    ImGui::Text("Mag Level [x%.2e]", 1.0 / g_scroll);
    ImGui::Text("R = %.2e", g_scroll);

    ImGui::Text("Center Coords [%.5e, %.5e]", lx / T_SIZE_W, ly / T_SIZE_H);

    ImGui::Checkbox("Use double precision", &d_prec);

    ImGui::Text("Average %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);

    ImGui::Separator();
    ImGui::Text("Coordinate input");

    ImGui::InputDouble("X", &llx, 0.0, 0.0, "%.20f");
    ImGui::InputDouble("Y", &lly, 0.0, 0.0, "%.20f");
    if(ImGui::InputDouble("R", &llr))
    {
        llm = 1.0 / llr;
    }
    if(ImGui::InputDouble("M", &llm))
    {
        llr = 1.0 / llm;
    }

    ImGui::InputInt("Iterations", &lls);

    if (ImGui::Button("Go!"))
    {
        lx = llx * T_SIZE_W;
        ly = lly * T_SIZE_H;
        g_scroll = llr;
        iterations = lls;
    }

    ImGui::SameLine();
    if (ImGui::Button("Input To Center"))
    {
        llx = lx / T_SIZE_W;
        lly = ly / T_SIZE_H;
        llr = g_scroll;
        lls = iterations;
    }

    ImGui::Checkbox("Single Dispatch Mode", &single_mode);

    if(single_mode)
    {
        if(ImGui::Button("Run!"))
        {
            dispatch_todo = true;
        }
    }

    ImGui::Separator();

    if(ImGui::Combo("Fractal Set", &set_loc, sets_name, IM_ARRAYSIZE(sets_name)))
    {
        set = set_loc;
    }

    ImGui::Separator();

    ImGui::Combo("Color Mode", &color_mode, color_modes, IM_ARRAYSIZE(color_modes));

    if(color_mode == 1)
    {
        ImGui::ColorEdit3("Color", single_color);
    }

    ImGui::Separator();

    if(ImGui::Button(!record_win ? "Open Record Window" : "Close Record Window"))
    {
        record_win = !record_win;
    }

    if(ImGui::Button(!poli_win ? "Open Polinomial Editor Window" : "Close Polinomial Editor Window"))
    {
        poli_win = !poli_win;
    }

    ImGui::End();

    if(record_win)
    {
        ImGui::SetNextWindowPos(ImVec2(320, 10));
        ImGui::SetNextWindowSize(ImVec2(300, 450));
        ImGui::Begin("Record Window", &record_win, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        bool update_frame_info = false;

        ImGui::Text("Capture on specified point");
        update_frame_info |= ImGui::InputDouble("Mag Start", &min_mag);
        update_frame_info |= ImGui::InputDouble("Mag Stop", &max_mag);
        update_frame_info |= ImGui::InputFloat("Multiplier per frame", &mult_frame, 0.0f, 0.0f, "%.2f");

        if(update_frame_info)
        {
            number_frames = log(max_mag / min_mag) / log(mult_frame);
        }

        static bool run_capture = false;

        if(ImGui::Button(!run_capture ? "Start Capture" : "Stop Capture"))
        {
            // Calculate frames
            if(min_mag > max_mag)
            {
                std::cout << "error: Min mag > max mag" << std::endl;
            }
            else
            {
                number_frames = log(max_mag / min_mag) / log(mult_frame);
                std::cout << "Frames to generate: " << number_frames << std::endl;
                std::cout << "Duration: " << number_frames / 30.0 << "s at 30 fps" << std::endl;
                std::cout << "Duration: " << number_frames / 60.0 << "s at 60 fps" << std::endl;

                run_capture = true;
                single_mode = true;
                d_prec = true;
                curr_mag = min_mag;
                iterations_real = iterations;
                epoch_min = std::chrono::duration_cast<std::chrono::minutes>(
                    std::chrono::steady_clock::now().time_since_epoch()
                ).count();
            }
        }

        ImGui::Separator();
        ImGui::Text("Frames to generate: %u", number_frames);
        ImGui::Text("Duration: %fs at 30 fps", number_frames / 30.0);
        ImGui::Text("Duration: %fs at 60 fps", number_frames / 60.0);

        if(curr_mag <= max_mag && run_capture)
        {
            long long ns = runSingleFrameTimed(curr_mag, idata);
            ImGui::Text("Frame Time: %f ms", ns / 1E6f);
            ImGui::Text("Calculating Frame %u/%u", c_frame++, number_frames);
            curr_mag *= mult_frame;
        }
        else if(single_mode && run_capture)
        {
            run_capture = false;
            single_mode = false;
            d_prec = false;
            c_frame = 0;
            iterations_real = 0.0;
        }

        ImGui::End();
        
    }

    if(poli_win)
    {
        ImGui::SetNextWindowPos(ImVec2(T_SIZE_W - 310, 10));
        ImGui::SetNextWindowSize(ImVec2(300, 450));
        ImGui::Begin("Polinomial Window", &poli_win, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        static std::vector<double> powers;
        static std::vector<double> values;
        static int i = 0;

        if(ImGui::Button("Add Parcel"))
        {
            powers.push_back(0.0);
            values.push_back(0.0);
        }

        ImGui::SameLine();
        if(ImGui::Button("Remove Parcel") && powers.size() > 0)
        {
            powers.pop_back();
            values.pop_back();
        }

        for(int j = 0; j < powers.size(); j++)
        {
            ImGui::Separator();
            ImGui::InputDouble(("Power " + std::to_string(j)).c_str(), &powers[j], 0.0 , 0.0, "%.2f");
            ImGui::InputDouble(("Value " + std::to_string(j)).c_str(), &values[j], 0.0 , 0.0, "%.2f");
        }


        if(ImGui::Button("Send To Shader"))
        {
            static BinomialData data;
            data.size = powers.size();
            if(data.values != nullptr)
            {
                free(data.values);
                data.values = nullptr;
            }

            data.values = (double*)malloc(2 * data.size * sizeof(double));
            int k = 0;
            for(int i = 0; i < powers.size(); i++)
            {
                data.values[k] = values[i];
                data.values[k+1] = powers[i];
                k += 2;
            }
            
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, idata.cs_ssbo[1]);
            glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * data.size * sizeof(double) + sizeof(unsigned), &data, GL_STATIC_READ);
        }

        ImGui::End();
    }
}

void GLAPIENTRY
MessageCallback( GLenum source,
                 GLenum type,
                 GLuint id,
                 GLenum severity,
                 GLsizei length,
                 const GLchar* message,
                 const void* userParam )
{
    if(severity != 0x826b)
        fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message );
}

int main()
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(T_SIZE_W, T_SIZE_H, "FractalGenerator", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    InitData idata = Initialize(T_SIZE_W, T_SIZE_H);

    double x, y;
    double rx = 0.0, ry = 0.0;
    bool pressed = false;

    glfwWindowHint(GLFW_SAMPLES, 4);
    glEnable(GL_MULTISAMPLE);

    // During init, enable debug output
    glEnable              ( GL_DEBUG_OUTPUT );
    glDebugMessageCallback( MessageCallback, 0 );

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Poll for and process events */
        glfwPollEvents();

        /* Compute stage 0 */
        glUseProgram(idata.compute_program);

        glUniform1i(idata.d_precl, (int)d_prec);

        glfwGetCursorPos(window, &x, &y);
        glfwSetScrollCallback(window, scroll_callback);

        if(!single_mode)
        {
            if(glfwGetMouseButton(window, 0) == GLFW_PRESS)
            {
                if(!pressed)
                {
                    rx = x;
                    ry = y;
                    pressed = true;
                }
                else
                {
                    lx += (x - rx) * g_scroll;
                    ly += (y - ry) * g_scroll;

                    rx = x;
                    ry = y;
                }
            }
            else if(glfwGetMouseButton(window, 0) == GLFW_RELEASE && pressed)
            {
                pressed = false;
            }

            

            if(glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
            {
                iterations += 1;

                if(iterations > 1000000) iterations = 1000000;
            }
            else if(glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
            {
                iterations -= 1;

                if(iterations < 1) iterations = 1;
            }
        }

        glUniform1d(idata.pxld, lx / T_SIZE_W);
        glUniform1d(idata.pyld, ly / T_SIZE_H);
        glUniform1f(idata.pxl, (float)lx / T_SIZE_W);
        glUniform1f(idata.pyl, (float)ly / T_SIZE_H);
        glUniform1d(idata.zoomld, g_scroll);
        glUniform1f(idata.zooml, (float)g_scroll);
        glUniform1ui(idata.max_itl, iterations);
        glUniform1ui(idata.setl, set);
        glUniform1i(idata.cmodel, color_mode);
        glUniform3fv(idata.color_gradl, 1, single_color);

        if(!single_mode)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, idata.fb);
            glDispatchCompute(T_SIZE_W, T_SIZE_H, 1);
        }
        else if(dispatch_todo)
        {
            dispatchDone = false;
            glBindFramebuffer(GL_FRAMEBUFFER, idata.fb);
            glDispatchCompute(T_SIZE_W, T_SIZE_H, 1);
            dispatch_todo = false;
        }

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        dispatchDone = true;

        /* Render here */
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(idata.render_program);
        glBindVertexArray(idata.rect_vao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, idata.texture);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ui_window(idata);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        /* Swap front and back buffers */
        glfwSwapBuffers(window);
    }

    CleanUp(idata);

    glfwTerminate();
    return 0;
}
