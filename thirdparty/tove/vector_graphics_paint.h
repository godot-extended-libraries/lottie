/*************************************************************************/
/*  vg_paint.h 		                                                     */
/*************************************************************************/

#ifndef VG_PAINT_H
#define VG_PAINT_H

#include "core/resource.h"
#include "tove2d/src/cpp/common.h"
#include "tove2d/src/cpp/paint.h"

class VGPaint : public Resource {
	GDCLASS(VGPaint, Resource);

public:
	tove::PaintRef data = tove::tove_make_shared<tove::Color>(0.0f, 0.0f, 0.0f);
};

#endif // VG_PAINT_H
