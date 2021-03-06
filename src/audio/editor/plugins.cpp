#include "animation/animation_system.h"
#include "animation/editor/animation_editor.h"
#include "audio_device.h"
#include "audio_scene.h"
#include "audio_system.h"
#include "clip_manager.h"
#include "editor/asset_browser.h"
#include "editor/property_grid.h"
#include "editor/studio_app.h"
#include "editor/utils.h"
#include "editor/world_editor.h"
#include "engine/crc32.h"
#include "engine/engine.h"
#include "engine/plugin_manager.h"
#include "engine/property_register.h"
#include "engine/universe/universe.h"
#include "imgui/imgui.h"
#include "renderer/render_scene.h"


using namespace Lumix;


namespace
{


static const Lumix::ResourceType CLIP_TYPE("clip");


struct AssetBrowserPlugin LUMIX_FINAL : public AssetBrowser::IPlugin
{
	explicit AssetBrowserPlugin(StudioApp& app)
		: m_app(app)
		, m_browser(*app.getAssetBrowser())
		, m_playing_clip(-1)
	{
	}


	Lumix::AudioDevice& getAudioDevice(Lumix::Engine& engine)
	{
		auto* audio = static_cast<Lumix::AudioSystem*>(engine.getPluginManager().getPlugin("audio"));
		return audio->getDevice();
	}


	bool acceptExtension(const char* ext, Lumix::ResourceType type) const override { return false; }


	void stopAudio()
	{
		if (m_playing_clip < 0) return;

		getAudioDevice(m_app.getWorldEditor()->getEngine()).stop(m_playing_clip);
		m_playing_clip = -1;
	}


	const char* getName() const override { return "Audio"; }


	bool onGUI(Lumix::Resource* resource, Lumix::ResourceType type) override
	{
		if (type != CLIP_TYPE) return false;
		auto* clip = static_cast<Lumix::Clip*>(resource);
		ImGui::LabelText("Length", "%f", clip->getLengthSeconds());
		auto& device = getAudioDevice(m_app.getWorldEditor()->getEngine());

		if (m_playing_clip >= 0)
		{
			if (ImGui::Button("Stop"))
			{
				stopAudio();
				return true;
			}
			float time = device.getCurrentTime(m_playing_clip);
			if (ImGui::SliderFloat("Time", &time, 0, clip->getLengthSeconds(), "%.2fs"))
			{
				device.setCurrentTime(m_playing_clip, time);
			}
		}

		if (m_playing_clip < 0 && ImGui::Button("Play"))
		{
			stopAudio();

			auto handle =
				device.createBuffer(clip->getData(), clip->getSize(), clip->getChannels(), clip->getSampleRate(), 0);
			device.play(handle, false);
			m_playing_clip = handle;
		}
		return true;
	}


	void onResourceUnloaded(Lumix::Resource*) override { stopAudio(); }


	bool hasResourceManager(Lumix::ResourceType type) const override { return type == CLIP_TYPE; }


	Lumix::ResourceType getResourceType(const char* ext) override
	{
		if (Lumix::equalStrings(ext, "ogg")) return CLIP_TYPE;
		return INVALID_RESOURCE_TYPE;
	}

	int m_playing_clip;
	StudioApp& m_app;
	AssetBrowser& m_browser;
};


struct StudioAppPlugin LUMIX_FINAL : public StudioApp::IPlugin
{
	explicit StudioAppPlugin(StudioApp& app)
		: m_app(app)
	{
		m_filter[0] = 0;
		m_is_opened = false;
		Action* action = LUMIX_NEW(app.getWorldEditor()->getAllocator(), Action)("Clip manager", "clip_manager");
		action->func.bind<StudioAppPlugin, &StudioAppPlugin::onAction>(this);
		action->is_selected.bind<StudioAppPlugin, &StudioAppPlugin::isOpened>(this);
		app.addWindowAction(action);
	}


	void pluginAdded(IPlugin& plugin) override
	{
		if (!equalStrings(plugin.getName(), "animation_editor")) return;

		auto& anim_editor = (AnimEditor::IAnimationEditor&)plugin;
		auto& event_type = anim_editor.createEventType("sound");
		event_type.size = sizeof(SoundAnimationEvent);
		event_type.label = "Sound";
		event_type.editor.bind<StudioAppPlugin, &StudioAppPlugin::onSoundEventGUI>(this);
	}


