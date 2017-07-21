#include <stdio.h>
#include <string>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>

#include "Renderer/Vulkan/VulkanRenderer.h"
#include "Renderer/Vulkan/VulkanCommandBuffer.h"

#include <GLFW\glfw3.h>

static void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error %d: %s\n", error, description);
}

int main(int, char**)
{
	// Setup window
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		return 1;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* window = glfwCreateWindow(1080, 720, "Scalpel", NULL, NULL);
	glfwMakeContextCurrent(window);

	//TODO: VULKAN RENDERER TESTING
	VulkanRenderer* v = VulkanRenderer::Get();
	v->Startup();
	v->CreateGLFWSurface(window);

	VulkanCommandBuffer CmdBuffer;
	CmdBuffer.Begin();
	/* ... Rendering Commands Here ... */
	CmdBuffer.Get().setLineWidth(4.0f);
	CmdBuffer.End();


	glClearColor(0, 0, 100, 1);

	// Main loop
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		//TODO: remove OpenGL Stuff
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glfwSwapBuffers(window);
	}

	// Cleanup
	glfwTerminate();

	return 0;
}
