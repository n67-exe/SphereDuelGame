#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <TL-Engine.h>

using namespace tle;
using namespace std;

#define RANGE(r) begin(r), end(r)

// full function signature (readable)
#ifdef _MSC_VER
#define FUNCTION_SIGNATURE __FUNCSIG__
#else
#define FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#endif // _MSC_VER

// c-style string
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
	auto type_name() // auto to avoid complicated signature, returns std::string
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

	// information provided by the caller
	struct CallInfo
	{
		cstring code = nullptr;
		cstring file = nullptr;
		long long line = 0;
		cstring function_name = nullptr;
		cstring function_signature = nullptr;
		cstring call_stack = nullptr;
	};

	// extract filename from path
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

	static string format_message(cstring category, cstring title, cstring description, const CallInfo& info)
	{
		constexpr int offset = 13;

		ostringstream message;

		message << left;
		message << setw(offset) << (category ? category : "") << " : " << (title ? title : "") << '\n';

		if (description)
			message << setw(offset) << "  description" << " : " << description << '\n';

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
	static void report_fatal_error(cstring title, cstring description, const CallInfo& info = {})
	{
		clog << '\n' << format_message("FATAL ERROR", title, description, info) << flush;

		throw runtime_error{"Fatal error"};
	}

	static void report_error(cstring title, cstring description, const CallInfo& info = {})
	{
		clog << '\n' << format_message("ERROR", title, description, info) << flush;
	}

	static void report_exception(cstring title, cstring description, const CallInfo& info = {})
	{
		clog << '\n' << format_message("EXCEPTION", title, description, info) << flush;
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

// data from https://www.wolframalpha.com
namespace numbers
{
	template <typename T>
	constexpr T pi_v = 3.141592653589793238462643383279503L; // = pi

	template <typename T>
	constexpr T deg_to_rad_v = 0.01745329251994329576923690768488613L; // = 2pi / 360

	template <typename T>
	constexpr T rad_to_deg_v = 57.29577951308232087679815481410517L; // = 360 / 2pi

	template <typename T>
	constexpr T largest_v = numeric_limits<T>::has_infinity ? numeric_limits<T>::infinity() : numeric_limits<T>::max(); // largest possible number

	constexpr float pi = pi_v<float>;
	constexpr float deg_to_rad = deg_to_rad_v<float>;
	constexpr float rad_to_deg = rad_to_deg_v<float>;
	constexpr float largest = largest_v<float>;
}

struct Vec3
{
	float x = 0, y = 0, z = 0;

public:
	float length() const
	{
		return hypot(hypot(x, y), z);
	}

	float squaredLength() const noexcept
	{
		return x * x + y * y + z * z;
	}

public:
	friend bool operator==(Vec3 l, Vec3 r)
	{
		return l.x == r.x && l.y == r.y && l.z == r.z;
	}

	friend bool operator!=(Vec3 l, Vec3 r)
	{
		return !(l == r);
	}

public:
	// +Vector
	friend Vec3 operator+(Vec3 v) noexcept
	{
		return v;
	}

	// -Vector
	friend Vec3 operator-(Vec3 v) noexcept
	{
		return Vec3{-v.x, -v.y, -v.z};
	}

	// Vector + Vector
	friend Vec3 operator+(Vec3 a, Vec3 b) noexcept
	{
		return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
	}

	// Vector - Vector
	friend Vec3 operator-(Vec3 a, Vec3 b) noexcept
	{
		return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
	}

	// Vector * Scalar
	friend Vec3 operator*(Vec3 v, float s) noexcept
	{
		return Vec3{v.x * s, v.y * s, v.z * s};
	}

	// Scalar * Vector
	friend Vec3 operator*(float s, Vec3 v) noexcept
	{
		return Vec3{v.x * s, v.y * s, v.z * s};
	}

	// Vector / Scalar
	friend Vec3 operator/(Vec3 v, float s) noexcept
	{
		return Vec3{v.x / s, v.y / s, v.z / s};
	}

public:
	friend float dot_product(Vec3 a, Vec3 b) noexcept
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}
};

Vec3 getPosition(const ISceneNode& node)
{
	return Vec3{node.GetX(), node.GetY(), node.GetZ()};
}

void setPosition(ISceneNode& node, Vec3 position)
{
	node.SetPosition(position.x, position.y, position.z);
}

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

// ON when actively pressed
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

// switches between ON and OFF when pressed
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

// has 2 buttons that increase and decrease the value, reports delta
struct AxisControl
{
	EKeyCode key_pos = kMaxKeyCodes; // kMaxKeyCodes if no key is assigned
	EKeyCode key_neg = kMaxKeyCodes; // kMaxKeyCodes if no key is assigned
	float multiplier = 1;
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
			delta = multiplier;
			return;
		}

		if (held_neg && !held_pos)
		{
			delta = -multiplier;
			return;
		}

		delta = 0;
	}
};

