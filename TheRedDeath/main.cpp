// include the basic windows header files and the Direct3D header files
#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx10.h>
#include <ctime>

// include the Direct3D Library file
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dx11.lib")
#pragma comment (lib, "d3dx10.lib")

// define the screen resolution
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600

#define SQUARE(x) ((x)*(x))

// a struct to define a single vertex
struct VERTEX
{
	FLOAT X, Y, Z;                      // Position in world space
	D3DXCOLOR Color;					// UK translation: colour
	FLOAT dX, dY;						// X, Y delta per second
};

#define NUMPOINTS 100000                // How many points to simulate

VERTEX Verts[NUMPOINTS];                // Master list of vertices

// Mouse position in world space
float MouseX, MouseY;

// Flags for whether or not each mouse button is pressed
char LMousePressed;
char RMousePressed;

// global declarations
IDXGISwapChain *swapchain;             // the pointer to the swap chain interface
ID3D11Device *dev;                     // the pointer to our Direct3D device interface
ID3D11DeviceContext *devcon;           // the pointer to our Direct3D device context
ID3D11RenderTargetView *backbuffer;    // the pointer to our back buffer
ID3D11InputLayout *pLayout;            // the pointer to the input layout
ID3D11VertexShader *pVS;               // the pointer to the vertex shader
ID3D11PixelShader *pPS;                // the pointer to the pixel shader
ID3D11Buffer *pVBuffer;                // the pointer to the vertex buffer

// function prototypes
void InitD3D(HWND hWnd);					 // sets up and initializes Direct3D
void RenderFrame(void);						 // renders a single frame
void CleanD3D(void);						 // closes Direct3D and releases memory
void InitGraphics(void);					 // creates the shape to render
void InitPipeline(void);					 // loads and prepares the shaders

// Functions for "Red Death" simulation
void RandomizePoints(void);					 // Assigns each point a random position and velocity
void UpdatePoints(float DeltaSeconds);		 // Ticks each point
void AttractPoints(float AttractionFactor);  // Attracts points to the mouse position.  A negative AttractionFactor will repel points.

// the WindowProc function prototype
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


// the entry point for any Windows program
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
	HWND hWnd;
    WNDCLASSEX wc;

    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"WindowClass";

    RegisterClassEx(&wc);

    RECT wr = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    hWnd = CreateWindowEx(NULL,
                          L"WindowClass",
                          L"The Red Death",
                          WS_OVERLAPPEDWINDOW,
                          300,
                          300,
                          wr.right - wr.left,
                          wr.bottom - wr.top,
                          NULL,
                          NULL,
                          hInstance,
                          NULL);

    ShowWindow(hWnd, nCmdShow);

    // set up and initialize Direct3D
    InitD3D(hWnd);
	
	ZeroMemory(&Verts, sizeof(Verts));

	// Initialize the points
	RandomizePoints();

	// Set both buttons to 'not pressed'
	LMousePressed = 0;
	RMousePressed = 0;

	// Create time vars
	clock_t PrevTime, CurrTime(clock());

    // enter the main loop:
    MSG msg;

    while(TRUE)
    {
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if(msg.message == WM_QUIT)
                break;
        }

		// Grab the time
		PrevTime = CurrTime;
		CurrTime = clock();

		// Update and render, baby!
		UpdatePoints((CurrTime-PrevTime)/1000.f);
        RenderFrame();
    }

    // clean up DirectX and COM
    CleanD3D();
	
    return msg.wParam;
}


