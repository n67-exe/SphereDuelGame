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

using cstring = const char*;

// optional second parameter is the message (message is always evaluated) // TODO: fix evaluation
#define DEREF(pointer, ...) \
	internal::deref_impl(pointer, cstring{__VA_ARGS__}, internal::CallInfo{#pointer, __FILE__, __LINE__, __FUNCTION__, FUNCTION_SIGNATURE})

// optional second parameter is the message (message is always evaluated) // TODO: fix evaluation
#define ASSERT(condition, ...) \
	internal::assert_impl(condition, cstring{__VA_ARGS__}, internal::CallInfo{#condition, __FILE__, __LINE__, __FUNCTION__, FUNCTION_SIGNATURE})

namespace internal
{
	// C++ 14 implementation of my C++ 20 implementation based on https://stackoverflow.com/questions/81870/is-it-possible-to-print-the-name-of-a-variables-type-in-standard-c/56766138#56766138
	template <typename T>
	auto type_name() // auto to avoid complicated signature 
	{
		string function = FUNCTION_SIGNATURE, prefix, suffix;

#if   defined(__clang__)
		prefix = "auto internal::type_name() [T = ";
		suffix = "]";
#elif defined(__GNUC__)
		prefix = "auto internal::type_name() [with T = ";
		suffix = "]";
#elif defined(_MSC_VER)
		prefix = "auto __cdecl internal::type_name<";
		suffix = ">(void)";
#endif

		// check that signature prefix and suffix are correct
		const bool starts_with_prefix = (function.size() >= prefix.size() && function.compare(0, prefix.size(), prefix) == 0);
		const bool ends_with_suffix = (function.size() >= suffix.size() && function.compare(function.size() - suffix.size(), suffix.size(), suffix) == 0);

		ASSERT(starts_with_prefix, "Function signature must start with prefix");
		ASSERT(ends_with_suffix, "Function signature must end with suffix");

		return function.substr(prefix.size(), function.size() - prefix.size() - suffix.size()); // strip prefix and suffix
	}

	struct CallInfo
	{
		cstring code;
		cstring file;
		long long line;
		cstring function_name;
		cstring function_signature;
	};

	constexpr cstring filename(cstring path)
	{
		if (path == nullptr)
			return path;

		const char* file = path;

		for (const char* p = path; *p; ++p)
			if (*p == '/' || *p == '\\')
				file = p + 1;

		return file;
	}

	string format_message(cstring category, cstring title, cstring reason, const CallInfo& info)
	{
		constexpr int offset = 12;

		ostringstream message;

		message << left;
		message << setw(offset) << (category ? category : "") << " : " << (title ? title : "") << '\n';

		if (reason)
			message << setw(offset) << "  reason" << " : " << reason << '\n';

		if (info.code)
			message << setw(offset) << "  code" << " : " << info.code << '\n';

		if (info.file)
			message << setw(offset) << "  location" << " : " << filename(info.file) << ":" << info.line << " [" << info.file << "]" << '\n';

		if (info.function_signature)
		{
			if (info.function_name)
				message << setw(offset) << "  function" << " : " << info.function_name << " [" << info.function_signature << "]" << '\n';
			else
				message << setw(offset) << "  function" << " : " << "[" << info.function_signature << "]" << '\n';
		}
		else 
			if (info.function_name)
				message << setw(offset) << "  function" << " : " << info.function_name << '\n';

		return message.str();
	}

	[[noreturn]]
	void report_fatal_error(cstring title, cstring reason, const CallInfo& info)
	{
		clog << '\n' << format_message("FATAL ERROR", title, reason, info) << flush;

		throw runtime_error{"Fatal error occurred"};
	}

	void report_error(cstring title, cstring reason, const CallInfo& info)
	{
		clog << '\n' << format_message("ERROR", title, reason, info) << flush;
	}

	template <typename T>
	T& deref_impl(T* pointer, cstring message, const CallInfo& info)
	{
		if (pointer == nullptr)
			report_fatal_error(("Null pointer dereference [" + type_name<T*>() + "]").c_str(), message, info);

		return *pointer;
	}

	void assert_impl(bool condition, cstring message, const CallInfo& info)
	{
		if (!condition)
			report_error("Condition not satisfied", message, info);
	}
}

struct Vec3
{
	float x = 0, y = 0, z = 0;
};

struct BinaryState
{
	bool value = false, just_changed = false;

	void setNewState(bool new_value) noexcept
	{
		just_changed = (value != new_value);
		value = new_value;
	}

	void keepState() noexcept
	{
		just_changed = false;
	}

	void changeState(bool change = true) noexcept
	{
		just_changed = change;

		if (change)
			value = !value;
	}

	friend bool operator==(const BinaryState& l, const BinaryState& r) noexcept
	{
		return l.value == r.value && l.just_changed == r.just_changed;
	}

	friend bool operator!=(const BinaryState& l, const BinaryState& r) noexcept
	{
		return !(l == r);
	}
};

class GameObject
{
public:
	explicit GameObject(I3DEngine& engine) noexcept
		: m_engine(engine)
	{}

	virtual ~GameObject() noexcept(false) = default;

	GameObject(const GameObject&) = default;
	GameObject(GameObject&&) = default;

	GameObject& operator=(const GameObject& other)
	{
		if (this == &other)
			return *this;

		ASSERT(&m_engine == &other.m_engine); // TODO: message

		return *this;
	}

	GameObject& operator=(GameObject&& other)
	{
		if (this == &other)
			return *this;

		ASSERT(&m_engine == &other.m_engine); // TODO: message

		return *this;
	}

public:
	virtual void updateBegin() {}
	virtual void updateEnd() {}

public:
	virtual ISceneNode& getTransform() = 0;
	virtual const ISceneNode& getTransform() const = 0;

protected:
	I3DEngine& m_engine;
};

class StaticCamera : public GameObject
{
public:
	explicit StaticCamera(I3DEngine& engine)
		: GameObject(engine), m_camera(DEREF(m_engine.CreateCamera(kManual)))
	{}

	/* probably unnecessary
	explicit StaticCamera(const GameObject& object)
		: GameObject(object), m_camera(DEREF(m_engine.CreateCamera(kManual)))
	{}

	explicit StaticCamera(GameObject&& object)
		: GameObject(move(object)), m_camera(DEREF(m_engine.CreateCamera(kManual)))
	{}
	*/

	virtual ~StaticCamera() override
	{
		m_engine.RemoveCamera(&m_camera);
	}

	StaticCamera(const StaticCamera&) = delete;
	StaticCamera(StaticCamera&&) = delete;

	StaticCamera& operator=(const StaticCamera&) = delete;
	StaticCamera& operator=(StaticCamera&&) = delete;

public:
	void renderScene() /* surprisingly can be made const */
	{
		m_engine.DrawScene(&m_camera);
	}

public:
	virtual ISceneNode& getTransform() override
	{
		return m_camera;
	}

	virtual const ISceneNode& getTransform() const override
	{
		return m_camera;
	}

protected:
	ICamera& m_camera;
};

struct ButtonControl
{
	EKeyCode key = kMaxKeyCodes; // kMaxKeyCodes if no key is assigned
	BinaryState state;

	void updateState(I3DEngine& engine)
	{
		if (!engine.IsActive() || key == kMaxKeyCodes)
			return state.setNewState(false);

		state.setNewState(engine.KeyHeld(key));
	}
};

struct ToggleControl
{
	EKeyCode key = kMaxKeyCodes; // kMaxKeyCodes if no key is assigned
	BinaryState state;

	void updateState(I3DEngine& engine)
	{
		if (!engine.IsActive() || key == kMaxKeyCodes)
			return state.keepState();

		state.changeState(engine.KeyHit(key));
	}
};

struct AxisControl
{
	EKeyCode key_pos = kMaxKeyCodes; // kMaxKeyCodes if no key is assigned
	EKeyCode key_neg = kMaxKeyCodes; // kMaxKeyCodes if no key is assigned
	float speed = 1;
	float delta = 0;

	void updateDelta(I3DEngine& engine)
	{
		if (!engine.IsActive())
		{
			delta = 0;
			return;
		}

		const bool held_pos = (key_pos == kMaxKeyCodes ? false : engine.KeyHeld(key_pos));
		const bool held_neg = (key_neg == kMaxKeyCodes ? false : engine.KeyHeld(key_neg));

		if (held_pos && !held_neg)
		{
			delta = speed;
			return;
		}

		if (held_neg && !held_pos)
		{
			delta = -speed;
			return;
		}

		delta = 0;
	}
};

struct MouseControl
{
	float speed = 1; // TODO: 0.05
	float delta_x = 0, delta_y = 0;

	void updateDelta(I3DEngine& engine)
	{
		if (!engine.IsActive())
		{
			engine.GetMouseMovementX();
			engine.GetMouseMovementY();

			delta_x = delta_y = 0;
			return;
		}

		delta_x = static_cast<float>(engine.GetMouseMovementX()) * speed;
		delta_y = static_cast<float>(engine.GetMouseMovementY()) * speed;
	}
};

class KeyboardControlledCamera : public StaticCamera
{
public:
	using StaticCamera::StaticCamera;

public:
	virtual void updateBegin() override 
	{
		m_camera.Move(x_controls.checkDelta(m_engine), y_controls.checkDelta(m_engine), z_controls.checkDelta(m_engine));
	}

public:
	AxisControls x_controls, y_controls, z_controls;
};

void main() try
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
catch (const std::exception& ex)
{
	clog << "\nCaught std::exception:\n";
	clog << ex.what() << flush;
}
catch (...)
{
	clog << "\nCaught unknown exception." << flush;
}
