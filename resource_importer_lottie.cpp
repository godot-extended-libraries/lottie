#include "resource_importer_lottie.h"

#include "core/io/file_access_pack.h"
#include "scene/2d/animated_sprite.h"

#include "thirdparty/rlottie/inc/rlottie.h"
#include "thirdparty/rlottie/inc/rlottiecommon.h"

String ResourceImporterLottie::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterLottie::get_import_options(List<ImportOption> *r_options, int p_preset) const {
}

bool ResourceImporterLottie::get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const {
	return true;
}

String ResourceImporterLottie::get_importer_name() const {
	return "Lottie AnimatedSprite";
}

String ResourceImporterLottie::get_visible_name() const {
	return "LottieAnimatedSprite";
}

void ResourceImporterLottie::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("json");
}

String ResourceImporterLottie::get_save_extension() const {
	return "scn";
}

String ResourceImporterLottie::get_resource_type() const {
	return "PackedScene";
}

int ResourceImporterLottie::get_preset_count() const {
	return 0;
}

Error ResourceImporterLottie::import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	FileAccess *file = FileAccess::create(FileAccess::ACCESS_RESOURCES);
	Error err;
	String data = file->get_file_as_string(p_source_file, &err);
	ERR_FAIL_COND_V(err != Error::OK, FAILED);
	std::unique_ptr<rlottie::Animation> lottie =
	rlottie::Animation::loadFromData(data.utf8().ptrw(), p_source_file.utf8().ptr());
	ERR_FAIL_COND_V(!lottie, FAILED);
	size_t width = 0;
	size_t height = 0;
	lottie->size(width, height);
	ERR_FAIL_COND_V(!width, FAILED);
	ERR_FAIL_COND_V(!height, FAILED);
	AnimatedSprite *root = memnew(AnimatedSprite);
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
		Vector<uint32_t> buffer;
		buffer.resize(width * height);
		rlottie::Surface surface(buffer.ptrw(), width, height, width * 4);
		lottie->renderSync(frame_i, surface);
		PoolByteArray pixels;
		int32_t buffer_byte_size = buffer.size() * sizeof(uint32_t);
		pixels.resize(buffer_byte_size);
		PoolByteArray::Write pixel_write = pixels.write();
		memcpy(pixel_write.ptr(), buffer.ptr(), buffer_byte_size);
		for (int32_t pixel_i = 0; pixel_i < pixels.size(); pixel_i += 4) {
			uint8_t r = pixels[pixel_i + 2];
			uint8_t g = pixels[pixel_i + 1];
			uint8_t b = pixels[pixel_i + 0];
			pixel_write[pixel_i + 2] = b;
			pixel_write[pixel_i + 1] = g;
			pixel_write[pixel_i + 0] = r;
		}
		img->create((int)width, (int)height, false, Image::FORMAT_RGBA8, pixels);
		Ref<ImageTexture> image_tex;
		image_tex.instance();
		image_tex->create_from_image(img);
		frames->add_frame(name, image_tex);
	}
	root->set_sprite_frames(frames);
	Ref<PackedScene> scene;
	scene.instance();
	scene->pack(root);
	String save_path = p_save_path + ".scn";
	r_gen_files->push_back(save_path);
	return ResourceSaver::save(save_path, scene);
}

ResourceImporterLottie::ResourceImporterLottie() {
}

ResourceImporterLottie::~ResourceImporterLottie() {
}