// processes mouse X and Y movement, reports delta
struct MouseControl
{
	float multiplier = 1;
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

		delta_x = static_cast<float>(engine.GetMouseMovementX()) * multiplier;
		delta_y = static_cast<float>(engine.GetMouseMovementY()) * multiplier;
	}
};

// abstract game engine object, transformable
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

class DynamicModel : public StaticModel
{
public:
	explicit DynamicModel(I3DEngine& engine, IMesh& mesh, Vec3 position = Vec3{}, float radius = 0)
		: StaticModel(engine, mesh, position), radius(radius)
	{}

public:
	friend float timeOfCollision(const DynamicModel& object_1, float time_1, const DynamicModel& object_2, float time_2)
	{
		const Vec3 d_p = (getPosition(object_1.getTransform()) - getPosition(object_2.getTransform())) - object_2.velocity * (time_1 - time_2);
		const Vec3 d_v = object_1.velocity - object_2.velocity;

		if (d_v == Vec3{})
			return numbers::largest;

		const float pv = dot_product(d_p, d_v);
		const float v2 = d_v.squaredLength();
		const float r = object_1.radius + object_2.radius;

		const float D = (pv * pv) - (v2 * (d_p.squaredLength() - (r * r)));

		ASSERT(!isnan(D));

		if (D <= 0)
			return numbers::largest;

		const float t = -(pv + sqrt(D)) / v2;

		ASSERT(!isnan(t));

		if (t < 0)
			return numbers::largest;

		return time_1 + t;
	}

	friend void resolveCollision(DynamicModel& object_1, DynamicModel& object_2, float force = 0)
	{
		ASSERT(force >= 0); // or it will crash

		const Vec3 d_p = getPosition(object_1.getTransform()) - getPosition(object_2.getTransform());

		const Vec3 dir = d_p / d_p.length();

		object_1.velocity = object_1.velocity - (2 * dot_product(object_1.velocity, dir) - force) * dir;
		object_2.velocity = object_2.velocity - (2 * dot_product(object_2.velocity, dir) + force) * dir;
	}

public:
	Vec3 velocity;
	float radius;
};

class SphereDynamicModel : public DynamicModel
{
public:
	explicit SphereDynamicModel(I3DEngine& engine, IMesh& mesh, string normal_skin, string hyper_skin, Vec3 position = Vec3{}, float radius = 0)
		: DynamicModel(engine, mesh, position, radius), m_normal_skin(move(normal_skin)), m_hyper_skin(move(hyper_skin))
	{
		setSkin(m_normal_skin);
	}

public:
	void setOrientation(float angle_horizontal = 0)
	{
		m_angle = remainder(remainder(angle_horizontal, 360.f) + 360.f, 360.f);

		m_model.ResetOrientation();

		m_model.RotateY(m_angle);
	}

public:
	virtual void updateBegin() override
	{
		DynamicModel::updateBegin();

		setOrientation(m_angle + rotation_axis.delta);

		const float angle_rad = m_angle * numbers::deg_to_rad;

		velocity = velocity + Vec3{sin(angle_rad), 0, cos(angle_rad)} * forward_axis.delta;
	}

public:
	AxisControl forward_axis, rotation_axis;
	int points = 0;
	bool dead = false;

protected:
	float m_angle = 0;

