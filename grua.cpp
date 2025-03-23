//! ----------
//? *  Grúa  *
//! ----------

//* Autor: Manuel Pereiro Conde

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "lecturaShader_0_9.h"
#include "dibujo.h"

#define pi 3.14159265359
#define MAP_LIMIT 150

//? constantes grua
#define VEL_MAX 60
#define ACEL .5
#define FRENADO 0.3
#define REV_MAX -10
#define GIRO_MAX 60
#define VEL_GIRO 1
#define AMORTIGUACION 0.9f
#define INCLIN_MAX 30.0f
#define VEL_INCLIN 1.0f

//? variables grua
float velocidad = 0;
float ang_giro = 0;
GLfloat anguloRuedaZ = 0.0f; //! rotacion roda para simular desplazamiento
GLfloat anguloRuedaX = 0.0f; //! orientacion parcial roda para simular xiro

//? variables camara
int tipoCamara = 0; // 0: perspectiva, 1: primera persona
float alfa = 0.5;
float beta = 0.5;
float dist_camara = 80.0f;
float vel_camara = 0.01f;

//? variables ventana
int w_width = 1600;
int w_height = 900;

void entradaTeclado(GLFWwindow *window);
extern GLuint setShaders(const char *nVertx, const char *nFrag);
GLuint shaderProgram;

typedef struct
{
	glm::vec3 posicion;
	float angulo_real;
	float inclinacion;
	float velocidad;
	glm::vec3 escala;
	glm::vec3 color;
	unsigned int VAO;
} parteGrua;

parteGrua base = {{0, 1.5, 0.5}, 0, 0, 0, {4, 2, 10}, {.05, .0, .5}, 0};
parteGrua cabina = {{0.0, 2.5, 4}, 0, 0, 0, {4.0f, 3.0f, 2.0}, {.15, .1, .6}, 0};
parteGrua ventana = {{0.0, 2.5, 4.5}, 0, 0, 0, {3.9f, 1.9f, 1.9}, {0.5, 0.8, 1.0}, 0};
parteGrua brazo = {{0, 3, 0}, 0, 0, 0, {0.5, 6, 0.5}, {0.5, 0.4, 0.0}, 0};
parteGrua articulacion = {{-0.4, 1.0, 0}, 35, 0, 0, {1.0, 1.0, 1.0}, {0.5, 0.5, 0.5}, 0};
parteGrua articulacion2 = {{0, 2.5, 0}, 0, 90, 0, {0.7, 0.7, 0.7}, {0.4, 0.4, 0.4}, 0};
parteGrua brazo2 = {{0, 3, 0}, 0, 0, 0, {0.5, 6, 0.5}, {0.5, 0.3, 0.0}, 0};

unsigned int VAO;
unsigned int VAOCuadradoXZ;
unsigned int VAOEsfera;
unsigned int VAOCubo;

double t0 = glfwGetTime();
double t1;
double tdelta;
int nbFrames = 0;

void tiempo()
{
	static float sec = 0;
	t1 = glfwGetTime();
	nbFrames++;
	tdelta = t1 - t0;
	sec += tdelta;

	if (sec >= 1.0)
	{
		std::cout << "FPS: " << nbFrames << std::endl;
		nbFrames = 0;
		sec = 0;
	}
	t0 = t1;
}


