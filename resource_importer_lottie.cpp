#include "resource_importer_lottie.h"



String ResourceImporterLottie::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterLottie::get_import_options(List<ImportOption> *r_options, int p_preset) const {
}

bool ResourceImporterLottie::get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const {
	return true;
}

void ResourceImporterLottie::_visit_render_node(const LOTLayerNode *layer, Node *p_owner, VGPath *p_current_node) {
	for (uint32_t i = 0; i < layer->mNodeList.size; i++) {
		tove::PathRef path_ref = tove::tove_make_shared<tove::Path>();
		LOTNode *node = layer->mNodeList.ptr[i];
		if (!node) {
			continue;
		}

		//Skip Invisible Stroke?
		if (node->mStroke.enable && node->mStroke.width == 0) {
			continue;
		}

		tove::SubpathRef subpath_ref = std::make_shared<tove::Subpath>();

		const float *data = node->mPath.ptPtr;
		if (!data) {
			continue;
		}
		for (size_t i = 0; i < node->mPath.elmCount; i++) {
			String path_print;
			switch (node->mPath.elmPtr[i]) {
				case 0: {
					path_print = "{MoveTo : (" + rtos(data[0]) + " " + rtos(data[1]) + ")}";
					subpath_ref = std::make_shared<tove::Subpath>();
					subpath_ref->moveTo(data[0], data[1]);
					data += 2;
					break;
				}
				case 1: {
					path_print = "{LineTo : (" + rtos(data[0]) + " " + rtos(data[1]) + ")}";
					subpath_ref->lineTo(data[0], data[1]);
					data += 2;
					break;
				}
				case 2: {
					path_print = "{CubicTo : c1(" + rtos(data[0]) + " " + rtos(data[1]) + ") c2(" + rtos(data[2]) + " " + rtos(data[3]) + ") e(" + rtos(data[4]) + " " + rtos(data[5]) + ")}";
					subpath_ref->curveTo(data[0], data[1], data[2], data[3], data[4], data[5]);
					data += 6;
					break;
				}
				case 3: {
					path_print = "{close}";
					path_ref->addSubpath(subpath_ref);
					break;
				}
				default:
					break;
			}
			print_verbose(path_print);
		}
		VGPath *path = memnew(VGPath(path_ref));
		String name = node->keypath;
		if (!name.empty()) {
			path->set_name(name);
		}
		//1: Stroke
		if (node->mStroke.enable) {
			// 	Stroke Width
			path->set_line_width(node->mStroke.width);
			float r = node->mColor.r;
			r /= 255.0f;
			float g = node->mColor.g;
			g /= 255.0f;
			float b = node->mColor.b;
			b /= 255.0f;
			float a = node->mColor.a;
			a /= 255.0f;
			Ref<VGColor> vg_color;
			vg_color.instance();
			vg_color->set_color(Color(r, g, b, a));
			path->set_line_color(vg_color);
			// 	Stroke Cap
			// 	Efl_Gfx_Cap cap;
			switch (node->mStroke.cap) {
				case CapFlat:
					print_verbose("{CapFlat}");
					break;
				case CapSquare:
					print_verbose("{CapSquare}");
					break;
				case CapRound:
					print_verbose("{CapRound}");
					break;
				default:
					print_verbose("{CapFlat}");
					break;
			}
			//Stroke Join
			switch (node->mStroke.join) {
				case LOTJoinStyle::JoinMiter:
					print_verbose("{JoinMiter}");
					path_ref->setLineJoin(TOVE_LINEJOIN_MITER);
					break;
				case LOTJoinStyle::JoinBevel:
					print_verbose("{JoinBevel}");
					path_ref->setLineJoin(TOVE_LINEJOIN_BEVEL);
					break;
				case LOTJoinStyle::JoinRound:
					print_verbose("{JoinRound}");
					path_ref->setLineJoin(TOVE_LINEJOIN_ROUND);
					break;
				default:
					print_verbose("{JoinMiter}");
					path_ref->setLineJoin(TOVE_LINEJOIN_MITER);
					break;
			}
			//Stroke Dash
			if (node->mStroke.dashArraySize > 0) {
				for (int i = 0; i <= node->mStroke.dashArraySize; i += 2) {
					print_verbose("{(length, gap) : (" + rtos(node->mStroke.dashArray[i]) + " " + rtos(node->mStroke.dashArray[i + 1]) + ")}");
				}
				path_ref->setLineDash(node->mStroke.dashArray, node->mStroke.dashArraySize);
			}
		}
		//Fill Method
		switch (node->mBrushType) {
			case BrushSolid: {
				float r = node->mColor.r;
				r /= 255.0f;
				float g = node->mColor.g;
				g /= 255.0f;
				float b = node->mColor.b;
				b /= 255.0f;
				float a = node->mColor.a;
				a /= 255.0f;
				Ref<VGColor> vg_color;
				vg_color.instance();
				vg_color->set_color(Color(r, g, b, a));
				path->set_fill_color(vg_color);
				path->set_line_color(vg_color);
				print_verbose("{BrushSolid}");
			} break;
			case BrushGradient: {
				_read_gradient(node, path);

			} break;
			default:
				break;
		}

		//Fill Rule
		if (node->mFillRule == LOTFillRule::FillEvenOdd) {
			path_ref->setFillRule(TOVE_FILLRULE_EVEN_ODD);
			print_verbose("{FillEvenOdd}");
		} else if (node->mFillRule == LOTFillRule::FillWinding) {
			path_ref->setFillRule(TOVE_FILLRULE_NON_ZERO);
			print_verbose("{FillWinding}");
		}
		p_current_node->add_child(path);
		path->set_owner(p_owner);
	}
}

