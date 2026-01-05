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
		cstring code = nullptr;
		cstring file = nullptr;
		long long line = 0;
		cstring function_name = nullptr;
		cstring function_signature = nullptr;
		cstring call_stack = nullptr;
	};

	static constexpr cstring filename(cstring path)
	{
		if (path == nullptr)
			return path;

		const char* file = path;

		for (const char* p = path; *p; ++p)
			if (*p == '/' || *p == '\\')
				file = p + 1;

		return file;
	}

	static string format_message(cstring category, cstring title, cstring reason, const CallInfo& info)
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

		if (info.call_stack)
			message << setw(offset) << "  call stack" << " : " << info.call_stack << '\n';

		return message.str();
	}

	[[noreturn]]
	static void report_fatal_error(cstring title, cstring reason, const CallInfo& info = {})
	{
		clog << '\n' << format_message("FATAL ERROR", title, reason, info) << flush;

		throw runtime_error{"Fatal error occurred"};
	}

	static void report_error(cstring title, cstring reason, const CallInfo& info = {})
	{
		clog << '\n' << format_message("ERROR", title, reason, info) << flush;
	}

	static void report_exception(cstring title, cstring reason, const CallInfo& info = {})
	{
		clog << '\n' << format_message("EXCEPTION", title, reason, info) << flush;
	}

	template <typename T>
	static T& deref_impl(T* pointer, cstring message, const CallInfo& info)
	{
		if (pointer == nullptr)
			report_fatal_error(("Null pointer dereference [" + type_name<T*>() + "]").c_str(), message, info);

		return *pointer;
	}

	static void assert_impl(bool condition, cstring message, const CallInfo& info)
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

struct ButtonControl
{
	EKeyCode key = kMaxKeyCodes; // kMaxKeyCodes if no key is assigned
	BinaryState state;

	void updateState(I3DEngine& engine, bool register_input)
	{
		if (!register_input || key == kMaxKeyCodes)
			return state.setNewState(false);

		state.setNewState(engine.KeyHeld(key));
	}
};

struct ToggleControl
{
	EKeyCode key = kMaxKeyCodes; // kMaxKeyCodes if no key is assigned
	BinaryState state;