//! cámara
void camara()
{

	glUseProgram(shaderProgram);
	glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)w_width / (float)w_height, 0.1f, (float)dist_camara * 20);
	unsigned int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
	glm::vec3 cameraPos, target;

	if (tipoCamara == 0) //! perspectiva (fija)
	{
		cameraPos = glm::vec3(
			(float)dist_camara * sin(alfa) * cos(beta),
			(float)dist_camara * sin(beta),
			(float)dist_camara * cos(alfa) * cos(beta));

		target = glm::vec3(0.0f, 0.0f, 0.0f);
	}
	if (tipoCamara == 1) //! primera persona
	{
		cameraPos = glm::vec3(
			base.posicion.x,
			base.posicion.y + 5,
			base.posicion.z);

		target = cameraPos + glm::vec3(
								 20 * sin(glm::radians(base.angulo_real)),
								 0,
								 20 * cos(glm::radians(base.angulo_real)));
	}

	if (tipoCamara == 2) //! tercera persona
	{
		cameraPos = glm::vec3(
			base.posicion.x - 20 * sin(glm::radians(base.angulo_real)),
			base.posicion.y + 13,
			base.posicion.z - 20 * cos(glm::radians(base.angulo_real)));

		target = glm::vec3(
			base.posicion.x,
			base.posicion.y + 2,
			base.posicion.z);
	}
	if (tipoCamara == 3) //! perspectiva, mirando a grúa
	{
		cameraPos = glm::vec3(
			(float)dist_camara * sin(alfa) * cos(beta),
			(float)dist_camara * sin(beta),
			(float)dist_camara * cos(alfa) * cos(beta));

		target = glm::vec3(
			base.posicion.x,
			base.posicion.y + 2,
			base.posicion.z);
	}
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

	glm::mat4 view = glm::lookAt(cameraPos, target, up);

	unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}



//? --- funcións movimiento grúa ---
//? simula a gravidade no brazo, que cae cara onde esté inclinado
void brazo_aplicar_gravedad()
{
	if (articulacion.inclinacion < 0 && articulacion.inclinacion > -INCLIN_MAX * 1.5)
		articulacion.inclinacion += VEL_INCLIN / 50 * articulacion.inclinacion;
	else if (articulacion.inclinacion > 0 && articulacion.inclinacion < INCLIN_MAX)
		articulacion.inclinacion += VEL_INCLIN / 50 * articulacion.inclinacion;
}

//? simula o xiro da grúa, que se move en función da velocidade da base
void ajustar_giro()
{

	static float angulo_anterior = 0.0f;
	float delta_angulo = base.angulo_real - angulo_anterior;
	angulo_anterior = base.angulo_real;

	base.angulo_real += glm::radians(anguloRuedaX);

	articulacion.angulo_real += delta_angulo * AMORTIGUACION;

	//? se o camion está en movimiento, a palanca tende ir a direccion contraria a que se move, por inercia
	//? se está quieto a palanca tende a seguir na direccion na que está inclinada por gravedad
	if (base.velocidad > 0)
	{
		if (articulacion.inclinacion > -INCLIN_MAX * 1.5)
			articulacion.inclinacion -= VEL_INCLIN;
	}
	else if (base.velocidad < 0)
	{
		if (articulacion.inclinacion < INCLIN_MAX)
			articulacion.inclinacion += VEL_INCLIN;
	}

	if (articulacion.angulo_real > 30.0f)
		articulacion.angulo_real = 30.0f;
	else if (articulacion.angulo_real < -30.0f)
		articulacion.angulo_real = -30.0f;

	articulacion.angulo_real *= 0.95f;
	// o brazo utiliza como matriz de trnasformacion de base o stack da articulacion, asi que se gira a articulacion xirará tamén o brazo
}

//? actualiza a posición da grúa en función da velocidade e controla os límites do mapa
void movimiento()
{
	if (abs(base.velocidad) > 0)
		ajustar_giro();

	base.posicion.x = base.posicion.x + base.velocidad * sin(glm::radians(base.angulo_real)) * tdelta;
	base.posicion.z = base.posicion.z + base.velocidad * cos(glm::radians(base.angulo_real)) * tdelta;

	if (base.posicion.x > MAP_LIMIT)
		base.posicion.x = -MAP_LIMIT;
	if (base.posicion.x < -MAP_LIMIT)
		base.posicion.x = MAP_LIMIT;
	if (base.posicion.z > MAP_LIMIT)
		base.posicion.z = -MAP_LIMIT;
	if (base.posicion.z < -MAP_LIMIT)
		base.posicion.z = MAP_LIMIT;
	anguloRuedaZ += base.velocidad / 3;
	if (anguloRuedaZ >= 360)
		anguloRuedaZ = 0;
}



