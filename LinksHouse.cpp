// Include standard headers
#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
GLFWwindow* window;
GLuint ProgramID;	// Link the program
GLuint MatrixID;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include <vector>
#include <iostream>
#include <ostream>
#include <cmath>
#include <fstream>
using namespace std;

/****** CAMERA ******/
glm::vec3 cameraPos = {0.5f, 0.4f, 0.5f};     // world position for camera
glm::vec3 cameraFront = {0.0f, 0.0f, -1.0f};
glm::vec3 cameraUp = {0.0f, 1.0f, 0.0f};

// time
float deltaTime = 0.0f;
float lastTime = 0.0f;

/**
 * Given a file path imagepath, read the data in that bitmapped image
 * and return the raw bytes of color in the data pointer.
 * The width and height of the image are returned in the weight and height pointers,
 * respectively.
 *
 * usage:
 *
 * unsigned char* imageData;
 * unsigned int width, height;
 * loadARGB_BMP("mytexture.bmp", &imageData, &width, &height);
 *
 * Modified from https://github.com/opengl-tutorials/ogl.
 */
void loadARGB_BMP(const char* imagepath, unsigned char** data, unsigned int* width, unsigned int* height) {

    printf("Reading image %s\n", imagepath);

    // Data read from the header of the BMP file
    unsigned char header[54];
    unsigned int dataPos;
    unsigned int imageSize;
    // Actual RGBA data

    // Open the file
    FILE * file = fopen(imagepath,"rb");
    if (!file){
        printf("%s could not be opened. Are you in the right directory?\n", imagepath);
        getchar();
        return;
    }

    // Read the header, i.e. the 54 first bytes

    // If less than 54 bytes are read, problem
    if ( fread(header, 1, 54, file)!=54 ){
        printf("Not a correct BMP file1\n");
        fclose(file);
        return;
    }

    // Read the information about the image
    dataPos    = *(int*)&(header[0x0A]);
    imageSize  = *(int*)&(header[0x22]);
    *width      = *(int*)&(header[0x12]);
    *height     = *(int*)&(header[0x16]);
    // A BMP files always begins with "BM"
    if ( header[0]!='B' || header[1]!='M' ){
        printf("Not a correct BMP file2\n");
        fclose(file);
        return;
    }
    // Make sure this is a 32bpp file
    if ( *(int*)&(header[0x1E])!=3  ) {
        printf("Not a correct BMP file3\n");
        fclose(file);
        return;
    }
    // fprintf(stderr, "header[0x1c]: %d\n", *(int*)&(header[0x1c]));
    // if ( *(int*)&(header[0x1C])!=32 ) {
    //     printf("Not a correct BMP file4\n");
    //     fclose(file);
    //     return;
    // }

    // Some BMP files are misformatted, guess missing information
    if (imageSize==0)    imageSize=(*width)* (*height)*4; // 4 : one byte for each Red, Green, Blue, Alpha component
    if (dataPos==0)      dataPos=54; // The BMP header is done that way

    // Create a buffer
    *data = new unsigned char [imageSize];

    if (dataPos != 54) {
        fread(header, 1, dataPos - 54, file);
    }

    // Read the actual data from the file into the buffer
    fread(*data,1,imageSize,file);

    // Everything is in memory now, the file can be closed.
    fclose (file);
}

/****** VertexData ******/
struct VertexData {
    float x;    // position
    float y;
    float z;

    float nx;   // normal veector
    float ny;
    float nz;

    float u;    // texture coordinates
    float v;
};

/****** TriData ******/
struct TriData {
    unsigned int a;    // vertices that make up the triangle
    unsigned int b;
    unsigned int c;
};

/****** Split strings at spaces ******/
vector<string> split(string& line) {
    vector<string> tokens;
    size_t pos = 0;
    string token;
    while ((pos = line.find(" ")) != string::npos) {
        token = line.substr(0, pos);
        tokens.push_back(token);
        line.erase(0, pos + 1);
    }
    tokens.push_back(line);

    return tokens;
}

/****** Reading PLY files ******/
void readPLYFile(std::string fname, std::vector<VertexData>* vertices, std::vector<TriData>* faces) {
    int count = 0;
    int vertexCount = 0;
    int faceCount = 0;
    string line;
    vector<string> tokens;    // temporarily holds strings each loop

    ifstream input(fname);

    while (getline(input, line)) {

        // get element vertex count
        if (count == 3) {
            tokens = split(line);
            vertexCount = stoi(tokens[2]);
        
        // get element face count
        } else if (count == 12) {
            tokens = split(line);
            faceCount = stoi(tokens[2]);
        
        // get all vertex data (first line, 1-indexed, is 16)
        } else if (count >= 15 && count < 15 + vertexCount) {
            tokens = split(line);
            VertexData newVert;
            newVert.x = stof(tokens[0]);
            newVert.y = stof(tokens[1]);
            newVert.z = stof(tokens[2]);

            newVert.nx = stof(tokens[3]);
            newVert.ny = stof(tokens[4]);
            newVert.nz = stof(tokens[5]);

            newVert.u = stof(tokens[6]);
            newVert.v = stof(tokens[7]);
            
            vertices->push_back(newVert);
            
        // get all tridata (last line is empty)
        } else if (count >= 15 + vertexCount && count < 15 + vertexCount + faceCount) {
            tokens = split(line);
            TriData newTri;
            newTri.a = stof(tokens[1]);    // first int isn't part of the coordinates
            newTri.b = stof(tokens[2]);
            newTri.c = stof(tokens[3]);
            
            faces->push_back(newTri);
        }

        tokens.clear();
        count++;
    }

    input.close();

}