	string m_normal_skin, m_hyper_skin;
};

class PlayerSphereDynamicModel final : public SphereDynamicModel
{
public:
	using SphereDynamicModel::SphereDynamicModel;

public:
	virtual void processInput(bool register_input) override
	{
		SphereDynamicModel::processInput(register_input);

		forward_axis.updateDelta(m_engine, register_input);
		rotation_axis.updateDelta(m_engine, register_input);
	}
};

class EnemySphereDynamicModel final : public SphereDynamicModel
{
public:
	using SphereDynamicModel::SphereDynamicModel;

public:
	virtual void processInput(bool register_input) override
	{
		SphereDynamicModel::processInput(register_input);

		// TODO: proper logic
		static uniform_int_distribution<> dist{0, 1};

		forward_axis.delta = forward_axis.multiplier * dist(random_generator);
		rotation_axis.delta = rotation_axis.multiplier * dist(random_generator);
	}

public:
	minstd_rand random_generator;
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

	// TODO: bounds
};

class DebugCamera final : public KeyboardControlledCamera
{
public:
	using KeyboardControlledCamera::KeyboardControlledCamera;

private:
	// move towards a direction defined in spherical coordinates
	void moveSpherical(float distance, float angle_h, float angle_v)
	{
		angle_h *= numbers::deg_to_rad;
		angle_v *= numbers::deg_to_rad;

		const float dx = distance * cos(angle_v) * sin(angle_h);
		const float dz = distance * cos(angle_v) * cos(angle_h);
		const float dy = distance * sin(angle_v) * -1;

		m_camera.Move(dx, dy, dz);
	}

public:
	void setOrientation(float angle_horizontal = 0, float angle_vertical = 0)
	{
		m_angle_h = remainder(remainder(angle_horizontal, 360.f) + 360.f, 360.f);
		m_angle_v = max(min(angle_vertical, 90.f), -90.f);

		m_camera.ResetOrientation();

		m_camera.RotateX(m_angle_v);
		m_camera.RotateY(m_angle_h);
	}

public:
	virtual void processInput(bool register_input) override
	{
		KeyboardControlledCamera::processInput(register_input);

		fly_toggle.updateState(m_engine, register_input);
		mouse_toggle.updateState(m_engine, register_input);
		accelerate_button.updateState(m_engine, register_input);
		mouse_move.updateDelta(m_engine, register_input);

		// stop mouse capture if we start ignoring user input
		// if the window loses focus, a bug in TL-Engine hides the cursor permanently over the window
		if (!register_input)
			mouse_toggle.state.setNewState(false);

		if (mouse_toggle.state.just_changed)
			mouse_toggle.state.value ? m_engine.StartMouseCapture() : m_engine.StopMouseCapture();
	}

	virtual void updateBegin() override
	{
		StaticCamera::updateBegin(); // not KeyboardControlledCamera since we redefine the movement logic here

		if (mouse_toggle.state.value)
			setOrientation(m_angle_h + mouse_move.delta_x, m_angle_v + mouse_move.delta_y);

		const float camera_speed_multiplier = (accelerate_button.state.value ? acceleration_multiplier : 1.f);

		moveSpherical(camera_speed_multiplier * x_axis.delta, m_angle_h + 90, 0);
		moveSpherical(camera_speed_multiplier * z_axis.delta, m_angle_h, (fly_toggle.state.value ? m_angle_v : 0));
		moveSpherical(camera_speed_multiplier * y_axis.delta, m_angle_h, (fly_toggle.state.value ? m_angle_v : 0) - 90);
	}

public:
	ToggleControl fly_toggle, mouse_toggle;
	ButtonControl accelerate_button;
	MouseControl mouse_move;

	float acceleration_multiplier = 2;

protected:
	float m_angle_h = 0, m_angle_v = 0;
};

