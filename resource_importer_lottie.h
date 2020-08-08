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
#include "thirdparty/tove/vector_graphics_color.h"
#include "thirdparty/tove/vector_graphics_editor.h"
#include "thirdparty/tove/vector_graphics_linear_gradient.h"
#include "thirdparty/tove/vector_graphics_path.h"
#include "thirdparty/tove/vector_graphics_radial_gradient.h"
#include "thirdparty/tove/vector_graphics_texture_renderer.h"

class ResourceImporterLottie : public ResourceImporter {
	GDCLASS(ResourceImporterLottie, ResourceImporter);
	void _visit_render_node(const LOTLayerNode *layer, Node *p_owner, VGPath *p_current_node);

	void _read_gradient(LOTNode *node, VGPath *path, bool p_is_line);
	/* The LayerNode can contain list of child layer node
   or a list of render node which will be having the 
   path and fill information.
   so visit_layer_node() checks if it is a composite layer
   then it calls visit_layer_node() for all its child layer
   otherwise calls visit_render_node() which try to visit all
   the nodes present in the NodeList.

   Note: renderTree() was only meant for internal use and only c structs
   for not duplicating the data. so never save the raw pointers.
   If possible try to avoid using it. 

   https://github.com/Samsung/rlottie/issues/384#issuecomment-670319668 */
	void _visit_layer_node(const LOTLayerNode *layer, Node *p_owner, VGPath *p_current_node);

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
#endif // RESOURCE_IMPORTER_LOTTIE