// this is the main message handler for the program
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_DESTROY:
            {
                PostQuitMessage(0);
                return 0;
            } break;
		case WM_KEYDOWN:
			{
				// Spacebar randomizes the points
				if((unsigned int)wParam == VK_SPACE)
					RandomizePoints();
			} break;
		case WM_MOUSEMOVE:
			{
				// Grab the mouse position (screen space)
				float ScreenX = (float)LOWORD(lParam);
				float ScreenY = (float)HIWORD(lParam);
				
				// Convert to world space
				MouseX = (ScreenX - SCREEN_WIDTH/2)/(SCREEN_WIDTH/2);
				MouseY = -(ScreenY - SCREEN_HEIGHT/2)/(SCREEN_HEIGHT/2);
			} break;
		case WM_LBUTTONDOWN:
			{
				LMousePressed = 1;
			} break;
		case WM_LBUTTONUP:
			{
				LMousePressed = 0;
			} break;
		case WM_RBUTTONDOWN:
			{
				RMousePressed = 1;
			} break;
		case WM_RBUTTONUP:
			{
				RMousePressed = 0;
			} break;
	}

    return DefWindowProc (hWnd, message, wParam, lParam);
}


// this function initializes and prepares Direct3D for use
void InitD3D(HWND hWnd)
{
    // create a struct to hold information about the swap chain
    DXGI_SWAP_CHAIN_DESC scd;

    // clear out the struct for use
    ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

    // fill the swap chain description struct
    scd.BufferCount = 1;                                   // one back buffer
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;    // use 32-bit color
    scd.BufferDesc.Width = SCREEN_WIDTH;                   // set the back buffer width
    scd.BufferDesc.Height = SCREEN_HEIGHT;                 // set the back buffer height
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;     // how swap chain is to be used
    scd.OutputWindow = hWnd;                               // the window to be used
    scd.SampleDesc.Count = 4;                              // how many multisamples
    scd.Windowed = TRUE;                                   // windowed/full-screen mode
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;    // allow full-screen switching

    // create a device, device context and swap chain using the information in the scd struct
    D3D11CreateDeviceAndSwapChain(NULL,
                                  D3D_DRIVER_TYPE_HARDWARE,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  D3D11_SDK_VERSION,
                                  &scd,
                                  &swapchain,
                                  &dev,
                                  NULL,
                                  &devcon);

	
    // get the address of the back buffer
    ID3D11Texture2D *pBackBuffer;
    swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

    // use the back buffer address to create the render target
    dev->CreateRenderTargetView(pBackBuffer, NULL, &backbuffer);
    pBackBuffer->Release();

    // set the render target as the back buffer
    devcon->OMSetRenderTargets(1, &backbuffer, NULL);

    // Set the viewport
    D3D11_VIEWPORT viewport;
    ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = SCREEN_WIDTH;
    viewport.Height = SCREEN_HEIGHT;

    devcon->RSSetViewports(1, &viewport);
	
    InitPipeline();
    InitGraphics();
}


// this is the function used to render a single frame
void RenderFrame(void)
{
    // copy the vertices into the buffer
    D3D11_MAPPED_SUBRESOURCE ms;
    devcon->Map(pVBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);    // map the buffer
    memcpy(ms.pData, Verts, sizeof(Verts));                 // copy the data
    devcon->Unmap(pVBuffer, NULL);                                      // unmap the buffer	
	
	// clear the back buffer to a deep blue
    devcon->ClearRenderTargetView(backbuffer, D3DXCOLOR(0.f, 0.f, 0.f, 1.0f));

    // select which vertex buffer to display
    UINT stride = sizeof(VERTEX);
    UINT offset = 0;
    devcon->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);

	// Draw all of the balls as points
	devcon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	devcon->Draw(NUMPOINTS,0);

    // switch the back buffer and the front buffer
    swapchain->Present(0, 0);
}

void RandomizePoints(void)
{
	for( int i = 0; i < NUMPOINTS; i++ )
	{
		// Random x,y between -1 and 1
		Verts[i].X = (rand()%200 - 100)/ 100.f;
		Verts[i].Y = (rand()%200 - 100)/ 100.f;

		// Random red-like color
		Verts[i].Color = D3DXCOLOR(1.0f, (rand()%40)/100.f, (rand()%20)/100.f, 1.0f);

		// Random velocity between -0.2 and 0.2 units/second
		Verts[i].dX = (rand()%200-100) / 500.f;
		Verts[i].dY = (rand()%200-100) / 500.f;
	}
}

