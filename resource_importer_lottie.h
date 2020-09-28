/*************************************************************************/
/*  resource_importer_lottie.h	                                         */
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
#ifndef RESOURCE_IMPORTER_LOTTIE
#define RESOURCE_IMPORTER_LOTTIE

#include "core/bind/core_bind.h"
#include "core/io/file_access_pack.h"
#include "core/io/resource_importer.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "scene/2d/animated_sprite.h"
#include "scene/2d/sprite.h"
#include "scene/3d/mesh_instance.h"
#include "scene/3d/spatial.h"
#include "scene/3d/sprite_3d.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/primitive_meshes.h"
#include "scene/resources/video_stream.h"

#include "thirdparty/rlottie/inc/rlottie.h"
#include "thirdparty/rlottie/inc/rlottiecommon.h"
class VideoStreamPlaybackLottie;
class VideoStreamLottie : public VideoStream {

	GDCLASS(VideoStreamLottie, VideoStream);

	String data;
	Vector2 scale = Vector2(1.0f, 1.0f);

protected:
	static void _bind_methods();

public:
	VideoStreamLottie();
	virtual Ref<VideoStreamPlayback> instance_playback();
	virtual void set_data(const String &p_file);
	String get_data();
	void set_scale(Vector2 p_scale);
	Vector2 get_scale() const;
	virtual void set_audio_track(int p_track);
};

class VideoStreamPlaybackLottie : public VideoStreamPlayback {

	GDCLASS(VideoStreamPlaybackLottie, VideoStreamPlayback);

	String data;

	int video_frames_pos, video_frames_capacity = 0;

	Vector<Vector<uint32_t> > video_frames;
	int num_decoded_samples, samples_offse = 0;
	AudioMixCallback mix_callback;
	void *mix_udata = nullptr;

	bool playing, paused = false;
	bool loop = true;
	double delay_compensation = 0.0;
	double time, video_frame_delay, video_pos = 0.0;

	PoolVector<uint8_t> frame_data;
	Ref<ImageTexture> texture = memnew(ImageTexture);

	std::unique_ptr<rlottie::Animation> lottie = nullptr;

public:
	VideoStreamPlaybackLottie() {}
	~VideoStreamPlaybackLottie() {
		for (int i = 0; i < video_frames.size(); i++) {
			video_frames.write[i].clear();
		}
		video_frames.clear();
	}

	bool open_data(const String &p_file) {
		if (p_file.empty()) {
			return false;
		}
		data = p_file;
		lottie = rlottie::Animation::loadFromData(p_file.utf8().ptrw(), p_file.md5_text().utf8().ptrw());
		size_t width = 0;
		size_t height = 0;
		lottie->size(width, height);
		texture->create(width, height, Image::FORMAT_RGBA8, Texture::FLAG_FILTER | Texture::FLAG_VIDEO_SURFACE);
		return true;
	}

	virtual void stop() {
		if (playing) {
			open_data(data); //Should not fail here...
			video_frames_capacity = video_frames_pos = 0;
			num_decoded_samples = 0;
			video_frame_delay = video_pos = 0.0;
		}
		time = 0.0;
		playing = false;
	}
	virtual void play() {
		stop();
		delay_compensation = ProjectSettings::get_singleton()->get("audio/video_delay_compensation_ms");
		delay_compensation /= 1000.0;
		playing = true;
	}

	virtual bool is_playing() const {
		return playing;
	}
	virtual void set_paused(bool p_paused) {
		paused = p_paused;
	}
	virtual bool is_paused() const {
		return paused;
	}
	virtual void set_loop(bool p_enable) {
		loop = p_enable;
	}
	virtual bool has_loop() const {
		return loop;
	}
	virtual float get_length() const {
		return lottie->totalFrame() / lottie->frameRate();
	}

	virtual float get_playback_position() const {
		return video_pos;
	}
	virtual void seek(float p_time) {
		time = p_time;
	}

	virtual void set_audio_track(int p_idx) {}