/****** TexturedMesh: has PLY and bitmap file paths ******/
class TexturedMesh {
    public:
    std::vector<VertexData> vertices;
    std::vector<TriData> faces;

	unsigned char *imgData {nullptr};	// BMP texture stuff from loadARGB_BMP
	unsigned int imgWidth;
	unsigned int imgHeight;

    GLuint vertexID;    // VBO IDs
    GLuint textureID;
	GLuint normalID;
    GLuint faceID;

    GLuint texObjID;    // texture object made to store bitmap image
    GLuint vaoID;       // to render texture mesh

    GLuint shaderID;    // shader linked to this textured mesh
	

    // constructor
    TexturedMesh(string PLYfilePath, string BMPfilePath) {

		// read files
		const char *BMPcstr = BMPfilePath.c_str();
		readPLYFile(PLYfilePath, &vertices, &faces);
		loadARGB_BMP(BMPcstr, &imgData, &imgWidth, &imgHeight);

		// Texture: generate and bind
		glGenTextures(1, &texObjID);
		glBindTexture(GL_TEXTURE_2D, texObjID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgWidth, imgHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, imgData);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		// bind and generate VAO before binding buffer objects + specifying vertex attributes!
		glGenVertexArrays(1, &vaoID);
		glBindVertexArray(vaoID);
		
		// Vertex data: generate and bind VBOs, fill buffer
		// because we're using glDrawElements, bind to GL_ELEMENT_ARRAY_BUFFER
		// glBufferData final arg = expected use of buffer (drawing)
		glGenBuffers(1, &vertexID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexID);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VertexData), vertices.data(), GL_STATIC_DRAW);
		
		// 1st attribute Vertex positions: pass to shader
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(
			0,                  // attribute, match shader's layout
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,			// normalized?
			sizeof(VertexData),                  // stride
			(void*)offsetof(VertexData, x)		// data
		);

		// 2nd attribute Texture coordinates: pass to shader
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(
			1,                  // attribute, match shader's layout
			2,                  // size
			GL_FLOAT,			// type
			GL_FALSE,           // normalized?
			sizeof(VertexData),                  // stride
			(void*)offsetof(VertexData, u)		// data
		);

		// 3rd attribute Normals: pass to shader
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(
			2,                  // attribute, match shader's layout
			3,                  // size
			GL_FLOAT,			// type
			GL_FALSE,           // normalized?
			sizeof(VertexData),                  // stride
			(void*)offsetof(VertexData, nx)		// data
		);

		// Faces: generate and bind EBOs, fill buffer. No need pass to shader
		glGenBuffers(1, &faceID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, faces.size() * sizeof(TriData), faces.data(), GL_STATIC_DRAW);

		// unbind VAO
		glBindVertexArray(0);

    }

    // renders this object
    void draw(glm::mat4 MVP) {

		glUseProgram(ProgramID);
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

		// re-bind VAO: recover all buffer bindings + vertex attrib specifications (aka state) before drawing
		glBindVertexArray(vaoID);

		glBindTexture(GL_TEXTURE_2D, texObjID);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// call glDrawElements
		glDrawElements(GL_TRIANGLES, faces.size() * 3,  GL_UNSIGNED_INT, 0);

		// unbind/disable things
		glBindVertexArray(0);
		glUseProgram(0);
		glDisable(GL_BLEND);

    }

};


//////////////////////////////////////////////////////////////////////////////
// Main
//////////////////////////////////////////////////////////////////////////////