	void updateState(I3DEngine& engine, bool register_input)
	{
		if (!register_input || key == kMaxKeyCodes)
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

	void updateDelta(I3DEngine& engine, bool register_input)
	{
		if (!register_input)
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
	float speed = 1;
	float delta_x = 0, delta_y = 0;

	void updateDelta(I3DEngine& engine, bool register_input)
	{
		if (!register_input)
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
	virtual void processInput(bool register_input) {};
	virtual void updateBegin() {};
	virtual void updateEnd() {};

public:
	virtual ISceneNode& getTransform() = 0;
	virtual const ISceneNode& getTransform() const = 0;

protected:
	I3DEngine& m_engine;
};

class StaticModel : public GameObject
{
public:
	explicit StaticModel(I3DEngine& engine, IMesh& mesh, Vec3 position = Vec3{})
		: GameObject(engine), m_model(DEREF(mesh.CreateModel(position.x, position.y, position.z)))
	{}

	virtual ~StaticModel() override
	{
		DEREF(m_model.GetMesh()).RemoveModel(&m_model);
	}

	StaticModel(const StaticModel&) = delete;
	StaticModel(StaticModel&&) = delete;

	StaticModel& operator=(const StaticModel&) = delete;
	StaticModel& operator=(StaticModel&&) = delete;

public:
	void setSkin(const string& filename, int texture = 0)
	{
		m_model.SetSkin(filename, texture);
	}

public:
	virtual ISceneNode& getTransform() override
	{
		return m_model;
	}

	virtual const ISceneNode& getTransform() const override
	{
		return m_model;
	}

protected:
	IModel& m_model;
};

class StaticCamera : public GameObject
{
public:
	explicit StaticCamera(I3DEngine& engine, Vec3 position = Vec3{})
		: GameObject(engine), m_camera(DEREF(m_engine.CreateCamera(kManual, position.x, position.y, position.z)))
	{}

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

class KeyboardControlledCamera : public StaticCamera
{
public:
	using StaticCamera::StaticCamera;

public:
	virtual void processInput(bool register_input) override
	{
		StaticCamera::processInput(register_input);

		x_axis.updateDelta(m_engine, register_input);
		y_axis.updateDelta(m_engine, register_input);
		z_axis.updateDelta(m_engine, register_input);
	}

	virtual void updateBegin() override
	{
		StaticCamera::updateBegin();

		m_camera.Move(x_axis.delta, y_axis.delta, z_axis.delta);
	}

public:
	AxisControl x_axis, y_axis, z_axis;
};

class DebugCamera final : public KeyboardControlledCamera
{
public:
	using KeyboardControlledCamera::KeyboardControlledCamera;

private:
	void moveEuler(float delta, float angle_h, float angle_v)
	{
		static const float deg_to_rad = acos(-1.f) / 180.f;

		angle_h *= deg_to_rad;
		angle_v *= deg_to_rad;

		const float dx = delta * cos(angle_v) * sin(angle_h);
		const float dz = delta * cos(angle_v) * cos(angle_h);
		const float dy = delta * sin(angle_v) * -1;

		m_camera.Move(dx, dy, dz);
	}

public:
	virtual void processInput(bool register_input) override
	{
		KeyboardControlledCamera::processInput(register_input);

		// TODO: do something about toggles
		fly_toggle.updateState(m_engine, register_input);
		mouse_toggle.updateState(m_engine, register_input);
		accelerate_button.updateState(m_engine, register_input);
		mouse_move.updateDelta(m_engine, register_input);

		if (!register_input)
			// not really useful since a bug in TL-Engine hides cursor permanently in this situation
			mouse_toggle.state.setNewState(false);
	}

	virtual void updateBegin() override
	{
		StaticCamera::updateBegin(); // not KeyboardControlledCamera since we redefine the movement logic here

		static float angle_h = 0;
		static float angle_v = 0;

		if (mouse_toggle.state.just_changed)
			mouse_toggle.state.value ? m_engine.StartMouseCapture() : m_engine.StopMouseCapture();

		if (mouse_toggle.state.value)
		{
			angle_h += mouse_move.delta_x;
			angle_v += mouse_move.delta_y;

			angle_h = remainder(remainder(angle_h, 360.f) + 360.f, 360.f);
			angle_v = max(min(angle_v, 90.f), -90.f);

			m_camera.ResetOrientation();

			m_camera.RotateX(angle_v);
			m_camera.RotateY(angle_h);
		}

		const float camera_speed_multiplier = (accelerate_button.state.value ? 3.f : 1.f);

		moveEuler(camera_speed_multiplier * x_axis.delta, angle_h + 90, 0);
		moveEuler(camera_speed_multiplier * z_axis.delta, angle_h, (fly_toggle.state.value ? angle_v : 0));
		moveEuler(camera_speed_multiplier * y_axis.delta, angle_h, (fly_toggle.state.value ? angle_v : 0) - 90);
	}

public:
	ToggleControl fly_toggle, mouse_toggle;
	ButtonControl accelerate_button;
	MouseControl mouse_move;
};

void main() try
{
	// Create a 3D engine (using TLX engine here) and open a window for it
	I3DEngine& engine = DEREF(New3DEngine(kTLX));

	try
	{
		//ASSERT(engine.StartFullscreen(1920, 1080));
		engine.StartWindowed();

		// Add default folder for meshes and other media
		//engine.AddMediaFolder("C:\\ProgramData\\TL-Engine\\Media");
		engine.AddMediaFolder("Media");

		/**** Set up your scene here ****/
		IMesh& water_mesh = DEREF(engine.LoadMesh("water.x"));
		IMesh& island_mesh = DEREF(engine.LoadMesh("island.x"));
		IMesh& skybox_mesh = DEREF(engine.LoadMesh("sky.x"));

		StaticModel water{engine, water_mesh, {0, -5, 0}};
		StaticModel island{engine, island_mesh, {0, -5, 0}};
		StaticModel skybox{engine, skybox_mesh, {0, -960, 0}};

		KeyboardControlledCamera camera_1{engine, {0, 200, 0}};

		camera_1.getTransform().RotateLocalX(90);

		camera_1.x_axis = {Key_Right, Key_Left, 1.5};
		camera_1.z_axis = {Key_Up, Key_Down, 1.5};

		StaticCamera camera_2{engine, {150, 150, -150}};

		camera_2.getTransform().RotateLocalY(-45);
		camera_2.getTransform().RotateLocalX(45);

		DebugCamera debug_camera{engine};

		StaticCamera* active_camera = &debug_camera;

		StaticCamera* cameras[] = {&camera_1, &camera_2, &debug_camera};
		StaticModel* static_objects[] = {&water, &island, &skybox};

		debug_camera.x_axis = {Key_L, Key_J, 1};
		debug_camera.z_axis = {Key_I, Key_K, 1};
		debug_camera.y_axis = {Key_Y, Key_H, 1};

		debug_camera.fly_toggle = {Key_U};
		debug_camera.accelerate_button = {Mouse_RButton};
		debug_camera.mouse_toggle = {Mouse_LButton};
		debug_camera.mouse_move.speed = 0.05f;

		//auto start = std::chrono::steady_clock::now();

		// The main game loop, repeat until engine is stopped
		while (engine.IsRunning())
		{
			const auto start_frame = std::chrono::steady_clock::now();

			const bool active = engine.IsActive();

			if (engine.KeyHit(Key_Escape))
				engine.Stop();

			if (engine.KeyHit(Key_1))
				active_camera = &camera_1;

			if (engine.KeyHit(Key_2))
				active_camera = &camera_2;

			if (engine.KeyHit(Key_0))
				active_camera = &debug_camera;

			for (GameObject* static_object : static_objects)
				DEREF(static_object).processInput(active);

			for (StaticCamera* camera : cameras)
				DEREF(camera).processInput(active && camera == active_camera);

			for (GameObject* static_object : static_objects)
				DEREF(static_object).updateBegin();

			for (StaticCamera* camera : cameras)
				DEREF(camera).updateBegin();

			for (GameObject* static_object : static_objects)
				DEREF(static_object).updateEnd();

			for (StaticCamera* camera : cameras)
				DEREF(camera).updateEnd();

			// Draw the scene
			DEREF(active_camera).renderScene();

			this_thread::sleep_until(start_frame + chrono::duration<int, std::ratio<1, 60>>(1));
		}
	}
	catch (...)
	{
		engine.Delete();

		throw;
	}

	// Delete the 3D engine now we are finished with it
	engine.Delete();
}
catch (const exception& ex)
{
	internal::report_exception("std::exception", ex.what());
}
catch (const CTLException& ex)
{
	// since we use c-style strings, we need to make sure returned std::string will outlive them
	string description = ex.Description();
	string call_stack = ex.CallStack();
	string file_path = ex.FileName();

	const internal::CallInfo info{nullptr, file_path.c_str(), ex.LineNum(), nullptr, nullptr, call_stack.c_str()};

	internal::report_exception("tle::CTLException", description.c_str(), info);

	// not reliable
	// ex.Display();
}
catch (...)
{
	internal::report_exception("unknown", nullptr);
}