	virtual Ref<Texture> get_texture() const {
		return texture;
	}
	bool has_enough_video_frames() const {
		if (video_frames_pos > 0) {
			// FIXME: AudioServer output latency was fixed in af9bb0e, previously it used to
			// systematically return 0. Now that it gives a proper latency, it broke this
			// code where the delay compensation likely never really worked.
			//const double audio_delay = AudioServer::get_singleton()->get_output_latency();
			const double video_time = (video_frames_pos - 1) / lottie->frameRate();
			return video_time >= time + /* audio_delay + */ delay_compensation;
		}
		return false;
	}
	bool should_process() {
		// FIXME: AudioServer output latency was fixed in af9bb0e, previously it used to
		// systematically return 0. Now that it gives a proper latency, it broke this
		// code where the delay compensation likely never really worked.
		//const double audio_delay = AudioServer::get_singleton()->get_output_latency();
		return get_playback_position() >= time + /* audio_delay + */ delay_compensation;
	}
	virtual void update(float p_delta) {
		if ((!playing || paused))
			return;

		time += p_delta;

		if (time < video_pos) {
			return;
		}

		bool video_frame_done = false;
		size_t width = 0;
		size_t height = 0;
		lottie->size(width, height);

		while (!has_enough_video_frames() ||
				video_frames_pos == 0) {
			Vector<uint32_t> video_frame;
			if (video_frames_pos >= video_frames_capacity) {
				video_frames.resize(++video_frames_capacity);
				Vector<uint32_t> &video_frame = video_frames.write[video_frames_capacity - 1];
				video_frame.resize(width * height);
			}
			video_frame = video_frames[video_frames_pos];

			if (video_frames_pos >= lottie->totalFrame()) {
				break; //Can't demux, EOS?
			}
			if (video_frames.size() && video_frames_pos < lottie->totalFrame()) {
				++video_frames_pos;
			}
		};

		while (video_frames_pos > 0 && !video_frame_done) {
			Vector<uint32_t> video_frame = video_frames[0];
			if (should_process()) {
				rlottie::Surface surface(video_frame.ptrw(), width, height, width * sizeof(uint32_t));
				lottie->renderSync(video_frames_pos, surface);
				PoolVector<uint8_t> frame_data;
				int32_t buffer_byte_size = video_frame.size() * sizeof(uint32_t);
				frame_data.resize(buffer_byte_size);
				PoolVector<uint8_t>::Write w = frame_data.write();
				memcpy(w.ptr(), video_frame.ptr(), buffer_byte_size);
				uint8_t *ptr_w = w.ptr();
				for (int32_t pixel_i = 0; pixel_i < frame_data.size(); pixel_i += 4) {
					SWAP(ptr_w[pixel_i + 2], ptr_w[pixel_i + 0]);
				}
				Ref<Image> img = memnew(Image(width, height, 0, Image::FORMAT_RGBA8, frame_data));
				texture->set_data(img); //Zero copy send to visual server
				video_frame_done = true;
			}
			video_pos = video_frames_pos / lottie->frameRate();
			memmove(video_frames.ptrw(), video_frames.ptr() + 1, (--video_frames_pos) * sizeof(void *));
			video_frames.write[video_frames_pos] = video_frame;
		}

		if (video_frames_pos == 0 && video_frames_pos >= lottie->totalFrame()) {
			stop();
		}
	}

	virtual void set_mix_callback(AudioMixCallback p_callback, void *p_userdata) {
		mix_callback = p_callback;
		mix_udata = p_userdata;
	}
	virtual int get_channels() const {
		return 0;
	}
	virtual int get_mix_rate() const {
		return 0;
	}
};

class ResourceImporterLottie : public ResourceImporter {
	GDCLASS(ResourceImporterLottie, ResourceImporter);

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

	ResourceImporterLottie() {}
	~ResourceImporterLottie() {}
};

// class ResourceFormatLoaderLottie : public ResourceFormatLoader {
// public:
// 	virtual RES load(const String &p_path, const String &p_original_path = "", Error *r_error = NULL) {

// 		FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
// 		if (!f) {
// 			if (r_error) {
// 				*r_error = ERR_CANT_OPEN;
// 			}
// 			return RES();
// 		}

// 		VideoStreamLottie *stream = memnew(VideoStreamLottie);
// 		stream->set_file(p_path);

// 		Ref<VideoStreamLottie> lottie_stream = Ref<VideoStreamLottie>(stream);

// 		if (r_error) {
// 			*r_error = OK;
// 		}

// 		f->close();
// 		memdelete(f);
// 		return lottie_stream;
// 	}
// 	virtual void get_recognized_extensions(List<String> *p_extensions) const {
// 		p_extensions->push_back("json");
// 	}
// 	virtual bool handles_type(const String &p_type) const {
// 		return ClassDB::is_parent_class(p_type, "VideoStream");
// 	}
// 	virtual String get_resource_type(const String &p_path) const {
// 		String el = p_path.get_extension().to_lower();
// 		if (el == "json")
// 			return "VideoStreamLottie";
// 		return "";
// 	}
// };

#endif // RESOURCE_IMPORTER_LOTTIE