class DynamicModelManager
{
public:
	explicit DynamicModelManager(I3DEngine& engine, IMesh& mesh, const string& normal_skin, const string& hyper_skin, int count, float cube_radius)
		: m_hypercube(engine, mesh, {}, cube_radius), m_cubes(count - 1, nullptr)
	{
		ASSERT(count > 0);

		m_hypercube.setSkin(hyper_skin);

		for (DynamicModel*& cube : m_cubes)
		{
			cube = new DynamicModel{engine, mesh, {}, cube_radius};

			DEREF(cube).setSkin(normal_skin);
		}
	}

	~DynamicModelManager()
	{
		for (DynamicModel* cube : m_cubes)
			delete cube;
	}

	DynamicModelManager(const DynamicModelManager&) = delete;
	DynamicModelManager(DynamicModelManager&&) = delete;

	DynamicModelManager& operator=(const DynamicModelManager&) = delete;
	DynamicModelManager& operator=(DynamicModelManager&&) = delete;

private:
	int cubeCount() const
	{
		return m_cubes.size() + 1;
	}

	DynamicModel& getCube(int index)
	{
		ASSERT(index <= m_cubes.size());

		return (index == m_cubes.size() ? m_hypercube : DEREF(m_cubes[index]));
	}

	const DynamicModel& getCube(int index) const
	{
		ASSERT(index <= m_cubes.size());

		return (index == m_cubes.size() ? m_hypercube : DEREF(m_cubes[index]));
	}

	bool withinBounds(Vec3 point) const noexcept
	{
		if (bounds_from.x != bounds_to.x)
			if (bounds_from.x > point.x || point.x > bounds_to.x)
				return false;

		if (bounds_from.y != bounds_to.y)
			if (bounds_from.y > point.y || point.y > bounds_to.y)
				return false;

		if (bounds_from.z != bounds_to.z)
			if (bounds_from.z > point.z || point.z > bounds_to.z)
				return false;

		return true;
	}

	Vec3 getNextSpawn(const SphereDynamicModel* player_ptr, const SphereDynamicModel* enemy_ptr, float margin)
	{
		const float x_margin = (bounds_from.x != bounds_to.x) ? margin : 0;
		const float y_margin = (bounds_from.y != bounds_to.y) ? margin : 0;
		const float z_margin = (bounds_from.z != bounds_to.z) ? margin : 0;

		uniform_real_distribution<float> x_distribution{bounds_from.x + x_margin, bounds_to.x - x_margin};
		uniform_real_distribution<float> y_distribution{bounds_from.y + y_margin, bounds_to.y - y_margin};
		uniform_real_distribution<float> z_distribution{bounds_from.z + z_margin, bounds_to.z - z_margin};

		Vec3 position;

		RETRY:
		{
			position.x = x_distribution(random_generator);
			position.y = y_distribution(random_generator);
			position.z = z_distribution(random_generator);

			for (int i = 0; i < cubeCount(); i++)
			{
				DynamicModel& cube = getCube(i);

				if ((position - getPosition(getCube(i).getTransform())).length() < separation + cube.radius)
					goto RETRY;
			}

			if (player_ptr)
			{
				const SphereDynamicModel& player = DEREF(player_ptr);

				if ((position - getPosition(player.getTransform())).length() < separation + player.radius)
					goto RETRY;
			}

			if (enemy_ptr)
			{
				const SphereDynamicModel& enemy = DEREF(enemy_ptr);

				if ((position - getPosition(enemy.getTransform())).length() < separation + enemy.radius)
					goto RETRY;
			}
		}

		return position;
	}

public:
	void respawnAll(const SphereDynamicModel* player_ptr, const SphereDynamicModel* enemy_ptr)
	{
		const Vec3 outside = bounds_to + Vec3{separation, separation, separation};

		for (int i = 0; i < cubeCount(); i++)
		{
			DynamicModel& cube = getCube(i);

			setPosition(cube.getTransform(), outside);
			cube.velocity = {};
		}

		for (int i = 0; i < cubeCount(); i++)
		{
			DynamicModel& cube = getCube(i);

			setPosition(cube.getTransform(), getNextSpawn(player_ptr, enemy_ptr, cube.radius));
		}
	}

