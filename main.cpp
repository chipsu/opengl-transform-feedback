#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "glad.h"
#include <glm/glm.hpp>
#include <vector>
#include <initializer_list>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <numeric>

#if 0
extern "C" {
	_declspec(dllexport) unsigned long NvOptimusEnablement = 1;
	_declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

const char* vert =
"#version 450 core\n"
"uniform float uTime;\n"
"uniform float uTimeDelta;\n"
"layout(location=0) in vec3 inPos;\n"
"layout(location=1) in vec3 inDir;\n"
"layout(location=2) in float inLife;\n"
"layout(location=3) in float inSize;\n"
"layout(location=0) out vec3 outPos;\n"
"layout(location=1) out vec3 outDir;\n"
"layout(location=2) out float outLife;\n"
"layout(location=3) out float outSize;\n"
"float rand(vec2 co){\n"
"	return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);\n"
"}\n"
"void main() {\n"
"	vec3 dir = inDir + vec3(0,-1,0) * uTimeDelta;\n"
"	vec3 pos = inPos + dir * uTimeDelta;\n"
"	float life = inLife - uTimeDelta;\n"
"	float size = inSize;\n"
"	if(life < 0) { pos = vec3(0,-0.9,0); dir = vec3(rand(vec2(uTime,gl_VertexID))-0.5,1+rand(vec2(uTime,gl_VertexID+1)),0); life=2+rand(vec2(uTime,gl_VertexID+2)); size=rand(vec2(uTime,gl_VertexID+3))*10; }\n"
"	gl_Position = vec4(pos, 1.0);\n"
"	gl_PointSize = size;\n"
"	outPos = pos;\n"
"	outDir = dir;\n"
"	outLife = life;\n"
"	outSize = size;\n"
"}\n";

const char* frag =
"#version 450 core\n"
"layout(location=2) in float inLife;\n"
"layout(location=0) out vec4 outColor;\n"
"void main() {\n"
"	vec2 coord = gl_PointCoord - vec2(0.5);\n"
"	float len = length(coord);\n"
"	if(len > 0.5) discard;\n"
"	outColor = vec4(inLife,gl_PointCoord.x,gl_PointCoord.y,inLife*0.5);\n"
"}\n";

GLuint vertexArrays[2] = { 0 };
GLuint vertexBuffers[2] = { 0 };
GLuint transformFeedback[2] = { 0 };
GLuint activeBuffer = 0;
GLuint indexBuffer = 0;
GLuint uTime = 0;
GLuint uTimeDelta = 0;
float lastUpdate = 0;
bool firstRender = true;
float lastFpsUpdate = 0;
size_t fpsCounter = 0;
char windowTitle[1000];
const size_t numParticles = 100000;

struct Particle {
	glm::vec3 pos = { 0, 0, 0 };
	glm::vec3 dir = { 0, 0, 0 };
	float life = 0;
	float size = 0;
};

void ResizeWindow(GLFWwindow* window) {
	int w, h;
	glfwGetFramebufferSize(window, &w, &h);
	glViewport(0, 0, w, h);
	glfwSwapInterval(0);
};

void InitBuffers() {
	std::vector<Particle> particles;
	particles.resize(numParticles);

	glCreateVertexArrays(2, vertexArrays);
	glCreateBuffers(2, vertexBuffers);
	glCreateTransformFeedbacks(2, transformFeedback);
	for(int i = 0; i < 2; ++i) {
		glBindVertexArray(vertexArrays[i]);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[i]);
		glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), &particles[0], GL_STATIC_DRAW);
		glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedback[i]);
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vertexBuffers[i]);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_TRUE, sizeof(Particle), (GLvoid*)offsetof(Particle, pos));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_TRUE, sizeof(Particle), (GLvoid*)offsetof(Particle, dir));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 1, GL_FLOAT, GL_TRUE, sizeof(Particle), (GLvoid*)offsetof(Particle, life));
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 1, GL_FLOAT, GL_TRUE, sizeof(Particle), (GLvoid*)offsetof(Particle, size));
	}
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

	std::vector<uint32_t> indices(particles.size());
	std::iota(indices.begin(), indices.end(), 1);
	glCreateBuffers(1, &indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), &indices[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

GLuint LoadShaderProgram() {
	auto program = glCreateProgram();

	auto loadShader = [&program](auto src, auto type) {
		auto shader = glCreateShader(type);
		glShaderSource(shader, 1, &src, 0);
		glCompileShader(shader);

		GLint status = 0;
		GLint infoLogLength = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);

		if(infoLogLength > 0) {
			std::string message;
			message.resize(infoLogLength + 1);
			glGetShaderInfoLog(shader, infoLogLength, NULL, &message[0]);
			std::cerr << message << std::endl;
			throw new std::runtime_error(message);
		}

		glAttachShader(program, shader);
	};

	loadShader(vert, GL_VERTEX_SHADER);
	//loadShader(geom, GL_GEOMETRY_SHADER);
	loadShader(frag, GL_FRAGMENT_SHADER);

	const char* feedbackValues[] = { "outPos", "outDir", "outLife", "outSize" };
	glTransformFeedbackVaryings(program, 4, feedbackValues, GL_INTERLEAVED_ATTRIBS);

	glLinkProgram(program);

	GLint status = 0;
	GLint infoLogLength = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
	if(infoLogLength > 0) {
		std::string message;
		message.resize(infoLogLength + 1);
		glGetProgramInfoLog(program, infoLogLength, NULL, &message[0]);
		throw new std::runtime_error(message);
	}

	uTime = glGetUniformLocation(program, "uTime");
	uTimeDelta = glGetUniformLocation(program, "uTimeDelta");

	return program;
}

