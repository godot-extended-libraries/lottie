/*************************************************************************/
/*  resource_importer_lottie.h   	                                     */
/*************************************************************************/

#ifndef RESOURCE_IMPORTER_LOTTIE
#define RESOURCE_IMPORTER_LOTTIE

#include "core/io/resource_saver.h"
#include "scene/3d/mesh_instance.h"
#include "scene/3d/spatial.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/primitive_meshes.h"

#include "thirdparty/rlottie/inc/rlottie.h"
#include "thirdparty/rlottie/inc/rlottiecommon.h"
#include "thirdparty/tove/tove2d/src/cpp/graphics.h"
#include "thirdparty/tove/vector_graphics_adaptive_renderer.h"
#include "thirdparty/tove/vector_graphics_editor.h"
#include "thirdparty/tove/vector_graphics_path.h"
#include "thirdparty/tove/vector_graphics_texture_renderer.h"

class ResourceImporterLottie : public ResourceImporter {
	GDCLASS(ResourceImporterLottie, ResourceImporter);
	void _visit_render_node(const LOTLayerNode *layer, Node *p_owner, Node *p_current_node);
	void _visit_layer_node(const LOTLayerNode *layer, Node *p_owner, Node *p_current_node);

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

	ResourceImporterLottie();
	~ResourceImporterLottie();
};

/*
   The LayerNode can contain list of child layer node
   or a list of render node which will be having the 
   path and fill information.
   so visit_layer_node() checks if it is a composite layer
   then it calls visit_layer_node() for all its child layer
   otherwise calls visit_render_node() which try to visit all
   the nodes present in the NodeList.

   Note: renderTree() was only meant for internal use and only c structs
   for not duplicating the data. so never save the raw pointers.
   If possible try to avoid using it. 

   https://github.com/Samsung/rlottie/issues/384#issuecomment-670319668
 */