void UpdatePoints(float DeltaSeconds)
{
	if( LMousePressed )
	{
		// Left clicking deters the points - half strength because with such a small screen area we don't want the points to get pinned to the sides so quickly
		AttractPoints(-0.5f);
	}
	if( RMousePressed )
	{
		// Right clicking attracts the points
		AttractPoints(1.0f);
	}
	
	for( int i = 0; i < NUMPOINTS; i++ )
	{
		// Apply the velocity
		Verts[i].X += Verts[i].dX * DeltaSeconds;
		Verts[i].Y += Verts[i].dY * DeltaSeconds;

		// Check for any collisions on the right wall
		if( Verts[i].X >= 1.0f )
		{
			Verts[i].X = 1.0f;		// Prevent fast moving points from going through the wall
			Verts[i].dX *= -1.0f;   // Bounce off the wall
		}

		// Left wall
		if( Verts[i].X <= -1.0f )
		{
			Verts[i].X = -1.0f;
			Verts[i].dX *= -1.0f;
		}
		
		// Top wall
		if( Verts[i].Y >= 1.0f )
		{
			Verts[i].Y = 1.0f;
			Verts[i].dY *= -1.0f;
		}

		// Bottom wall
		if( Verts[i].Y <= -1.0f )
		{
			Verts[i].Y = -1.0f;
			Verts[i].dY *= -1.0f;
		}
		
	}
}

void AttractPoints(float AttractionFactor)
{
	float newdx, newdy, Dist;

	for( int i = 0; i < NUMPOINTS; i++ )
	{
		// Get vector from vert to mouse
		newdx = MouseX - Verts[i].X;
		newdy = MouseY - Verts[i].Y;

		// Normalize
		Dist = sqrt(SQUARE(newdx) + SQUARE(newdy));
		newdx /= Dist;
		newdy /= Dist;

		// Get distance from vert to mouse
		Dist = sqrt(SQUARE(MouseX - Verts[i].X) + SQUARE(MouseY - Verts[i].Y));

		// Divide by distance so that closer points are affected more
		newdx /= Dist;
		newdy /= Dist;

		// Apply AttractionFactor
		newdx *= AttractionFactor;
		newdy *= AttractionFactor;

		// Set this as the new velocity
		Verts[i].dX = newdx;
		Verts[i].dY = newdy;
	}
}

// this is the function that cleans up Direct3D and COM
void CleanD3D(void)
{
    swapchain->SetFullscreenState(FALSE, NULL);    // switch to windowed mode

    // close and release all existing COM objects
    pLayout->Release();
    pVS->Release();
    pPS->Release();
    pVBuffer->Release();
    swapchain->Release();
    backbuffer->Release();
    dev->Release();
    devcon->Release();
}

// this is the function that creates the shape to render
void InitGraphics()
{
    // create the vertex buffer
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));

    bd.Usage = D3D11_USAGE_DYNAMIC;                // write access access by CPU and GPU
    bd.ByteWidth = sizeof(VERTEX) * NUMPOINTS;      // size is the VERTEX struct * NUMBALLS
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;       // use as a vertex buffer
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;    // allow CPU to write in buffer

    dev->CreateBuffer(&bd, NULL, &pVBuffer);       // create the buffer
}

// this function loads and prepares the shaders
void InitPipeline()
{
    // load and compile the two shaders
    ID3D10Blob *VS, *PS;
    D3DX11CompileFromFile(L"shaders.hlsl", 0, 0, "VShader", "vs_5_0", 0, 0, 0, &VS, 0, 0);
    D3DX11CompileFromFile(L"shaders.hlsl", 0, 0, "PShader", "ps_5_0", 0, 0, 0, &PS, 0, 0);

	// encapsulate both shaders into shader objects
	dev->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), NULL, &pVS);
	dev->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), NULL, &pPS);
	
    // set the shader objects
    devcon->VSSetShader(pVS, 0, 0);
    devcon->PSSetShader(pPS, 0, 0);

    // create the input layout object
    D3D11_INPUT_ELEMENT_DESC ied[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    dev->CreateInputLayout(ied, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &pLayout);
    devcon->IASetInputLayout(pLayout);
}