#include "resource_importer_lottie.h"
#include "core/io/file_access_pack.h"
#include "thirdparty/rlottie/inc/rlottie.h"
#include "thirdparty/rlottie/inc/rlottiecommon.h"
#include "thirdparty/tove/vector_graphics_path.h"

String ResourceImporterLottie::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterLottie::get_import_options(List<ImportOption> *r_options, int p_preset) const {
}

bool ResourceImporterLottie::get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const {
	return true;
}

String ResourceImporterLottie::get_importer_name() const {
	return "lottie";
}

String ResourceImporterLottie::get_visible_name() const {
	return "LottieVGPath";
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
	VGPath *root = memnew(VGPath);
	Ref<VGSpriteRenderer> renderer;
	renderer.instance();
	root->set_renderer(renderer);
	renderer->set_quality(0.7f);
	VGPathAnimation *frame_root = memnew(VGPathAnimation());
	root->add_child(frame_root);
	frame_root->set_owner(root);
	frame_root->set_data(data);
	String frame_root_path = String(root->get_path_to(frame_root)) + ":frame";
	AnimationPlayer *ap = memnew(AnimationPlayer);
	Ref<Animation> animation;
	animation.instance();
	std::unique_ptr<rlottie::Animation> lottie =
			rlottie::Animation::loadFromData(data.utf8().ptrw(), p_source_file.utf8().ptr());
	ERR_FAIL_COND_V(!lottie, FAILED);
	float hertz = 1.0f / lottie->frameRate();
	int32_t track = animation->get_track_count();
	animation->add_track(Animation::TYPE_VALUE);
	animation->set_length((lottie->totalFrame() - 1) * hertz);
	animation->track_set_path(track, frame_root_path);
	animation->track_insert_key(track, 0, 0);
	float frame = lottie->totalFrame() - 1;
	animation->track_insert_key(track, frame * hertz, frame);
	animation->set_loop(true);
	ap->add_animation("Default", animation);
	root->add_child(ap);
	ap->set_owner(root);
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