GLFWwindow* InitWindow() {
	if(!glfwInit()) {
		throw std::runtime_error("glfwInit failed");
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

	auto window = glfwCreateWindow(640, 480, "OpenGL", 0, 0);
	if(!window) {
		throw std::runtime_error("Failed to create window");
	}

	glfwMakeContextCurrent(window);

	if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		throw std::runtime_error("Failed to initialize OpenGL context");
	}

	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height) {
		ResizeWindow(window);
	});

	ResizeWindow(window);

	auto program = LoadShaderProgram();
	glUseProgram(program);

	glClearColor(0, 0, .5, 0);

	return window;
}

void Render(GLFWwindow* window) {
	auto otherBuffer = (activeBuffer + 1) & 0x1;
	auto now = glfwGetTime();
	auto delta = now - lastUpdate;
	lastUpdate = now;
	if(lastFpsUpdate + 1.0f < now) {
		sprintf_s(windowTitle, 1000, "opengl-transform-feedback - FPS: %d, Points: %d", fpsCounter, numParticles);
		glfwSetWindowTitle(window, windowTitle);
		fpsCounter = 0;
		lastFpsUpdate = now;
	}
	fpsCounter++;
	glUniform1f(uTime, now);
	glUniform1f(uTimeDelta, delta);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	//glEnable(GL_RASTERIZER_DISCARD);
	glBindVertexArray(vertexArrays[activeBuffer]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedback[otherBuffer]);

	glBeginTransformFeedback(GL_POINTS);
	if(firstRender) {
		glDrawElements(GL_POINTS, numParticles, GL_UNSIGNED_INT, 0);
		firstRender = false;
	} else {
		glDrawTransformFeedback(GL_POINTS, transformFeedback[activeBuffer]);
	}
	glEndTransformFeedback();
	//glDisable(GL_RASTERIZER_DISCARD);
	//glDrawElements(GL_POINTS, points.size(), GL_UNSIGNED_INT, 0);
	//glDrawTransformFeedback(GL_POINTS, transformFeedback[activeBuffer]);

	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

	glfwSwapBuffers(window);
	activeBuffer = (activeBuffer + 1) & 0x1;
}

void main() {
	auto window = InitWindow();
	InitBuffers();
	lastUpdate = glfwGetTime();
	while(1) {
		glfwPollEvents();
		if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
		Render(window);
	}
}