	void applyVelocities(SphereDynamicModel* player_ptr, SphereDynamicModel* enemy_ptr)
	{
		auto damp = [&](Vec3 v) -> Vec3
		{
			return v * damping_multiplier;

			const float l = v.length();

			if (!l)
				return {};

			return v / l * max(0.f, l - damping_limit);
		};

		for (int i = 0; i < cubeCount(); i++)
		{
			DynamicModel& cube = getCube(i);

			cube.velocity = damp(cube.velocity);

			// TODO: hypercube
		}

		if (player_ptr)
		{
			SphereDynamicModel& player = DEREF(player_ptr);

			player.velocity = damp(player.velocity);
		}

		if (enemy_ptr)
		{
			SphereDynamicModel& enemy = DEREF(enemy_ptr);

			enemy.velocity = damp(enemy.velocity);
		}
	}

	void processCollisions(SphereDynamicModel* player_ptr, SphereDynamicModel* enemy_ptr)
	{
		if (!player_ptr && !enemy_ptr)
			return;

		int count = m_cubes.size() + 1;

		float player_time = 0, enemy_time = 0;
		static vector<float> cube_times, player_collision_times, enemy_collision_times;

		float player_enemy_collision_time;

		cube_times.assign(count, 0);

		if (player_ptr)
			player_collision_times.resize(count);

		if (enemy_ptr)
			enemy_collision_times.resize(count);

		auto respawn_cube = [&](int i, float time)
		{
			DynamicModel& cube = getCube(i);

			setPosition(cube.getTransform(), getNextSpawn(player_ptr, enemy_ptr, cube.radius));
			cube.velocity = {};

			cube_times[i] = time;
		};

		auto advance_time = [&](float time)
		{
			for (int i = 0; i < count; i++)
			{
				DynamicModel& cube = getCube(i);

				setPosition(cube.getTransform(), getPosition(cube.getTransform()) + cube.velocity * (time - cube_times[i]));

				cube_times[i] = time;
			}

			if (player_ptr)
			{
				SphereDynamicModel& player = DEREF(player_ptr);

				setPosition(player.getTransform(), getPosition(player.getTransform()) + player.velocity * (time - player_time));

				player_time = time;
			}

			if (enemy_ptr)
			{
				SphereDynamicModel& enemy = DEREF(enemy_ptr);

				setPosition(enemy.getTransform(), getPosition(enemy.getTransform()) + enemy.velocity * (time - enemy_time));

				enemy_time = time;
			}
		};

		auto calculate_collision_time = [&](int i)
		{
			if (player_ptr)
				player_collision_times[i] = timeOfCollision(DEREF(player_ptr), player_time, getCube(i), cube_times[i]);

			if (enemy_ptr)
				enemy_collision_times[i] = timeOfCollision(DEREF(enemy_ptr), enemy_time, getCube(i), cube_times[i]);
		};

		auto calculate_all_collision_times = [&]()
		{
			if (player_ptr)
			{
				SphereDynamicModel& player = DEREF(player_ptr);

				for (int i = 0; i < count; i++)
					player_collision_times[i] = timeOfCollision(player, player_time, getCube(i), cube_times[i]);
			}

			if (enemy_ptr)
			{
				SphereDynamicModel& enemy = DEREF(enemy_ptr);

				for (int i = 0; i < count; i++)
					enemy_collision_times[i] = timeOfCollision(enemy, enemy_time, getCube(i), cube_times[i]);
			}

			if (player_ptr && enemy_ptr)
				player_enemy_collision_time = timeOfCollision(DEREF(player_ptr), player_time, DEREF(enemy_ptr), enemy_time);
		};

		calculate_all_collision_times();

		enum class CollisionType
		{
			None,
			PlayerCube,
			EnemyCube,
			PlayerEnemy,
		};

		while (true)
		{
			CollisionType closest_collision_type = CollisionType::None;
			float closest_collision_time = numbers::largest;

			int index;

			if (player_ptr)
			{
				vector<float>::iterator closest_player_collision_time = min_element(RANGE(player_collision_times));

				if (*closest_player_collision_time < closest_collision_time)
				{
					closest_collision_type = CollisionType::PlayerCube;
					closest_collision_time = *closest_player_collision_time;

					index = closest_player_collision_time - begin(player_collision_times);
				}
			}

			if (enemy_ptr)
			{
				vector<float>::iterator closest_enemy_collision_time = min_element(RANGE(enemy_collision_times));

				if (*closest_enemy_collision_time < closest_collision_time)
				{
					closest_collision_type = CollisionType::EnemyCube;
					closest_collision_time = *closest_enemy_collision_time;

					index = closest_enemy_collision_time - begin(enemy_collision_times);
				}
			}

			if (player_ptr && enemy_ptr)
			{
				if (player_enemy_collision_time < closest_collision_time)
				{
					closest_collision_type = CollisionType::PlayerEnemy;
					closest_collision_time = player_enemy_collision_time;
				}
			}

			if (closest_collision_time >= 1)
				break;

			ASSERT(closest_collision_type != CollisionType::None);

			advance_time(closest_collision_time);

			switch (closest_collision_type)
			{
			break; case CollisionType::PlayerCube:
			{
				SphereDynamicModel& player = DEREF(player_ptr);
				DynamicModel& cube = getCube(index);;

				respawn_cube(index, closest_collision_time);
				calculate_collision_time(index);

				player.points += 10;
				// TODO: hypercube
			}
			break; case CollisionType::EnemyCube:
			{
				SphereDynamicModel& enemy = DEREF(enemy_ptr);
				DynamicModel& cube = getCube(index);

				respawn_cube(index, closest_collision_time);
				calculate_collision_time(index);

				enemy.points += 10;
				// TODO: hypercube
			}
			break; case CollisionType::PlayerEnemy:
			{
				SphereDynamicModel& player = DEREF(player_ptr);
				SphereDynamicModel& enemy = DEREF(enemy_ptr);

				if (player.points > enemy.points + 40)
				{
					player.points += 40;
					enemy.dead = true;

					enemy_ptr = nullptr;

					break; // no recalculation since movement is not affected
				}

				if (enemy.points > player.points + 40)
				{
					enemy.points += 40;
					player.dead = true;

					player_ptr = nullptr;

					break; // no recalculation since movement is not affected
				}

				resolveCollision(player, enemy, bounce_force);

				player.velocity.y = 0;
				enemy.velocity.y = 0;

				calculate_all_collision_times();
			}
			}
		}

		advance_time(1);
	}