void ResourceImporterLottie::_read_gradient(LOTNode *node, VGPath *path) {
	if (!node->mGradient.stopPtr) {
	
	}
	Ref<Gradient> color_ramp;
	color_ramp.instance();
	Vector<Gradient::Point> points;
	for (int32_t stop_i = 0; stop_i < node->mGradient.stopCount; stop_i++) {
		LOTGradientStop stop = node->mGradient.stopPtr[stop_i];
		float r = stop.r;
		r /= 255.0f;
		float g = stop.g;
		g /= 255.0f;
		float b = stop.b;
		b /= 255.0f;
		float a = stop.a;
		a /= 255.0f;
		Gradient::Point p;
		p.offset = stop.pos;
		p.color = Color(r, g, b, a);
		points.push_back(p);
	}
	color_ramp->set_points(points);
	switch (node->mGradient.type) {
		case LOTGradientType::GradientLinear: {
			print_verbose("{GradientLinear}");
			Ref<VGLinearGradient> vg_gradient;
			vg_gradient.instance();
			vg_gradient->set_color_ramp(color_ramp);
			vg_gradient->set_p1(Vector2(node->mGradient.start.x, node->mGradient.start.y));
			vg_gradient->set_p2(Vector2(node->mGradient.end.x, node->mGradient.end.y));
			path->set_fill_color(vg_gradient);
			path->set_line_color(vg_gradient);
			break;
		}
		case LOTGradientType::GradientRadial: {
			print_verbose("{GradientRadial}");
			Ref<VGRadialGradient> vg_gradient;
			vg_gradient.instance();
			Vector2 center;
			center.x = node->mGradient.center.x;
			center.y = node->mGradient.center.y;
			vg_gradient->set_center(center);
			Vector2 focal;
			center.x = node->mGradient.focal.x;
			center.y = node->mGradient.focal.y;
			vg_gradient->set_focal(focal);
			float radius = node->mGradient.cradius;
			vg_gradient->set_radius(radius);
			vg_gradient->set_color_ramp(color_ramp);
			path->set_fill_color(vg_gradient);
			path->set_line_color(vg_gradient);
			break;
		}
		default:
			break;
	}
}

String ResourceImporterLottie::get_importer_name() const {
	return "lottie";
}

String ResourceImporterLottie::get_visible_name() const {
	return "LOTTIE";
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
	String path = ProjectSettings::get_singleton()->globalize_path(p_source_file);
	std::unique_ptr<rlottie::Animation> lottie =
			rlottie::Animation::loadFromFile(path.utf8().ptr());
	ERR_FAIL_COND_V(!lottie, FAILED);
	size_t w = 0;
	size_t h = 0;
	lottie->size(w, h);
	ERR_FAIL_COND_V(w == 0, FAILED);
	ERR_FAIL_COND_V(h == 0, FAILED);
	VGPath *root = memnew(VGPath);
	Ref<VGSpriteRenderer> renderer;
	renderer.instance();
	root->set_renderer(renderer);
	String base_name = p_source_file.get_file().get_basename();
	root->set_name(base_name);
	ERR_FAIL_COND_V(!lottie->totalFrame(), FAILED);
	String frame_root_path;
	{
		VGPathAnimation *frame_root = memnew(VGPathAnimation());
		root->add_child(frame_root);
		frame_root->set_owner(root);
		for (int32_t frame_i = 0; frame_i < lottie->totalFrame(); frame_i++) {
			const LOTLayerNode *tree = lottie->renderTree(frame_i, w, h);
			VGPath *frame = memnew(VGPath());
			frame_root->add_frame(frame);
			_visit_layer_node(tree, root, frame);
			if (frame_i) {
				frame->set_visible(false);
			}
		}
		frame_root_path = String(root->get_path_to(frame_root)) + ":frame";
	}
	{
		AnimationPlayer *ap = memnew(AnimationPlayer);
		Ref<Animation> animation;
		animation.instance();
		float hertz = 1.0f / lottie->frameRate();
		int32_t track = animation->get_track_count();
		animation->add_track(Animation::TYPE_VALUE);
		animation->set_length((lottie->totalFrame() - 1) * hertz);
		animation->track_set_path(track, frame_root_path);
		animation->track_insert_key(track, float(0) * hertz, 0);
		animation->track_insert_key(track, float(lottie->totalFrame() - 1) * hertz, lottie->totalFrame() - 1);
		animation->set_loop(true);
		ap->add_animation("Default", animation);
		root->add_child(ap);
		ap->set_owner(root);
	}
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

void ResourceImporterLottie::_visit_layer_node(const LOTLayerNode *layer, Node *p_owner, VGPath *p_current_node) {
	if (!layer->mVisible) {
		return;
	}
	//Don't need to update it anymore since its layer is invisible.
	if (layer->mAlpha == 0) {
		return;
	}
	//Is this layer a container layer?
	for (unsigned int i = 0; i < layer->mLayerList.size; i++) {
		LOTLayerNode *clayer = layer->mLayerList.ptr[i];
		_visit_layer_node(clayer, p_owner, p_current_node);
	}

	//visit render nodes.
	if (layer->mNodeList.size > 0) {
		_visit_render_node(layer, p_owner, p_current_node);
	}
}
