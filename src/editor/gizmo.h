#pragma once


#include "core/lumix.h"
#include "core/vec3.h"
#include "universe/universe.h"
#include "graphics/model.h"


namespace Lumix
{


class Event;
class IRenderDevice;
struct Matrix;
class Renderer;
class Universe;
class WorldEditor;


class LUMIX_ENGINE_API Gizmo
{
	public:
		enum class Flags : uint32_t
		{
			FIXED_STEP = 1
		};

		enum class TransformOperation : uint32_t
		{
			ROTATE,
			TRANSLATE
		};

		enum class TransformMode : uint32_t
		{
			X,
			Y,
			Z,
			CAMERA_XZ
		};

		enum class PivotMode
		{
			CENTER,
			OBJECT_PIVOT
		};

		enum class CoordSystem
		{
			LOCAL,
			WORLD
		};

	public:
		Gizmo(WorldEditor& editor);
		~Gizmo();

		void create(Renderer& renderer);
		void destroy();
		void updateScale(Component camera);
		void setUniverse(Universe* universe);
		void startTransform(Component camera, int x, int y, TransformMode mode);
		void transform(Component camera, TransformOperation operation, int x, int y, int relx, int rely, int flags);
		void render(Renderer& renderer, IRenderDevice& render_device);
		RayCastModelHit castRay(const Vec3& origin, const Vec3& dir);
		void togglePivotMode();
		void toggleCoordSystem();

	private:
		void getMatrix(Matrix& mtx);
		void getEnityMatrix(Matrix& mtx, int selection_index);
		Vec3 getMousePlaneIntersection(Component camera, int x, int y);
		void rotate(int relx, int rely, int flags);
		float computeRotateAngle(int relx, int rely, int flags);

	private:
		WorldEditor& m_editor;
		Renderer* m_renderer;
		Universe* m_universe;
		TransformMode m_transform_mode;
		Vec3 m_transform_point;
		int m_relx_accum;
		int m_rely_accum;
		class Model* m_model;
		float m_scale;
		PivotMode m_pivot_mode;
		CoordSystem m_coord_system;
};


} // !namespace Lumix