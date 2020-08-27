/*************************************************************************/
/*  resource_importer_lottie.cpp                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "resource_importer_lottie.h"

#include "core/bind/core_bind.h"
#include "core/io/file_access_pack.h"
#include "scene/2d/animated_sprite.h"
#include "scene/2d/sprite.h"
#include "scene/3d/sprite_3d.h"

#include "thirdparty/rlottie/inc/rlottie.h"
#include "thirdparty/rlottie/inc/rlottiecommon.h"

String ResourceImporterLottie::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterLottie::get_import_options(List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "3d"), false));
	Dictionary d = Engine::get_singleton()->get_version_info();
	if (!(d["major"] == Variant(3) && d["minor"] == Variant(1))) {
		r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "compress/video_ram"), true));
		r_options->push_back(ImportOption(PropertyInfo(Variant::REAL, "compress/lossy_quality", PROPERTY_HINT_RANGE, "0,1,0.01"), 0.7));
	}
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "start_frame", PROPERTY_HINT_RANGE, "0,65536,1,or_greater"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::VECTOR2, "scale"), Vector2(1.0f, 1.0f)));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "animation/import"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "animation/begin_playing"), true));
}

bool ResourceImporterLottie::get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const {
	return true;
}

String ResourceImporterLottie::get_importer_name() const {
	return "lottie_sprite";
}

String ResourceImporterLottie::get_visible_name() const {
	return "Lottie Sprite";
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
	String data;
	//Backport code
	//String data = file->get_file_as_string(p_source_file, &err);
	Vector<uint8_t> array = file->get_file_as_array(p_source_file);
	data.parse_utf8((const char *)array.ptr(), array.size());
	//End backport code
	std::unique_ptr<rlottie::Animation> lottie =
			rlottie::Animation::loadFromData(data.utf8().ptrw(), p_source_file.utf8().ptr());
	ERR_FAIL_COND_V(!lottie, FAILED);
	size_t width = 0;
	size_t height = 0;
	lottie->size(width, height);
	ERR_FAIL_COND_V(!width, FAILED);
	ERR_FAIL_COND_V(!height, FAILED);
	Vector2 scale = p_options["scale"];
	width *= scale.width;
	height *= scale.height;
	ERR_FAIL_COND_V(!width, FAILED);
	ERR_FAIL_COND_V(!height, FAILED);
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
		Ref<Image> img;
		img.instance();
		img->create((int)width, (int)height, false, Image::FORMAT_RGBA8, pixels);
		Ref<ImageTexture> image_tex;
		image_tex.instance();
		Dictionary d = Engine::get_singleton()->get_version_info();
		if (!(d["major"] == Variant(3) && d["minor"] == Variant(1))) {
			if (p_options["compress/video_ram"]) {
				if (ProjectSettings::get_singleton()->get("rendering/vram_compression/import_etc2")) {
					img->compress(Image::COMPRESS_ETC2, Image::COMPRESS_SOURCE_GENERIC, p_options["compress/lossy_quality"]);
				} else if (ProjectSettings::get_singleton()->get("rendering/vram_compression/import_etc")) {
					img->compress(Image::COMPRESS_ETC, Image::COMPRESS_SOURCE_GENERIC, p_options["compress/lossy_quality"]);
				} else if (ProjectSettings::get_singleton()->get("rendering/vram_compression/import_pvrtc")) {
					img->compress(Image::COMPRESS_PVRTC2, Image::COMPRESS_SOURCE_GENERIC, p_options["compress/lossy_quality"]);
				} else {
					img->compress(Image::COMPRESS_S3TC, Image::COMPRESS_SOURCE_GENERIC, p_options["compress/lossy_quality"]);
				}
			}
		}
		image_tex->create_from_image(img);
		frames->add_frame(name, image_tex);
	}
	Node *root = nullptr;
	if (p_options["3d"] && !p_options["animation/import"]) {
		root = memnew(Sprite3D);
		Sprite3D *sprite = cast_to<Sprite3D>(root);
		int32_t frame = p_options["start_frame"];
		Ref<Texture> tex = frames->get_frame("default", frame);
		ERR_FAIL_COND_V(tex.is_null(), FAILED);
		sprite->set_texture(tex);
		sprite->set_draw_flag(SpriteBase3D::FLAG_SHADED, true);
	} else if (!p_options["3d"] && !p_options["animation/import"]) {
		root = memnew(Sprite);
		Sprite *sprite = cast_to<Sprite>(root);
		int32_t frame = p_options["start_frame"];
		Ref<Texture> tex = frames->get_frame("default", frame);
		ERR_FAIL_COND_V(tex.is_null(), FAILED);
		sprite->set_texture(tex);
	} else if (p_options["3d"] && p_options["animation/import"]) {
		root = memnew(AnimatedSprite3D);
		AnimatedSprite3D *animate_sprite = cast_to<AnimatedSprite3D>(root);
		if (p_options["animation/begin_playing"]) {
			animate_sprite->call("_set_playing", true);
		}
		animate_sprite->set_draw_flag(SpriteBase3D::FLAG_SHADED, true);
		animate_sprite->set_frame(p_options["start_frame"]);
		animate_sprite->set_sprite_frames(frames);
	} else {
		root = memnew(AnimatedSprite);
		AnimatedSprite *animate_sprite = cast_to<AnimatedSprite>(root);
		if (p_options["animation/begin_playing"]) {
			animate_sprite->call("_set_playing", true);
		}
		animate_sprite->set_frame(p_options["start_frame"]);
		animate_sprite->set_sprite_frames(frames);
	}
	Ref<PackedScene> scene;
	scene.instance();
	scene->pack(root);
	String save_path = p_save_path + ".scn";
	r_gen_files->push_back(save_path);
	return ResourceSaver::save(save_path, scene);
}