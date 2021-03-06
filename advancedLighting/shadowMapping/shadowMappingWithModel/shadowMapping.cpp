// 引入GLEW库 定义静态链接
#define GLEW_STATIC
#include <GLEW/glew.h>
// 引入GLFW库
#include <GLFW/glfw3.h>
// 引入SOIL库
#include <SOIL/SOIL.h>
// 引入GLM库
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>

// 包含着色器加载库
#include "Shader.h"
// 包含相机控制辅助类
#include "camera.h"
// 包含纹理加载辅助类
#include "texture.h"
// 加载模型的类
#include "model.h"

// 键盘回调函数原型声明
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
// 鼠标移动回调函数原型声明
void mouse_move_callback(GLFWwindow* window, double xpos, double ypos);
// 鼠标滚轮回调函数原型声明
void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// 场景中移动
void do_movement();

// 定义程序常量
const int WINDOW_WIDTH = 800, WINDOW_HEIGHT = 600;
const int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
// 用于相机交互参数
GLfloat lastX = WINDOW_WIDTH / 2.0f, lastY = WINDOW_HEIGHT / 2.0f;
bool firstMouseMove = true;
bool keyPressedStatus[1024]; // 按键情况记录
GLfloat deltaTime = 0.0f; // 当前帧和上一帧的时间差
GLfloat lastFrame = 0.0f; // 上一帧时间
Camera camera(glm::vec3(0.0f, 1.0f, 6.0f));
glm::vec3 lightPos(0.5f, 4.0f, 1.0f);
Model objModel;
GLuint CubeVAOId, CubeVBOId;
GLuint planeVAOId, planeVBOId;
GLuint quadVAOId, quadVBOId;

void prepareVBO();
bool prepareDepthFBO(GLuint& depthTextId, GLuint& fboId);
bool prepareDepthFBO2(GLuint& depthTextId, GLuint& fboId);
void renderScene(Shader &shader);
GLboolean bUseShadow = true;
GLboolean bUsePCF = false;
GLuint planeTextId;

