#pragma once

#include <Windows.h>
#include "core/resource_manager.h"
#include "core/resource_manager_base.h"
#include "graphics/irender_device.h"


class WGLRenderDevice : public Lux::IRenderDevice
{
public:
	WGLRenderDevice(Lux::Engine& engine, const char* pipeline_path)
	{
		Lux::Pipeline* pipeline_object = static_cast<Lux::Pipeline*>(engine.getResourceManager().get(Lux::ResourceManager::PIPELINE)->load(pipeline_path));
		ASSERT(pipeline_object);
		if(pipeline_object)
		{
			m_pipeline = Lux::PipelineInstance::create(*pipeline_object);
			m_pipeline->setRenderer(engine.getRenderer());
		}

	}

	virtual void beginFrame() override
	{
		BOOL b = wglMakeCurrent(m_hdc, m_opengl_context);
		ASSERT(b);
	}

	virtual void endFrame() override
	{
		BOOL b = wglSwapLayerBuffers(m_hdc, WGL_SWAP_MAIN_PLANE);
		ASSERT(b);
	}

	virtual Lux::PipelineInstance& getPipeline()
	{
		return *m_pipeline;
	}

	Lux::PipelineInstance* m_pipeline;
	HDC m_hdc;
	HGLRC m_opengl_context;
};