//* debuxa unha carretera con liñas de separacion de carril
void dibujarCarretera(glm::vec3 pos, float rot, float tam)
{
	glm::mat4 transform;
	glm::mat4 stack;
	unsigned int transformLoc = glGetUniformLocation(shaderProgram, "model");
	unsigned int colorLoc = glGetUniformLocation(shaderProgram, "Color");

 	transform = glm::mat4(1.0f);
	transform = glm::translate(transform, pos);
	transform = glm::rotate(transform, glm::radians(rot), glm::vec3(0.0f, 1.0f, 0.0f));
	stack = transform;  
	transform = glm::scale(transform, glm::vec3(tam, 0.05, 45));
	glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(transform));
	glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(0.3, 0.3, 0.3)));
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glBindVertexArray(VAOCuadradoXZ);
	glDrawArrays(GL_TRIANGLES, 0, 6);

 	glm::vec3 colorLinea(1.0f, 1.0f, 1.0f);

 	transform = stack;
	int num_lineas = tam/13;
	for (int i = 0; i < num_lineas; i++)  
	{
		transform = stack;
		transform = glm::translate(transform, glm::vec3(i * 10 - 75, 0.03, 0));  
		transform = glm::scale(transform, glm::vec3(5, 0.1, 1.5));				 
		glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(transform));
		glUniform3fv(colorLoc, 1, glm::value_ptr(colorLinea));
		glBindVertexArray(VAOCuadradoXZ);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
}

//* debuxa un arbol cun tronco de cubo e unha copa de 3 esferas
void dibujarArbol(glm::vec3 pos, float escala)
{
    glm::mat4 transform;
    glm::mat4 stack;
    unsigned int transformLoc = glGetUniformLocation(shaderProgram, "model");
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "Color");

	//* tronco
    transform = glm::mat4(1.0f);
    transform = glm::translate(transform, pos);
    stack = transform;  
    transform = glm::scale(transform, glm::vec3(escala * 1.5, escala * 8, escala * 1.5));
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(transform));
    glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(0.55f, 0.27f, 0.07f))); // marrón
    glBindVertexArray(VAOCubo);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glm::vec3 colorCopa(0.0f, 0.5f, 0.0f);
	float desplazamiento = escala * 2.5; 

	//* follas
	glm::vec3 posicionesCopa[3] = {
		glm::vec3(-escala, desplazamiento * 2, 0),  // Esfera izquierda
		glm::vec3(escala, desplazamiento * 2, 0),   // Esfera derecha
		glm::vec3(0, desplazamiento * 3, 0) // Esfera superior
	};

    for (int i = 0; i < 3; i++)  
    {
        transform = stack;
        transform = glm::translate(transform, posicionesCopa[i]);
        transform = glm::scale(transform, glm::vec3(escala * 2.5, escala * 2.5, escala * 2.5));
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(transform));
        glUniform3fv(colorLoc, 1, glm::value_ptr(colorCopa));
        glBindVertexArray(VAOEsfera);
        glDrawArrays(GL_TRIANGLES, 0, 1080);
    }
}

//* función que debuxa unha parte da grúa
// precisa os punteiros ás matrices de transformación e un stack
void dibujarParteGrua(parteGrua parte, glm::mat4 *transform, glm::mat4 *stack, unsigned int transformLoc, unsigned int colorLoc, int modificarStack)
{

	*transform = *stack;
	*transform = glm::translate(*transform, parte.posicion);
	*transform = glm::rotate(*transform, glm::radians(parte.inclinacion), glm::vec3(1.0f, 0.0f, 0.0f));
	*transform = glm::rotate(*transform, glm::radians(parte.angulo_real), glm::vec3(0.0f, 1.0f, .0f));
	if (modificarStack)
		*stack = *transform;
	*transform = glm::scale(*transform, parte.escala);
	glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(*transform));
	glUniform3fv(colorLoc, 1, glm::value_ptr(parte.color));
	glBindVertexArray(parte.VAO);
	if (parte.VAO == VAOCubo)
		glDrawArrays(GL_TRIANGLES, 0, 36);
	else
		glDrawArrays(GL_TRIANGLES, 0, 1080);
}


