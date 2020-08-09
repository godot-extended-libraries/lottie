/*************************************************************************/
/*  register_types.cpp                                                   */
/*************************************************************************/

#include "register_types.h"
#include "core/io/resource_importer.h"
#include "resource_importer_lottie.h"
#include "resource_importer_lottie_sprite.h"
#include "thirdparty/tove/vector_graphics_adaptive_renderer.h"
#include "thirdparty/tove/vector_graphics_color.h"
#include "thirdparty/tove/vector_graphics_gradient.h"
#include "thirdparty/tove/vector_graphics_linear_gradient.h"
#include "thirdparty/tove/vector_graphics_paint.h"
#include "thirdparty/tove/vector_graphics_path.h"
#include "thirdparty/tove/vector_graphics_radial_gradient.h"
#include "thirdparty/tove/vector_graphics_renderer.h"
#include "thirdparty/tove/vector_graphics_texture_renderer.h"
#include "thirdparty/tove/image_loader_svg_vgpath.h"

#ifdef TOOLS_ENABLED
#include "thirdparty/tove/vector_graphics_editor_plugin.h"
#endif

#ifdef TOOLS_ENABLED
static void editor_init_callback() {
	EditorNode *editor = EditorNode::get_singleton();
	editor->add_editor_plugin(memnew(VGEditorPlugin(editor)));
}
#endif

void register_lottie_types() {
	ClassDB::register_class<VGPath>();
	ClassDB::register_class<VGPathAnimation>();	
	ClassDB::register_virtual_class<VGPaint>();
	ClassDB::register_class<VGColor>();
	ClassDB::register_class<VGGradient>();
	ClassDB::register_class<VGLinearGradient>();
	ClassDB::register_class<VGRadialGradient>();

	ClassDB::register_virtual_class<VGRenderer>();
	ClassDB::register_class<VGSpriteRenderer>();
	ClassDB::register_class<VGMeshRenderer>();

#ifdef TOOLS_ENABLED
	EditorNode::add_init_callback(editor_init_callback);
#endif

	Ref<ResourceImporterLottie> lottie_spatial_loader;
	lottie_spatial_loader.instance();
	ResourceFormatImporter::get_singleton()->add_importer(lottie_spatial_loader);

	Ref<ResourceImporterLottieSprite> lottie_sprite_loader;
	lottie_sprite_loader.instance();
	ResourceFormatImporter::get_singleton()->add_importer(lottie_sprite_loader);

	Ref<ResourceImporterSVGVGPath> svg_vg_path_loader;
	svg_vg_path_loader.instance();
	ResourceFormatImporter::get_singleton()->add_importer(svg_vg_path_loader);
}

void unregister_lottie_types() {
}