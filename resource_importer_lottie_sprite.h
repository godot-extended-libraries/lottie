/*************************************************************************/
/*  resource_importer_lottie_sprite.h   	                                     */
/*************************************************************************/

#ifndef RESOURCE_IMPORTER_LOTTIE_SPRITE
#define RESOURCE_IMPORTER_LOTTIE_SPRITE

#include "core/io/resource_saver.h"
#include "scene/2d/animated_sprite.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/primitive_meshes.h"

#include "thirdparty/rlottie/inc/rlottie.h"

class ResourceImporterLottieSprite : public ResourceImporter {
	GDCLASS(ResourceImporterLottieSprite, ResourceImporter);

public:
	virtual String get_importer_name() const;
	virtual String get_visible_name() const;
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual String get_save_extension() const;
	virtual String get_resource_type() const;

	virtual int get_preset_count() const;
	virtual String get_preset_name(int p_idx) const;

	virtual void get_import_options(List<ImportOption> *r_options,
			int p_preset = 0) const;
	virtual bool
	get_option_visibility(const String &p_option,
			const Map<StringName, Variant> &p_options) const;
	virtual Error import(const String &p_source_file, const String &p_save_path,
			const Map<StringName, Variant> &p_options,
			List<String> *r_platform_variants,
			List<String> *r_gen_files = NULL,
			Variant *r_metadata = NULL);

	ResourceImporterLottieSprite();
	~ResourceImporterLottieSprite();
};

String ResourceImporterLottieSprite::get_importer_name() const {
	return "lottie_sprite_animation";
}

String ResourceImporterLottieSprite::get_visible_name() const {
	return "Lottie Sprite Animation";
}

void ResourceImporterLottieSprite::get_recognized_extensions(
		List<String> *p_extensions) const {
	p_extensions->push_back("json");
}

String ResourceImporterLottieSprite::get_save_extension() const {
	return "scn";
}

String ResourceImporterLottieSprite::get_resource_type() const {
	return "PackedScene";
}

bool ResourceImporterLottieSprite::get_option_visibility(
		const String &p_option, const Map<StringName, Variant> &p_options) const {
	return true;
}

int ResourceImporterLottieSprite::get_preset_count() const {
	return 0;
}
String ResourceImporterLottieSprite::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterLottieSprite::get_import_options(List<ImportOption> *r_options,
		int p_preset) const {}

Error ResourceImporterLottieSprite::import(const String &p_source_file,
		const String &p_save_path,
		const Map<StringName, Variant> &p_options,
		List<String> *r_platform_variants,
		List<String> *r_gen_files,
		Variant *r_metadata) {
	String path = ProjectSettings::get_singleton()->globalize_path(p_source_file);
	std::unique_ptr<rlottie::Animation> lottie =
			rlottie::Animation::loadFromFile(path.utf8().ptr());
	ERR_FAIL_COND_V(!lottie, FAILED);
	size_t w = 0;
	size_t h = 0;
	lottie->size(w, h);
	ERR_FAIL_COND_V(w == 0, FAILED);
	ERR_FAIL_COND_V(h == 0, FAILED);

	Node2D *root = memnew(Node2D);
	AnimatedSprite *sprite = memnew(AnimatedSprite);
	root->add_child(sprite);
	sprite->set_owner(root);
	Ref<QuadMesh> mesh;
	mesh.instance();
	Ref<Image> img;
	img.instance();
	Ref<SpriteFrames> frames;
	frames.instance();
	List<StringName> animations;
	frames->get_animation_list(&animations);
	String name = animations[0];
	frames->set_animation_speed(name, lottie->frameRate());
	for (int32_t frame_i = 0; frame_i < lottie->totalFrame(); frame_i++) {
		PoolVector<uint8_t> pixels;
		pixels.resize(w * h);
		PoolVector<uint8_t>::Write pixels_write = pixels.write();
		rlottie::Surface surface((uint32_t *)pixels_write.ptr(), w, h, w * 4);
		lottie->renderSync(frame_i, surface);
		for (int32_t pixel_i = 0; pixel_i < pixels.size(); pixel_i += 4) {
			uint8_t r = pixels[pixel_i + 2];
			uint8_t g = pixels[pixel_i + 1];
			uint8_t b = pixels[pixel_i + 0];
			pixels_write[pixel_i + 2] = b;
			pixels_write[pixel_i + 1] = g;
			pixels_write[pixel_i + 0] = r;
		}
		img->create(w, h, false, Image::FORMAT_RGBA8, pixels);
		img->generate_mipmaps();
		Ref<ImageTexture> image_tex;
		image_tex.instance();
		image_tex->create_from_image(img);
		frames->add_frame(name, image_tex);
	}
	sprite->set_sprite_frames(frames);
	Ref<PackedScene> scene;
	scene.instance();
	scene->pack(root);
	String save_path = p_save_path + ".scn";
	r_gen_files->push_back(save_path);
	return ResourceSaver::save(save_path, scene);
}

ResourceImporterLottieSprite::ResourceImporterLottieSprite() {}

ResourceImporterLottieSprite::~ResourceImporterLottieSprite() {}

#endif // RESOURCE_IMPORTER_LOTTIE