	void checkBounds(SphereDynamicModel* player_ptr, SphereDynamicModel* enemy_ptr)
	{
		if (player_ptr)
		{
			SphereDynamicModel& player = DEREF(player_ptr);

			if (!withinBounds(getPosition(player.getTransform())))
				player.dead = true;
		}

		if (enemy_ptr)
		{
			SphereDynamicModel& enemy = DEREF(enemy_ptr);

			if (!withinBounds(getPosition(enemy.getTransform())))
				enemy.dead = true;
		}
	}

protected:
	DynamicModel m_hypercube;
	vector<DynamicModel*> m_cubes;

public:
	float separation = 0;
	Vec3 bounds_from, bounds_to;
	minstd_rand random_generator;

	float bounce_force = 0;
	float damping_multiplier = 0, damping_limit = 0;
};

template <typename F>
string fixed_float_to_string(F value, streamsize before, streamsize after, char fill = ' ')
{
	ASSERT(before >= 0);
	ASSERT(after >= 0);

	ostringstream str;

	streamsize width = before + (after == 0 ? 0 : after + 1);

	str << fixed << right << setfill(fill) << setprecision(after) << setw(width) << value;

	return str.str();
}

enum class GameState
{
	Playing,
	Paused,
	GameWon,
	GameOver,
};

// Enables debug camera (key 0)
constexpr bool debug_mode = false;