int main( int argc, char* argv[]) {

	///////////////////////////////////////////////////////
	int currentStep = 1;
	int substep = 1;
	if (argc > 1 ) {
		currentStep = atoi(argv[1]);
	}
	if (argc > 2) {
		substep = atoi(argv[2]);
	}
	///////////////////////////////////////////////////////

	// Initialise GLFW
	if( !glfwInit() )
	{
		fprintf( stderr, "Failed to initialize GLFW\n" );
		getchar();
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	// glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	// glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	// glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	// glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	float screenW = 1400;
	float screenH = 900;
	window = glfwCreateWindow( screenW, screenH, "Link's House - blockheron", NULL, NULL);
	if( window == NULL ){
		fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
	}


	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

	// Dark blue background
	glClearColor(0.2f, 0.2f, 0.3f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

    // Create the shaders
    GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    std::string VertexShaderCode = "\
    	#version 330 core\n\
		// Input vertex data, different for all executions of this shader.\n\
		layout(location = 0) in vec3 vertexPosition;\n\
		layout(location = 1) in vec2 uv;\n\
		layout(location = 2) in vec3 vertexNormal;\n\
		// Output data ; will be interpolated for each fragment.\n\
		out vec2 uv_out;\n\
		// Values that stay constant for the whole mesh.\n\
		uniform mat4 MVP;\n\
		void main(){ \n\
			// Output position of the vertex, in clip space : MVP * position\n\
			gl_Position =  MVP * vec4(vertexPosition,1);\n\
			// The color will be interpolated to produce the color of each fragment\n\
			uv_out = uv;\n\
		}\n";

    // Read the Fragment Shader code from the file
    std::string FragmentShaderCode = "\
		#version 330 core\n\
		in vec2 uv_out; \n\
		uniform sampler2D tex;\n\
		void main() {\n\
			gl_FragColor = texture(tex, uv_out);\n\
		}\n";

	// Compile Vertex Shader
    char const * VertexSourcePointer = VertexShaderCode.c_str();
    glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
    glCompileShader(VertexShaderID);

    // Compile Fragment Shader
    char const * FragmentSourcePointer = FragmentShaderCode.c_str();
    glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
    glCompileShader(FragmentShaderID);

    // Link the program
    ProgramID = glCreateProgram();
    glAttachShader(ProgramID, VertexShaderID);
    glAttachShader(ProgramID, FragmentShaderID);
    glLinkProgram(ProgramID);

    glDetachShader(ProgramID, VertexShaderID);
    glDetachShader(ProgramID, FragmentShaderID);

    glDeleteShader(VertexShaderID);
    glDeleteShader(FragmentShaderID);

	// Get a handle for our "MVP" uniform
	MatrixID = glGetUniformLocation(ProgramID, "MVP");
	glm::mat4 MVP;

	// Get a handle for texture uniform
	GLuint TexID = glGetUniformLocation(ProgramID, "tex");
	glUniform1i(TexID, 0);	// 1 coresponds to glGenTextures


	/****** LOAD OBJECTS ******/
	std::vector<TexturedMesh> textMeshes;

	TexturedMesh bottles("LinksHouse/Bottles.ply", "LinksHouse/bottles.bmp");
	TexturedMesh floor("LinksHouse/Floor.ply", "LinksHouse/floor.bmp");
	TexturedMesh patio("LinksHouse/Patio.ply", "LinksHouse/patio.bmp");
	TexturedMesh table("LinksHouse/Table.ply", "LinksHouse/table.bmp");
	TexturedMesh walls("LinksHouse/Walls.ply", "LinksHouse/walls.bmp");
	TexturedMesh windowbg("LinksHouse/WindowBG.ply", "LinksHouse/windowbg.bmp");
	TexturedMesh woodObjects("LinksHouse/WoodObjects.ply", "LinksHouse/woodobjects.bmp");
	// transparent objects: render last
	TexturedMesh doorbg("LinksHouse/DoorBG.ply", "LinksHouse/doorbg.bmp");
	TexturedMesh metalObjects("LinksHouse/MetalObjects.ply", "LinksHouse/metalobjects.bmp");
	TexturedMesh curtains("LinksHouse/Curtains.ply", "LinksHouse/curtains.bmp");

	textMeshes.push_back(bottles);
	textMeshes.push_back(floor);
	textMeshes.push_back(patio);
	textMeshes.push_back(table);
	textMeshes.push_back(walls);
	textMeshes.push_back(windowbg);
	textMeshes.push_back(woodObjects);
	textMeshes.push_back(doorbg);
	textMeshes.push_back(metalObjects);
	textMeshes.push_back(curtains);
	
    // move + rotate speed
    float speed = 2.5f;
    float yaw = -90.0f;     // initially point at -z axis
    
    /***** RUNNING PROGRAM ON WINDOW ******/
	do {
        // get time
        float currentTime = static_cast<float>(glfwGetTime());
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;     // UPDATE TIME

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glm::mat4 Projection = glm::perspective(glm::radians(45.0f), screenW/screenH, 0.001f, 1000.0f);
		glLoadMatrixf(glm::value_ptr(Projection));

		glMatrixMode( GL_MODELVIEW );
		glPushMatrix();


        // front and back camera movement
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            cameraPos += speed * deltaTime * cameraFront;
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            cameraPos -= speed * deltaTime * cameraFront;
        }

        // left and right camera rotation
         if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            yaw -= deltaTime * speed * 25.0f;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            yaw += deltaTime * speed * 25.0f;
        }

        glm::vec3 direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(0.0f));
        direction.y = sin(glm::radians(0.0f));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(0.0f));
        cameraFront = glm::normalize(direction);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        glm::mat4 M = glm::mat4(1.0f);

		glm::mat4 MV = view * M;
		
		glLoadMatrixf(glm::value_ptr(view));

		MVP = Projection * view * M;

		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_2D);

		// DRAW STUFF HERE
		for (int i = 0; i < textMeshes.size(); i++) {
			textMeshes[i].draw(MVP);
		}

		glBindTexture(GL_TEXTURE_2D, 0);
		// glUseProgram(0);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

    }

	// Check if the ESC key was pressed or the window was closed
	while ( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS && \
            glfwWindowShouldClose(window) == 0 );

	// Cleanup shader
	glDeleteProgram(ProgramID);

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}
