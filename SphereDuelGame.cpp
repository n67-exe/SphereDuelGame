#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include <TL-Engine.h>	// TL-Engine include file and namespace

using namespace tle;
using namespace std;

#ifdef _MSC_VER
#define FUNCTION_SIGNATURE __FUNCSIG__
#else
#define FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#endif // _MSC_VER

namespace internal
{
	template <typename T>
	T& unwrap_impl(T* pointer, const char* file, int line, const char* function)
	{
		if (pointer == nullptr)
			throw runtime_error((string)"ERROR: attempted null pointer dereference at " + file + ":" + to_string(line) + " in " + function + "(...)");

		return *pointer;
	}


	template <typename T>
	const T& unwrap_impl(const T* pointer, const char* file, int line, const char* function)
	{
		if (pointer == nullptr)
			throw runtime_error((string)"ERROR: attempted null pointer dereference at " + file + ":" + to_string(line) + " in " + function + "(...)");

		return *pointer;
	}
}

#define UNWRAP(pointer) internal::unwrap_impl(pointer, __FILE__, __LINE__, __FUNCTION__)

class Camera
{
public:
	Camera(I3DEngine& engine)
		: m_engine(engine)
		, m_camera(UNWRAP(engine.CreateCamera(kManual)))
	{
		
	}

	void attachTo()
	{
		//if (m_camera)
	}

private:
	I3DEngine& m_engine;
	ICamera& m_camera;
	TFloat32 m_x, m_y, m_z; // TODO: vec3
};

void main()
{
	// Create a 3D engine (using TLX engine here) and open a window for it
	I3DEngine& myEngine = *New3DEngine(kTLX);;

	myEngine.StartWindowed();
	myEngine.StartMouseCapture();

	// Add default folder for meshes and other media
	myEngine.AddMediaFolder("C:\\ProgramData\\TL-Engine\\Media");

	/**** Set up your scene here ****/
	IMesh& gridMesh = *myEngine.LoadMesh("Grid.x");
	IMesh& cubeMesh = *myEngine.LoadMesh("Cube.x");
	IMesh& sphereMesh = *myEngine.LoadMesh("Sphere.x");

	IModel& grid = *gridMesh.CreateModel(0, 0, 0);

	IModel& sphere = *sphereMesh.CreateModel(0, 10, 0);

	IModel& cube1 = *cubeMesh.CreateModel(-55, 5, 0);
	IModel& cube2 = *cubeMesh.CreateModel(-55, 15, 0);
	IModel& cube3 = *cubeMesh.CreateModel(55, 5, 0);
	IModel& cube4 = *cubeMesh.CreateModel(55, 15, 0);

	ICamera& camera = *myEngine.CreateCamera(kManual);

	const char* skins[] =
	{
		"Clouds.jpg",
		"Jupiter.jpg",
		"EarthHi.jpg ",
		"Mars.jpg",
		"EarthNight.jpg",
		"Saturn.jpg",
		"EarthPlain.jpg",
		"Pluto.jpg",
	};

	auto nextSkin = [i = 0, &skins]() mutable -> const char*
	{
		if (i >= size(skins))
			i = 0;

		return skins[i++];
	};

	enum class MoveDirection
	{
		Xpos, Xneg,
		Ypos, Yneg,
		Zpos, Zneg,
	};

	const float initialVelocity = 0.5;
	const float initialCameraSpeed = 0.5;
	const float initialCameraRotation = 0.05;

	MoveDirection direction = MoveDirection::Xpos;

	float velocity = initialVelocity;
	float cameraSpeed = initialCameraSpeed;
	float cameraRotation = initialCameraRotation;

	float angle_h = 0;
	float angle_v = 0;

	bool cameraFly = false;
	bool mouseCaptured = true;

	auto cameraMove = [&camera](float delta, float angle_h, float angle_v)
	{
		const float degRad = acos(-1.f) / 180.f;

		angle_h *= degRad;
		angle_v *= degRad;

		float dX = delta * cos(angle_v) * sin(angle_h);
		float dY = delta * sin(angle_v) * -1;
		float dZ = delta * cos(angle_v) * cos(angle_h);

		camera.Move(dX, dY, dZ);
	};

	auto start = std::chrono::steady_clock::now();

	// The main game loop, repeat until engine is stopped
	while (myEngine.IsRunning())
	{
		auto start_frame = std::chrono::steady_clock::now();

		if (myEngine.KeyHit(Key_Escape))
			myEngine.Stop();

		if (myEngine.KeyHit(Key_Space))
			cameraFly = !cameraFly;

		if (myEngine.KeyHit(Key_Tab))
		{
			mouseCaptured = !mouseCaptured;

			mouseCaptured ? myEngine.StartMouseCapture() : myEngine.StopMouseCapture();
		}

		cameraSpeed = initialCameraSpeed * (myEngine.KeyHeld(Key_Shift) ? 3 : 1);

		angle_h += myEngine.GetMouseMovementX() * cameraRotation;
		angle_v += myEngine.GetMouseMovementY() * cameraRotation;

		angle_h = remainder(remainder(angle_h, 360) + 360, 360);
		angle_v = max(min(angle_v, 90.f), -90.f);

		camera.ResetOrientation();

		camera.RotateX(angle_v);
		camera.RotateY(angle_h);

		if (myEngine.KeyHeld(Key_A))
			cameraMove(cameraSpeed, angle_h - 90, 0);

		if (myEngine.KeyHeld(Key_D))
			cameraMove(cameraSpeed, angle_h + 90, 0);

		if (myEngine.KeyHeld(Key_S))
			cameraMove(-cameraSpeed, angle_h, cameraFly ? angle_v : 0);

		if (myEngine.KeyHeld(Key_W))
			cameraMove(cameraSpeed, angle_h, cameraFly ? angle_v : 0);

		if (myEngine.KeyHeld(Key_Q))
			cameraMove(cameraSpeed, angle_h, (cameraFly ? angle_v : 0) + 90);

		if (myEngine.KeyHeld(Key_E))
			cameraMove(cameraSpeed, angle_h, (cameraFly ? angle_v : 0) - 90);

		if (sphere.GetX() > 40)
		{
			direction = MoveDirection::Xneg;

			sphere.SetSkin(nextSkin());
		}

		if (sphere.GetX() < -40)
		{
			direction = MoveDirection::Xpos;

			sphere.SetSkin(nextSkin());
		}

		float velocityX = 0;

		switch (direction)
		{
		break; case MoveDirection::Xpos:
			velocityX = velocity;
		break; case MoveDirection::Xneg:
			velocityX = -velocity;
		}

		sphere.MoveX(velocityX);

		// Draw the scene
		myEngine.DrawScene();

		std::this_thread::sleep_until(start_frame + std::chrono::milliseconds(10));
	}

	// Delete the 3D engine now we are finished with it
	myEngine.Delete();
}