void main() try
{
	// Create a 3D engine (using TLX engine here)
	I3DEngine& engine = DEREF(New3DEngine(kTLX));

	try
	{
		// Fullscreen window crashes if it's not in focus
		//ASSERT(engine.StartFullscreen(1920, 1080));
		engine.StartWindowed();

		// Add media folder
		engine.AddMediaFolder("Media");

		// Load resources
		IMesh& water_mesh = DEREF(engine.LoadMesh("water.x"));
		IMesh& island_mesh = DEREF(engine.LoadMesh("island.x"));
		IMesh& skybox_mesh = DEREF(engine.LoadMesh("sky.x"));
		IMesh& sphere_mesh = DEREF(engine.LoadMesh("spheremesh.x"));
		IMesh& cube_mesh = DEREF(engine.LoadMesh("minicube.x"));

		IFont& main_font = DEREF(engine.LoadFont("Comic Sans MS", 36));

		// Initialize game objects
		StaticModel water{engine, water_mesh, {0, -5, 0}};
		StaticModel island{engine, island_mesh, {0, -5, 0}};
		StaticModel skybox{engine, skybox_mesh, {0, -960, 0}};

		auto* player = new PlayerSphereDynamicModel{engine, sphere_mesh, "regularsphere.jpg", "hypersphere.jpg", {0, 10, 0}, 10};
		{
			DEREF(player).forward_axis = {Key_W, Key_S, 1};
			DEREF(player).rotation_axis = {Key_D, Key_A, 1};
		}

		auto* enemy = new EnemySphereDynamicModel{engine, sphere_mesh, "enemysphere.jpg", "hypersphere.jpg", {0, 10, 0}, 10};
		{
			DEREF(enemy).random_generator.seed(1);
		}

		DynamicModelManager cube_manager{engine, cube_mesh, "minicube.jpg", "hypercube.jpg", 12, 2.5};
		{
			cube_manager.bounds_from = {-100, 2.5, -100};
			cube_manager.bounds_to = {100, 2.5, 100};
			cube_manager.separation = 10;
			cube_manager.damping_multiplier = 0.75;
			cube_manager.bounce_force = 4;

			cube_manager.respawnAll(player, enemy);
		}

		KeyboardControlledCamera camera_1{engine, {0, 200, 0}}; // TODO: a bit higher
		{
			camera_1.getTransform().RotateLocalX(90);

			camera_1.x_axis = {Key_Right, Key_Left, 1.5};
			camera_1.z_axis = {Key_Up, Key_Down, 1.5};
		}

		StaticCamera camera_2{engine, {150, 150, -150}};
		{
			camera_2.getTransform().RotateLocalY(-45);
			camera_2.getTransform().RotateLocalX(45);
		}

		DebugCamera debug_camera{engine, {0, 10, 0}};
		{
			debug_camera.x_axis = {Key_L, Key_J, 1};
			debug_camera.z_axis = {Key_I, Key_K, 1};
			debug_camera.y_axis = {Key_Y, Key_H, 1};

			debug_camera.fly_toggle = {Key_U};

			debug_camera.accelerate_button = {Mouse_RButton};
			debug_camera.acceleration_multiplier = 5;

			debug_camera.mouse_toggle = {Mouse_LButton};
			debug_camera.mouse_move.multiplier = 0.05;
		}

		StaticModel* const static_objects[] = {&water, &island, &skybox};
		StaticCamera* const cameras[] = {&camera_1, &camera_2, &debug_camera};

		// Set initial game state
		StaticCamera* active_camera = &camera_1;

		GameState state = GameState::Playing;

		int player_points = 0, enemy_points = 0;

		// 60 FPS
		constexpr auto target_frame_time = chrono::duration_cast<chrono::steady_clock::duration>(std::chrono::duration<int, std::ratio<1, 60>>(1));

		float delta_time = 0;

		chrono::steady_clock::time_point frame_start_time = chrono::steady_clock::now();

		// Main game loop
		while (engine.IsRunning())
		{
			if (engine.KeyHit(Key_Escape))
				engine.Stop();

			if (engine.KeyHit(Key_P))
				switch (state)
				{
				break; case GameState::Playing:
					state = GameState::Paused;
				break; case GameState::Paused:
					state = GameState::Playing;
				}

			if (state != GameState::Paused)
			{
				if (engine.KeyHit(Key_1))
					active_camera = &camera_1;

				if (engine.KeyHit(Key_2))
					active_camera = &camera_2;

				if (engine.KeyHit(Key_0) && debug_mode)
					active_camera = &debug_camera;
			}

			const bool register_input = (engine.IsActive() && state == GameState::Playing);

			for (StaticModel* const static_object : static_objects)
				DEREF(static_object).processInput(register_input);

			if (player)
				DEREF(player).processInput(register_input);

			if (enemy)
				DEREF(enemy).processInput(register_input);

			for (StaticCamera* const camera : cameras)
				DEREF(camera).processInput(register_input && camera == active_camera);

			if (state != GameState::Paused)
			{
				for (StaticModel* const static_object : static_objects)
					DEREF(static_object).updateBegin();

				if (player)
					DEREF(player).updateBegin();

				if (enemy)
					DEREF(enemy).updateBegin();

				for (StaticCamera* const camera : cameras)
					DEREF(camera).updateBegin();
				
				// TODO: collisions
				cube_manager.applyVelocities(player, enemy);
				cube_manager.processCollisions(player, enemy);
				cube_manager.checkBounds(player, enemy);

				if (player)
				{
					player_points = DEREF(player).points;

					if (DEREF(player).dead)
					{
						delete player;

						player = nullptr;
					}
				}

				if (enemy)
				{
					enemy_points = DEREF(enemy).points;

					if (DEREF(enemy).dead)
					{
						delete enemy;

						enemy = nullptr;
					}
				}
			}

			if (state == GameState::Playing)
			{
				if (!enemy)
					state = GameState::GameWon;
				else if (!player)
					state = GameState::GameOver;
			}

			// Display FPS
			main_font.Draw(fixed_float_to_string(1 / delta_time, 3, 2) + " FPS", 0, 0, kBlack, kLeft, kTop);

			ostringstream player_stats, enemy_stats;

			player_stats << "\nPlayer: " << setw(3) << player_points;
			enemy_stats << "\nEnemy: " << setw(3) << enemy_points;

			const string stats = (enemy_points > player_points ? enemy_stats.str() + player_stats.str() : player_stats.str() + enemy_stats.str());

			// Display points
			main_font.Draw("Points:" + stats, engine.GetWidth() - 5, 0, kBlack, kRight, kTop);

			switch (state)
			{
			break; case GameState::Paused:
				main_font.Draw("Paused", engine.GetWidth() / 2, engine.GetHeight() / 2, kBlack, kCentre, kVCentre);
			break; case GameState::GameWon:
				main_font.Draw("Congrats, You WON", engine.GetWidth() / 2, engine.GetHeight() / 2, kGreen, kCentre, kVCentre);
			break; case GameState::GameOver:
				main_font.Draw("You Lose", engine.GetWidth() / 2, engine.GetHeight() / 2, kRed, kCentre, kVCentre);
			}

			// Render the scene
			DEREF(active_camera).renderScene();

			// Calculate next frame start time
			const chrono::steady_clock::time_point next_frame_start_time = max(chrono::steady_clock::now(), frame_start_time + target_frame_time);

			// Calculate delta time
			delta_time = chrono::duration<float>(next_frame_start_time - frame_start_time).count();

			// Sleep
			this_thread::sleep_until(next_frame_start_time);

			frame_start_time = next_frame_start_time;
		}
	}
	catch (...)
	{
		engine.Delete();

		throw;
	}

	engine.Delete();
}
// Report exceptions:
catch (const exception& ex)
{
	internal::report_exception("std::exception", ex.what());
}
catch (const CTLException& ex)
{
	// since we use c-style strings, we need to make sure returned std::string will outlive them
	const string description = ex.Description();
	const string call_stack = ex.CallStack();
	const string file_path = ex.FileName();

	const internal::CallInfo info{nullptr, file_path.c_str(), ex.LineNum(), nullptr, nullptr, call_stack.c_str()};

	internal::report_exception("tle::CTLException", description.c_str(), info);

	// ex.Display() is not reliable
}
catch (...)
{
	internal::report_exception("Unknown", nullptr);
}