//* debuxa as rodas da grúa
void dibujarRuedas()
{
	glm::mat4 transform;
	glm::mat4 stack;
	unsigned int transformLoc = glGetUniformLocation(shaderProgram, "model");
	unsigned int colorLoc = glGetUniformLocation(shaderProgram, "Color");

	glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(0.0, 0.0, 0.0)));

	transform = glm::mat4(1.0f);
	transform = glm::translate(transform, glm::vec3(base.posicion.x, base.posicion.y, base.posicion.z));
	transform = glm::rotate(transform, glm::radians(base.angulo_real - 90), glm::vec3(0.0f, 1.0f, 0.0f));
	stack = transform;

	glm::vec3 escalaRueda(1.5f, 1.5f, 0.5f);
	float radioRueda = 0.5f;

	glm::vec3 posiciones[4] = {
		{3.5f, -radioRueda, 2.0f},
		{3.5f, -radioRueda, -2.0f},
		{-3.5f, -radioRueda, 2.0f},
		{-3.5f, -radioRueda, -2.0f}};

	for (int i = 0; i < 4; i++)
	{
		transform = stack;
		transform = glm::translate(transform, posiciones[i]);
		if (i < 2)
			// as rodas de diante xiran segun a direccion
			// reducido por 1.2f para que non xire demasiado (queda raro)
			transform = glm::rotate(transform, glm::radians(anguloRuedaX / 1.2f), glm::vec3(0.0f, 1.0f, 0.0f));
		transform = glm::rotate(transform, glm::radians(-anguloRuedaZ), glm::vec3(0.0f, 0.0f, 1.0f));

		transform = glm::scale(transform, escalaRueda); // Aplicar escala
		glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(transform));
		glBindVertexArray(VAOCubo);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}
}

void display()
{
	glUseProgram(shaderProgram);
	unsigned int transformLoc = glGetUniformLocation(shaderProgram, "model");
	unsigned int colorLoc = glGetUniformLocation(shaderProgram, "Color");
	glm::mat4 transform = glm::mat4(1.0f);
	glm::mat4 stack = glm::mat4(1.0f);

	camara();
	tiempo();
	movimiento();

	//* suelo
	transform = glm::translate(transform, glm::vec3(0, 0, 0));
	transform = glm::scale(transform, glm::vec3(1000, 1, 1000));
	glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(transform));
	glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(.2, .7, 0.3)));
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glBindVertexArray(VAOCuadradoXZ);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	transform = glm::mat4(1.0f);

	//* carretera
	dibujarCarretera(glm::vec3(0, 0.05, 100), 0, 210);
	dibujarCarretera(glm::vec3(0, 0.05, -100), 0, 210);
	dibujarCarretera(glm::vec3(-100, 0.05, 0), 90, 210);
	dibujarCarretera(glm::vec3(100, 0.05, 0), 90, 210);

	//* arboles
	dibujarArbol(glm::vec3(150, 0, -50), 1.8);
	dibujarArbol(glm::vec3(-150, 0, 50), 2);
	dibujarArbol(glm::vec3(120, 0, 100), 1.2);
	dibujarArbol(glm::vec3(-160, 0, -80), 2.4);
	dibujarArbol(glm::vec3(80, 0, -150), 2);
	dibujarArbol(glm::vec3(-30, 0, -150), 2);
	dibujarArbol(glm::vec3(150, 0, -30), 2);
	dibujarArbol(glm::vec3(-150, 0, 30), 1.7);

	//* grua
	dibujarParteGrua(base, &transform, &stack, transformLoc, colorLoc, 1);
	dibujarParteGrua(cabina, &transform, &stack, transformLoc, colorLoc, 0);
	dibujarParteGrua(ventana, &transform, &stack, transformLoc, colorLoc, 0);
	dibujarParteGrua(articulacion, &transform, &stack, transformLoc, colorLoc, 1);
	dibujarParteGrua(brazo, &transform, &stack, transformLoc, colorLoc, 1);
	dibujarParteGrua(articulacion2, &transform, &stack, transformLoc, colorLoc, 1);
	dibujarParteGrua(brazo2, &transform, &stack, transformLoc, colorLoc, 0);

	//* ruedas
	dibujarRuedas();

	glBindVertexArray(0);
}





