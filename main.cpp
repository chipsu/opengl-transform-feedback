#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "glad.h"
#include <glm/glm.hpp>
#include <vector>
#include <initializer_list>
#include <cstring>
#include <iostream>
#include <stdexcept>

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
"in vec3 inPos;\n"
"out vec3 outPos;\n"
"void main() {\n"
"vec3 dir = vec3(1.0,0,0);\n"
"vec3 pos = inPos + dir * uTimeDelta;\n"
"if(pos.x > 1.0) pos.x=-1.0f;\n"
"gl_Position = vec4(pos.x,sin(uTime),0,1.0);\n"
"gl_PointSize = 5.0;\n"
"outPos = pos;\n"
"}\n";

//const char* geom =
//"#version 450 core\n"
//"layout(points) in;\n"
//"layout(points) out;\n"
//"layout(max_vertices = 1) out;\n"
//"in vec3 inPos[];\n"
//"out vec3 outPos;\n"
//"void main() {\n"
//"gl_PointSize = 5.0;\n"
//"gl_Position = gl_in[0].gl_Position;\n"
//"outPos = inPos[0];\n"
//"EmitVertex();\n"
//"}\n";

const char* frag =
"#version 450 core\n"
"layout(location=0) out vec4 outColor;\n"
"void main() {\n"
"outColor = vec4(1,0,0,0.5);\n"
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
std::vector<glm::vec3> points = {
	{ 0, 0, 0 },
	{ 0.1, 0, 0 },
	{ 0.2, 0, 0 },
	{ 0.3, 0, 0 },
	{ 0.4, 0, 0 },
};

void resize(GLFWwindow* window) {
	int w, h;
	glfwGetFramebufferSize(window, &w, &h);
	glViewport(0, 0, w, h);
	glfwSwapInterval(0);
};

void init_buffers() {
	glCreateVertexArrays(2, vertexArrays);
	glCreateBuffers(2, vertexBuffers);
	glCreateTransformFeedbacks(2, transformFeedback);
	for(int i = 0; i < 2; ++i) {
		glBindVertexArray(vertexArrays[i]);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[i]);
		glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(glm::vec3), &points[0], GL_STATIC_DRAW);
		glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedback[i]);
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vertexBuffers[i]);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
	}
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

	std::vector<uint32_t> indices;
	for(int i = 0; i < points.size(); ++i) {
		indices.push_back(i);
	}
	glCreateBuffers(1, &indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), &indices[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

GLuint load_program() {
	auto program = glCreateProgram();

	auto load_shader = [&program](auto src, auto type) {
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

	load_shader(vert, GL_VERTEX_SHADER);
	//load_shader(geom, GL_GEOMETRY_SHADER);
	load_shader(frag, GL_FRAGMENT_SHADER);

	const char* feedbackValues[] = { "outPos" };
	glTransformFeedbackVaryings(program, 1, feedbackValues, GL_INTERLEAVED_ATTRIBS);

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

GLFWwindow* setup() {
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

	glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height) {
		resize(window);
		});

	resize(window);

	auto program = load_program();
	glUseProgram(program);

	glClearColor(0, 0, .5, 0);

	return window;
}

void render(GLFWwindow* window) {
	auto otherBuffer = (activeBuffer + 1) & 0x1;
	auto now = glfwGetTime();
	auto delta = now - lastUpdate;
	lastUpdate = now;
	glUniform1f(uTime, now);
	glUniform1f(uTimeDelta, delta);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	//glEnable(GL_RASTERIZER_DISCARD);
	glBindVertexArray(vertexArrays[activeBuffer]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedback[otherBuffer]);

	glBeginTransformFeedback(GL_POINTS);
	if(firstRender) {
		glDrawElements(GL_POINTS, points.size(), GL_UNSIGNED_INT, 0);
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
	auto window = setup();
	init_buffers();
	lastUpdate = glfwGetTime();
	while(1) {
		glfwPollEvents();
		if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
		render(window);
	}
}