	void onSoundEventGUI(u8* data, AnimEditor::Component& component)
	{
		auto* ev = (SoundAnimationEvent*)data;
		AudioScene* scene = (AudioScene*)m_app.getWorldEditor()->getUniverse()->getScene(crc32("audio"));
		auto getter = [](void* data, int idx, const char** out) -> bool {
			auto* scene = (AudioScene*)data;
			*out = scene->getClipName(idx);
			return true;
		};
		int current = 0;
		AudioScene::ClipInfo* clip = scene->getClipInfo(ev->clip);
		current = clip ? scene->getClipInfoIndex(clip) : -1;

		if (ImGui::Combo("Clip", &current, getter, scene, scene->getClipCount()))
		{
			ev->clip = scene->getClipInfo(current)->name_hash;
		}
	}


	const char* getName() const override { return "audio"; }


	bool isOpened() const { return m_is_opened; }
	void onAction() { m_is_opened = !m_is_opened; }


	void onWindowGUI() override
	{
		if (ImGui::BeginDock("Clip Manager", &m_is_opened))
		{
			ImGui::InputText("Filter", m_filter, Lumix::lengthOf(m_filter));

			auto universe = m_app.getWorldEditor()->getUniverse();
			auto* audio_scene = static_cast<Lumix::AudioScene*>(universe->getScene(Lumix::crc32("audio")));
			int clip_count = audio_scene->getClipCount();
			for (int clip_id = 0; clip_id < clip_count; ++clip_id)
			{
				auto* clip_info = audio_scene->getClipInfo(clip_id);

				if (m_filter[0] != 0 && Lumix::stristr(clip_info->name, m_filter) == 0)
				{
					continue;
				}

				if (ImGui::TreeNode((const void*)(uintptr)clip_id, "%s", clip_info->name))
				{
					char buf[30];
					Lumix::copyString(buf, Lumix::lengthOf(buf), clip_info->name);
					if (ImGui::InputText("Name", buf, sizeof(buf)))
					{
						Lumix::copyString(clip_info->name, buf);
						clip_info->name_hash = Lumix::crc32(buf);
					}
					auto* clip = audio_scene->getClipInfo(clip_id)->clip;
					char path[Lumix::MAX_PATH_LENGTH];
					Lumix::copyString(path, clip ? clip->getPath().c_str() : "");
					if (m_app.getAssetBrowser()->resourceInput("Clip", "", path, Lumix::lengthOf(path), CLIP_TYPE))
					{
						audio_scene->setClip(clip_id, Lumix::Path(path));
					}
					bool looped = audio_scene->getClipInfo(clip_id)->looped;
					if (ImGui::Checkbox("Looped", &looped))
					{
						clip_info->looped = looped;
					}
					if (ImGui::Button("Remove"))
					{
						audio_scene->removeClip(clip_info);
						--clip_count;
					}
					ImGui::TreePop();
				}
			}

			if (ImGui::Button("Add"))
			{
				audio_scene->addClip("test", Lumix::Path("test.ogg"));
			}
		}
		ImGui::EndDock();
	}


	StudioApp& m_app;
	char m_filter[256];
	bool m_is_opened;
};


struct EditorPlugin LUMIX_FINAL : public WorldEditor::Plugin
{
	explicit EditorPlugin(WorldEditor& editor)
		: m_editor(editor)
	{
	}

	bool showGizmo(ComponentUID cmp) override
	{
		static const ComponentType ECHO_ZONE_TYPE = PropertyRegister::getComponentType("echo_zone");

		if (cmp.type == ECHO_ZONE_TYPE)
		{
			auto* audio_scene = static_cast<AudioScene*>(cmp.scene);
			float radius = audio_scene->getEchoZoneRadius(cmp.handle);
			Universe& universe = audio_scene->getUniverse();
			Vec3 pos = universe.getPosition(cmp.entity);

			auto* scene = static_cast<RenderScene*>(universe.getScene(crc32("renderer")));
			if (!scene) return true;
			scene->addDebugSphere(pos, radius, 0xff0000ff, 0);
			return true;
		}

		return false;
	}

	WorldEditor& m_editor;
};

} // anonymous namespace


LUMIX_STUDIO_ENTRY(audio)
{
	app.registerComponent("ambient_sound", "Audio/Ambient sound");
	app.registerComponent("audio_listener", "Audio/Listener");
	app.registerComponent("echo_zone", "Audio/Echo zone");

	auto& editor = *app.getWorldEditor();
	auto& allocator = editor.getAllocator();

	auto* asset_browser_plugin = LUMIX_NEW(allocator, AssetBrowserPlugin)(app);
	app.getAssetBrowser()->addPlugin(*asset_browser_plugin);

	auto* app_plugin = LUMIX_NEW(allocator, StudioAppPlugin)(app);
	app.addPlugin(*app_plugin);

	auto* plugin = LUMIX_NEW(allocator, EditorPlugin)(editor);
	editor.addPlugin(*plugin);
}