void openGlInit()
{
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
}

//? callback para actualizar tamaño da ventá
void cambioTamaño(GLFWwindow *window, int width, int height)
{
	glViewport(0, 0, width, height);
	w_height = height;
	w_width = width;
	camara();
}

// Main
int main()
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow *window = glfwCreateWindow(w_width, w_height, "Grúa", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Error al crear la ventana" << std::endl;
		glfwTerminate();
		return -1;
	}
	
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, cambioTamaño);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Error al inicializar GLAD" << std::endl;
		return -1;
	}

	openGlInit();
	shaderProgram = setShaders("shader.vert", "shader.frag");


	CuadradoXZ();
	dibujaEsfera();
	dibujaCubo();

	base.VAO = VAOCubo;
	cabina.VAO = VAOCubo;
	brazo.VAO = VAOCubo;
	articulacion.VAO = VAOEsfera;
	articulacion2.VAO = VAOEsfera;
	brazo2.VAO = VAOCubo;
	ventana.VAO = VAOCubo;

	while (!glfwWindowShouldClose(window))
	{
		entradaTeclado(window);

		glClearColor(0.2f, 0.3, 0.5f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		display(); // Llamar a la función que dibuja la escena

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteVertexArrays(1, &VAOCuadradoXZ);
	glDeleteVertexArrays(1, &VAOEsfera);
	glDeleteVertexArrays(1, &VAOCubo);
	glfwTerminate();
	return 0;
}

void entradaTeclado(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS)
	{
		if (dist_camara > 10.0f)
			dist_camara -= 1.0f;
	}
	else if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS)
	{
		if (dist_camara < 200.0f)
			dist_camara += 1.0f;
	}
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{

		if (base.velocidad < VEL_MAX)
		{
			base.velocidad += ACEL;
		}
	}
	else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{

		if (base.velocidad > REV_MAX)
		{
			base.velocidad -= ACEL;
		}
	}
	else
	{
		brazo_aplicar_gravedad();
		if (base.velocidad > 1)
			base.velocidad -= FRENADO;
		else if (base.velocidad < -1)
			base.velocidad += FRENADO;
		else
			base.velocidad = 0;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		if (anguloRuedaX < GIRO_MAX)
			anguloRuedaX += VEL_GIRO;
	}
	else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		if (anguloRuedaX > -GIRO_MAX)
			anguloRuedaX -= VEL_GIRO;
	}
	else
	{
		if (anguloRuedaX > 0)
			anguloRuedaX -= VEL_GIRO;
		else if (anguloRuedaX < 0)
			anguloRuedaX += VEL_GIRO;
	}

	if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
	{
		tipoCamara = 1;
	}
	else if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
	{
		tipoCamara = 0;
	}
	else if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
	{
		tipoCamara = 2;
	}
	else if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS)
	{
		tipoCamara = 3;
	}
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
		if (beta <= 1.5f)
		{
			beta += vel_camara;
		}
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
		if (beta >= 0.1f)
		{
			beta -= vel_camara;
		}
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
		alfa -= vel_camara;
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
		alfa += vel_camara;
}