int main(int argc, char** argv)
{
	
	if (!glfwInit())	// 初始化glfw库
	{
		std::cout << "Error::GLFW could not initialize GLFW!" << std::endl;
		return -1;
	}

	// 开启OpenGL 3.3 core profile
	std::cout << "Start OpenGL core profile version 3.3" << std::endl;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	// 创建窗口
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
		"Demo of Shadow Mapping(Press B P to view effect)", NULL, NULL);
	if (!window)
	{
		std::cout << "Error::GLFW could not create winddow!" << std::endl;
		glfwTerminate();
		return -1;
	}
	// 创建的窗口的context指定为当前context
	glfwMakeContextCurrent(window);

	// 注册窗口键盘事件回调函数
	glfwSetKeyCallback(window, key_callback);
	// 注册鼠标事件回调函数
	glfwSetCursorPosCallback(window, mouse_move_callback);
	// 注册鼠标滚轮事件回调函数
	glfwSetScrollCallback(window, mouse_scroll_callback);
	// 鼠标捕获 停留在程序内
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// 初始化GLEW 获取OpenGL函数
	glewExperimental = GL_TRUE; // 让glew获取所有拓展函数
	GLenum status = glewInit();
	if (status != GLEW_OK)
	{
		std::cout << "Error::GLEW glew version:" << glewGetString(GLEW_VERSION) 
			<< " error string:" << glewGetErrorString(status) << std::endl;
		glfwTerminate();
		return -1;
	}

	// 设置视口参数
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	
	// Section1 准备顶点数据和模型数据
	prepareVBO();

	// 为了方便更换模型 我们从文件读取模型文件路径
	std::ifstream modelPath("modelPath.txt");
	if (!modelPath)
	{
		std::cerr << "Error::could not read model path file." << std::endl;
		glfwTerminate();
		std::system("pause");
		return -1;
	}
	std::string modelFilePath;
	std::getline(modelPath, modelFilePath);
	if (!objModel.loadModel(modelFilePath))
	{
		glfwTerminate();
		std::system("pause");
		return -1;
	}
	// Section2 加载纹理
	planeTextId = TextureHelper::load2DTexture("../../resources/textures/wood.png");

	// Section3 准备着色器程序
	Shader shader("scene.vertex", "scene.frag");
	Shader viewDepthShader("viewDepth.vertex", "viewDepth.frag");
	Shader depthMapShader("depthMap.vertex", "depthMap.frag");

	// Section4 准备用于绘制depth-map的FBO
	GLuint depthFBOId, depthTextId;
	if (!prepareDepthFBO2(depthTextId, depthFBOId))
	{
		std::cout << "Error::FBO :" << " not complete." << std::endl;
		glfwTerminate();
		std::system("pause");
		return -1;
	}

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

	glEnable(GL_DEPTH_TEST);
	// 开始游戏主循环
	while (!glfwWindowShouldClose(window))
	{
		GLfloat currentFrame = (GLfloat)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		glfwPollEvents(); // 处理例如鼠标 键盘等事件
		do_movement(); // 根据用户操作情况 更新相机属性
		
		lightPos.z = cos(glfwGetTime()) * 2.0f; // 光源随时间变动

		// Pass1 从光源角度绘制场景到depth-map
		depthMapShader.use();
		// 光源的View-Projection矩阵
		glm::mat4 lightViewMatrix = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
		GLfloat near_plane = 1.0f, far_plane = 10.5f;
		glm::mat4 lightProjMatrix = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
		glm::mat4 lightVPMatrix = lightProjMatrix * lightViewMatrix;
		glUniformMatrix4fv(glGetUniformLocation(depthMapShader.programId, "lightVPMatrix"),
			1, GL_FALSE, glm::value_ptr(lightVPMatrix));
		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthFBOId);
			glClear(GL_DEPTH_BUFFER_BIT);
			renderScene(depthMapShader);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		// Pass2 利用depth-map 渲染光照场景
		glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
		glClearColor(0.18f, 0.04f, 0.14f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		shader.use();
		glm::mat4 projection = glm::perspective(camera.mouse_zoom,
			(GLfloat)(WINDOW_WIDTH) / WINDOW_HEIGHT, 0.1f, 100.0f); // 投影矩阵
		glm::mat4 view = camera.getViewMatrix(); // 视变换矩阵
		// 设置光源属性
		GLint lightAmbientLoc = glGetUniformLocation(shader.programId, "light.ambient");
		GLint lightDiffuseLoc = glGetUniformLocation(shader.programId, "light.diffuse");
		GLint lightSpecularLoc = glGetUniformLocation(shader.programId, "light.specular");
		GLint lightPosLoc = glGetUniformLocation(shader.programId, "light.position");
		glUniform3f(lightAmbientLoc, 0.2f, 0.2f, 0.2f);
		glUniform3f(lightDiffuseLoc, 0.5f, 0.5f, 0.5f);
		glUniform3f(lightSpecularLoc, 1.0f, 1.0f, 1.0f);
		glUniform3f(lightPosLoc, lightPos.x, lightPos.y, lightPos.z);
		// 设置观察者位置
		GLint viewPosLoc = glGetUniformLocation(shader.programId, "viewPos");
		glUniform3f(viewPosLoc, camera.position.x, camera.position.y, camera.position.z);
		// 设置变换矩阵
		glUniformMatrix4fv(glGetUniformLocation(shader.programId, "projection"),
			1, GL_FALSE, glm::value_ptr(projection));
		glUniformMatrix4fv(glGetUniformLocation(shader.programId, "view"),
			1, GL_FALSE, glm::value_ptr(view));
		// 设置变换到光源坐标系的矩阵
		glUniformMatrix4fv(glGetUniformLocation(shader.programId, "lightVPMatrix"),1, GL_FALSE, glm::value_ptr(lightVPMatrix));
		glUniform1i(glGetUniformLocation(shader.programId, "bUseShadow"), bUseShadow); // 如果使用了bool选项 记得传递给着色器
		glUniform1i(glGetUniformLocation(shader.programId, "bUsePCF"), bUsePCF);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, depthTextId);
		glUniform1i(glGetUniformLocation(shader.programId, "depthMap"), 4);
		renderScene(shader);
		glBindVertexArray(0);
		
		// Pass2 利用矩形 可视化 Depth map 作为调试方法
		/*
		glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		viewDepthShader.use();
		glUniform1i(glGetUniformLocation(viewDepthShader.programId, "depthMap"), 0);
		glUniform1f(glGetUniformLocation(viewDepthShader.programId, "nearPlane"), near_plane);
		glUniform1f(glGetUniformLocation(viewDepthShader.programId, "farPlane"), far_plane);
		glActiveTexture(GL_TEXTURE0);
		glBindVertexArray(quadVAOId);
		glBindTexture(GL_TEXTURE_2D, depthTextId);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);
		*/

		glBindVertexArray(0);
		glUseProgram(0);
		glfwSwapBuffers(window); // 交换缓存
	}
	// 释放资源
	glDeleteVertexArrays(1, &planeVAOId);
	glDeleteBuffers(1, &planeVBOId);
	glDeleteVertexArrays(1, &CubeVAOId);
	glDeleteBuffers(1, &CubeVBOId);
	glDeleteVertexArrays(1, &quadVAOId);
	glDeleteBuffers(1, &quadVBOId);
	glfwTerminate();

	return 0;
}
void prepareVBO()
{
	// 指定平面的顶点属性数据 顶点位置 纹理  法向量 
	GLfloat planeVertices[] = {
		25.0f, -0.5f, 25.0f, 25.0f, 0.0f, 0.0f, 1.0f, 0.0f,
		-25.0f, -0.5f, -25.0f, 0.0f, 25.0f, 0.0f, 1.0f, 0.0f,
		-25.0f, -0.5f, 25.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,

		25.0f, -0.5f, 25.0f, 25.0f, 0.0f, 0.0f, 1.0f, 0.0f,
		25.0f, -0.5f, -25.0f, 25.0f, 25.0f, 0.0f, 1.0f, 0.0f,
		-25.0f, -0.5f, -25.0f, 0.0f, 25.0f, 0.0f, 1.0f, 0.0f
	};

	// 创建物体缓存对象
	// Step1: 创建并绑定VAO对象
	glGenVertexArrays(1, &planeVAOId);
	glBindVertexArray(planeVAOId);
	// Step2: 创建并绑定VBO 对象 传送数据
	glGenBuffers(1, &planeVBOId);
	glBindBuffer(GL_ARRAY_BUFFER, planeVBOId);
	glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
	// Step3: 指定解析方式  并启用顶点属性
	// 顶点位置属性
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
		8 * sizeof(GL_FLOAT), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	// 顶点纹理坐标
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
		8 * sizeof(GL_FLOAT), (GLvoid*)(3 * sizeof(GL_FLOAT)));
	glEnableVertexAttribArray(1);
	// 顶点法向量属性
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE,
		8 * sizeof(GL_FLOAT), (GLvoid*)(5 * sizeof(GL_FLOAT)));
	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	// 指定立方体顶点属性数据 顶点位置 纹理 法向量
	GLfloat cubeVertices[] = {
		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,	// A
		0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,	// B
		0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,	// C
		0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,	// C
		-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,	// D
		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,	// A


		-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,	// E
		-0.5f, 0.5f, -0.5f, 0.0, 1.0f, 0.0f, 0.0f, -1.0f, // H
		0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f,	// G
		0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f,	// G
		0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f,	// F
		-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,	// E

		-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f,	// D
		-0.5f, 0.5f, -0.5f, 1.0, 1.0f, -1.0f, 0.0f, 0.0f, // H
		-0.5f, -0.5f, -0.5f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f,	// E
		-0.5f, -0.5f, -0.5f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f,	// E
		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,	// A
		-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f,	// D

		0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,  // F
		0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, // G
		0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, // C
		0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, // C
		0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // B
		0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, // F

		0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f,	// G
		-0.5f, 0.5f, -0.5f, 0.0, 1.0f, 0.0f, 1.0f, 0.0f,    // H
		-0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,	// D
		-0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,	// D
		0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,	// C
		0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f,	// G

		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,  // A
		-0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f,  // E
		0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, // F
		0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, // F
		0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, // B
		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, // A
	};
	// 创建物体缓存对象
	glGenVertexArrays(1, &CubeVAOId);
	glBindVertexArray(CubeVAOId);
	glGenBuffers(1, &CubeVBOId);
	glBindBuffer(GL_ARRAY_BUFFER, CubeVBOId);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
		8 * sizeof(GL_FLOAT), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
		8 * sizeof(GL_FLOAT), (GLvoid*)(3 * sizeof(GL_FLOAT)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE,
		8 * sizeof(GL_FLOAT), (GLvoid*)(5 * sizeof(GL_FLOAT)));
	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	// 指定用于展示depth-map的矩形顶点属性数据 位置 纹理
	GLfloat quadVertices[] = {
		-1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
	};
	glGenVertexArrays(1, &quadVAOId);
	glGenBuffers(1, &quadVBOId);
	glBindVertexArray(quadVAOId);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBOId);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
		5 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
		5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
}
bool prepareDepthFBO(GLuint& depthTextId, GLuint& fboId)
{   
	glGenFramebuffers(1, &fboId);
	glBindFramebuffer(GL_FRAMEBUFFER, fboId);
	glGenTextures(1, &depthTextId);
	glBindTexture(GL_TEXTURE_2D, depthTextId);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 
		SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindFramebuffer(GL_FRAMEBUFFER, fboId);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTextId, 0);
	glDrawBuffer(GL_NONE); // 通知OpenGL这个buffer不用来读写color buffer
	glReadBuffer(GL_NONE);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return false;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return true;
}
bool prepareDepthFBO2(GLuint& depthTextId, GLuint& fboId)
{
	glGenFramebuffers(1, &fboId);
	glBindFramebuffer(GL_FRAMEBUFFER, fboId);
	glGenTextures(1, &depthTextId);
	glBindTexture(GL_TEXTURE_2D, depthTextId);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
		SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); // 截断到边缘 而不是重复
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	// 设置边缘颜色
	GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, fboId);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTextId, 0);
	glDrawBuffer(GL_NONE); // 通知OpenGL这个buffer不用来读写color buffer
	glReadBuffer(GL_NONE);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return false;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return true;
}
void renderScene(Shader &shader)
{
	// 地板平面
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, planeTextId);
	glUniform1i(glGetUniformLocation(shader.programId, "texture_diffuse0"), 0);
	glUniform1i(glGetUniformLocation(shader.programId, "texture_specular0"), 0);
	glm::mat4 model;
	glUniformMatrix4fv(glGetUniformLocation(shader.programId, "model"), 1, GL_FALSE, glm::value_ptr(model));
	glBindVertexArray(planeVAOId);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
	
	// 绘制模型
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -0.5f, 2.0f)); // 适当下调位置
	model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f)); // 适当缩小模型
	glUniformMatrix4fv(glGetUniformLocation(shader.programId, "model"), 1, GL_FALSE, glm::value_ptr(model));
	objModel.draw(shader);
	
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(2.0f, -0.5f, 1.0f));
	model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f)); 
	glUniformMatrix4fv(glGetUniformLocation(shader.programId, "model"), 1, GL_FALSE, glm::value_ptr(model));
	objModel.draw(shader);
	
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-1.0f, -0.5f, 1.5f));
	model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));
	glUniformMatrix4fv(glGetUniformLocation(shader.programId, "model"), 1, GL_FALSE, glm::value_ptr(model));
	objModel.draw(shader);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key >= 0 && key < 1024)
	{
		if (action == GLFW_PRESS)
			keyPressedStatus[key] = true;
		else if (action == GLFW_RELEASE)
			keyPressedStatus[key] = false;
	}
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, GL_TRUE); // 关闭窗口
	}
	else if (key == GLFW_KEY_B && action == GLFW_PRESS)
	{
		bUseShadow = !bUseShadow;
	}
	else if (key == GLFW_KEY_P && action == GLFW_PRESS)
	{
		bUsePCF = !bUsePCF;
	}
}
void mouse_move_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouseMove) // 首次鼠标移动
	{
		lastX = xpos;
		lastY = ypos;
		firstMouseMove = false; 
	}

	GLfloat xoffset = xpos - lastX;
	GLfloat yoffset = lastY - ypos;

	lastX = xpos;
	lastY = ypos;

	camera.handleMouseMove(xoffset, yoffset);
}
// 由相机辅助类处理鼠标滚轮控制
void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.handleMouseScroll(yoffset);
}
// 由相机辅助类处理键盘控制
void do_movement()
{
	
	if (keyPressedStatus[GLFW_KEY_W])
		camera.handleKeyPress(FORWARD, deltaTime);
	if (keyPressedStatus[GLFW_KEY_S])
		camera.handleKeyPress(BACKWARD, deltaTime);
	if (keyPressedStatus[GLFW_KEY_A])
		camera.handleKeyPress(LEFT, deltaTime);
	if (keyPressedStatus[GLFW_KEY_D])
		camera.handleKeyPress(RIGHT, deltaTime);
}