void ResourceImporterLottie::_visit_layer_node(const LOTLayerNode *layer, Node *p_owner, Node *p_current_node) {
	if (!layer->mVisible) {
		return;
	}
	//Don't need to update it anymore since its layer is invisible.
	if (layer->mAlpha == 0) {
		return;
	}

	VGPath *path = memnew(VGPath);
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

void ResourceImporterLottie::_visit_render_node(const LOTLayerNode *layer, Node *p_owner, Node *p_current_node) {
	tove::GraphicsRef tove_graphics = tove::tove_make_shared<tove::Graphics>();
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
		path_ref->setLineWidth(node->mStroke.width);

		tove::SubpathRef subpath_ref = std::make_shared<tove::Subpath>();

		const float *data = node->mPath.ptPtr;
		if (!data) {
			continue;
		}
		path_ref->addSubpath(subpath_ref);
		for (size_t i = 0; i < node->mPath.elmCount; i++) {
			String path_print;
			switch (node->mPath.elmPtr[i]) {
				case 0: {
					path_print = "{MoveTo : (" + rtos(data[0]) + " " + rtos(data[1]) + ")}";
					data += 2;
					subpath_ref->moveTo(data[0], data[1]);
					break;
				}
				case 1: {
					path_print = "{LineTo : (" + rtos(data[0]) + " " + rtos(data[1]) + ")}";
					data += 2;
					subpath_ref->lineTo(data[0], data[1]);
					break;
				}
				case 2: {
					path_print = "{CubicTo : c1(" + rtos(data[0]) + " " + rtos(data[1]) + ") c2(" + rtos(data[2]) + " " + rtos(data[3]) + ") e(" + rtos(data[4]) + " " + rtos(data[5]) + ")}";
					data += 6;
					subpath_ref->curveTo(data[0], data[1], data[2], data[3], data[4], data[5]);
					break;
				}
				case 3: {
					path_print = "{close}";
					break;
				}
				default:
					break;
			}
			print_line(path_print);
		}
		//1: Stroke
		if (node->mStroke.enable) {
			// 	Stroke Width
			// 	efl_gfx_shape_stroke_width_set(shape, node->mStroke.width);

			// 	Stroke Cap
			// 	Efl_Gfx_Cap cap;
			switch (node->mStroke.cap) {
				case CapFlat:
					print_line("{CapFlat}");
					break;
				case CapSquare:
					print_line("{CapSquare}");
					break;
				case CapRound:
					print_line("{CapRound}");
					break;
				default:
					print_line("{CapFlat}");
					break;
			}

			//Stroke Join
			switch (node->mStroke.join) {
				case LOTJoinStyle::JoinMiter:
					print_line("{JoinMiter}");
					path_ref->setLineJoin(TOVE_LINEJOIN_MITER);
					break;
				case LOTJoinStyle::JoinBevel:
					print_line("{JoinBevel}");
					path_ref->setLineJoin(TOVE_LINEJOIN_BEVEL);
					break;
				case LOTJoinStyle::JoinRound:
					print_line("{JoinRound}");
					path_ref->setLineJoin(TOVE_LINEJOIN_ROUND);
					break;
				default:
					print_line("{JoinMiter}");
					path_ref->setLineJoin(TOVE_LINEJOIN_MITER);
					break;
			}
			//Stroke Dash
			if (node->mStroke.dashArraySize > 0) {
				for (int i = 0; i <= node->mStroke.dashArraySize; i += 2) {
					print_line("{(length, gap) : (" + rtos(node->mStroke.dashArray[i]) + " " + rtos(node->mStroke.dashArray[i + 1]) + ")}");
				}
				path_ref->setLineDash(node->mStroke.dashArray, node->mStroke.dashArraySize);
			}
		}
		//Fill Method
		Ref<VGPaint> paint;
		paint.instance();
		switch (node->mBrushType) {
			case BrushSolid: {
				float pa = ((float)node->mColor.a) / 255;
				float r = (int)(((float)node->mColor.r) * pa);
				float g = (int)(((float)node->mColor.g) * pa);
				float b = (int)(((float)node->mColor.b) * pa);
				float a = node->mColor.a;
				tove::PaintRef fill_ref = tove::tove_make_shared<tove::Color>(r, g, b, a);
				path_ref->setFillColor(fill_ref);
				tove::PaintRef line_ref = std::make_shared<tove::Color>(r, g, b);
				path_ref->setLineColor(line_ref);
			} break;
			case BrushGradient: {
				// same way extract brush gradient value.
			} break;
			default:
				break;
		}

		//Fill Rule
		if (node->mFillRule == LOTFillRule::FillEvenOdd) {
			path_ref->setFillRule(TOVE_FILLRULE_EVEN_ODD);
			print_line("{FillEvenOdd}");
		} else if (node->mFillRule == LOTFillRule::FillWinding) {
			path_ref->setFillRule(TOVE_FILLRULE_NON_ZERO);
			print_line("{FillWinding}");
		}

		VGPath *path = memnew(VGPath(path_ref));
		Ref<VGSpriteRenderer> renderer;
		renderer.instance();
		path->set_renderer(renderer);
		p_current_node->add_child(path);
		path->set_owner(p_owner);
		path->set_dirty(true);
	}
}

String ResourceImporterLottie::get_importer_name() const {
	return "lottie";
}

String ResourceImporterLottie::get_visible_name() const {
	return "LOTTIE";
}

void ResourceImporterLottie::get_recognized_extensions(
		List<String> *p_extensions) const {
	p_extensions->push_back("json");
}

String ResourceImporterLottie::get_save_extension() const {
	return "scn";
}

String ResourceImporterLottie::get_resource_type() const {
	return "PackedScene";
}

bool ResourceImporterLottie::get_option_visibility(
		const String &p_option, const Map<StringName, Variant> &p_options) const {
	return true;
}

int ResourceImporterLottie::get_preset_count() const {
	return 0;
}
String ResourceImporterLottie::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterLottie::get_import_options(List<ImportOption> *r_options,
		int p_preset) const {}

Error ResourceImporterLottie::import(const String &p_source_file,
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
	ERR_FAIL_COND_V(!lottie->frameRate(), FAILED);
	for (int32_t frame_i = 0; frame_i < 1; frame_i++) {
		const LOTLayerNode *tree = lottie->renderTree(frame_i, w, h);
		_visit_layer_node(tree, root, root);
	}
	Ref<PackedScene> scene;
	scene.instance();
	scene->pack(root);
	String save_path = p_save_path + ".scn";
	r_gen_files->push_back(save_path);
	return ResourceSaver::save(save_path, scene);
}

ResourceImporterLottie::ResourceImporterLottie() {}

ResourceImporterLottie::~ResourceImporterLottie() {}

#endif // RESOURCE_IMPORTER_LOTTIE
