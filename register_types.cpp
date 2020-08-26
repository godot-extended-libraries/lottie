/*************************************************************************/
/*  register_types.cpp                                                   */
/*************************************************************************/

#include "register_types.h"
#include "core/io/resource_importer.h"
#include "resource_importer_lottie.h"


void register_lottie_types() {
	Ref<ResourceImporterLottie> lottie_sprite_animation;
	lottie_sprite_animation.instance();
	ResourceFormatImporter::get_singleton()->add_importer(lottie_sprite_animation);
}

void unregister_lottie_types() {
}