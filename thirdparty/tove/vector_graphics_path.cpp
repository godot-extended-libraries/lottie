/*************************************************************************/
/*  vg_path.cpp  	                                                     */
/*************************************************************************/

#include "vector_graphics_path.h"
#include "core/os/file_access.h"
#include "editor/editor_node.h"
#include "scene/2d/sprite.h"
#include "vector_graphics_adaptive_renderer.h"
#include "vector_graphics_color.h"
#include "vector_graphics_linear_gradient.h"
#include "vector_graphics_radial_gradient.h"

static tove::PaintRef to_tove_paint(Ref<VGPaint> p_paint) {
	Ref<VGColor> color = p_paint;
	if (color.is_null()) {
		Ref<VGGradient> gradient = p_paint;
		if (gradient.is_null()) {
			return tove::PaintRef();
		} else {
			Ref<VGLinearGradient> linear_gradient = p_paint;
			Ref<VGRadialGradient> radial_gradient = p_paint;

			tove::PaintRef tove_gradient;

			if (!linear_gradient.is_null()) {
				Vector2 p1 = linear_gradient->get_p1();
				Vector2 p2 = linear_gradient->get_p2();
				tove_gradient = tove::tove_make_shared<tove::LinearGradient>(
						p1.x, p1.y, p2.x, p2.y);
			} else if (!radial_gradient.is_null()) {
				Vector2 center = radial_gradient->get_center();
				Vector2 focal = radial_gradient->get_focal();
				float radius = radial_gradient->get_radius();
				tove_gradient = tove::tove_make_shared<tove::RadialGradient>(
						center.x, center.y, focal.x, focal.y, radius);
			} else {
				return tove::PaintRef();
			}

			Ref<Gradient> color_ramp = gradient->get_color_ramp();
			if (!color_ramp.is_null()) {
				const Vector<Gradient::Point> &p = color_ramp->get_points();
				for (int i = 0; i < p.size(); i++) {
					tove_gradient->addColorStop(
							p[i].offset,
							p[i].color.r,
							p[i].color.g,
							p[i].color.b,
							p[i].color.a);
				}
			}

			return tove_gradient;
		}
	} else {
		const Color c = color->get_color();
		return tove::tove_make_shared<tove::Color>(c.r, c.g, c.b, c.a);
	}
}

static Ref<VGPaint> from_tove_paint(const tove::PaintRef &p_paint) {
	if (p_paint) {
		if (p_paint->isGradient()) {
			Ref<Gradient> color_ramp;
			color_ramp.instance();

			auto grad = std::dynamic_pointer_cast<tove::AbstractGradient>(p_paint);

			Vector<Gradient::Point> points;
			for (int i = 0; i < grad->getNumColorStops(); i++) {
				Gradient::Point p;
				ToveRGBA rgba;
				p.offset = grad->getColorStop(i, rgba, 1.0f);
				p.color = Color(rgba.r, rgba.g, rgba.b, rgba.a);
				points.push_back(p);
			}
			color_ramp->set_points(points);

			ToveGradientParameters params;
			grad->getGradientParameters(params);

			auto lgrad = std::dynamic_pointer_cast<tove::LinearGradient>(p_paint);
			auto rgrad = std::dynamic_pointer_cast<tove::RadialGradient>(p_paint);
			if (lgrad.get()) {
				Ref<VGLinearGradient> linear_gradient;
				linear_gradient.instance();
				linear_gradient->set_p1(Vector2(params.values[0], params.values[1]));
				linear_gradient->set_p2(Vector2(params.values[2], params.values[3]));
				linear_gradient->set_color_ramp(color_ramp);
				return linear_gradient;
			} else if (rgrad.get()) {
				Ref<VGRadialGradient> radial_gradient;
				radial_gradient.instance();
				radial_gradient->set_center(Vector2(params.values[0], params.values[1]));
				radial_gradient->set_focal(Vector2(params.values[2], params.values[3]));
				radial_gradient->set_radius(params.values[4]);
				radial_gradient->set_color_ramp(color_ramp);
				return radial_gradient;
			}

			return Ref<VGPaint>();
		} else {
			ToveRGBA rgba;
			p_paint->getRGBA(rgba, 1.0f);
			Ref<VGColor> color;
			color.instance();
			color->set_color(Color(rgba.r, rgba.g, rgba.b, rgba.a));
			return color;
		}
	} else {
		return Ref<VGPaint>();
	}
}

static Point2 compute_center(const tove::PathRef &p_path) {
	const float *bounds = p_path->getBounds();
	return Point2((bounds[0] + bounds[2]) / 2, (bounds[1] + bounds[3]) / 2);
}

void VGPath::import_svg(const String &p_path) {
	String units = "px";
	float dpi = 96.0;

	Vector<uint8_t> buf = FileAccess::get_file_as_array(p_path);

	String str;
	str.parse_utf8((const char *)buf.ptr(), buf.size());

	tove::GraphicsRef tove_graphics = tove::Graphics::createFromSVG(
			str.utf8().ptr(), units.utf8().ptr(), dpi);

	const float *bounds = tove_graphics->getBounds();

	float s = 256.0f / MAX(bounds[2] - bounds[0], bounds[3] - bounds[1]);
	if (s > 1.0f) {
		tove::nsvg::Transform transform(s, 0, 0, 0, s, 0);
		transform.setWantsScaleLineWidth(true);
		tove_graphics->set(tove_graphics, transform);
	}

	const int n = tove_graphics->getNumPaths();
	for (int i = 0; i < n; i++) {
		tove::PathRef tove_path = tove_graphics->getPath(i);
		Point2 center = compute_center(tove_path);
		tove_path->set(tove_path, tove::nsvg::Transform(1, 0, -center.x, 0, 1, -center.y));

		VGPath *path = memnew(VGPath(tove_path));
		path->set_position(center);

		std::string name = tove_path->getName();
		if (name.empty()) {
			name = "Path";
		}

		path->set_name(String(name.c_str()));

		add_child(path);
		path->set_owner(get_owner());
	}
}

void VGPath::recenter() {
	Point2 center = compute_center(tove_path);
	tove_path->set(tove_path, tove::nsvg::Transform(1, 0, -center.x, 0, 1, -center.y));
	vg_transform.translate(center.x, center.y);

	set_position(get_position() + get_transform().untranslated().xform(vg_transform.elements[2]));
	vg_transform = vg_transform.untranslated();
	set_dirty();
}

tove::GraphicsRef VGPath::create_tove_graphics() const {
	tove::GraphicsRef tove_graphics = tove::tove_make_shared<tove::Graphics>();
	tove_graphics->addPath(tove_path);
	return tove_graphics;
}

void VGPath::add_tove_path(const tove::GraphicsRef &p_tove_graphics) const {
	const Transform2D t = get_transform() * vg_transform;

	const Vector2 &tx = t.elements[0];
	const Vector2 &ty = t.elements[1];
	const Vector2 &to = t.elements[2];
	tove::nsvg::Transform tove_transform(tx.x, ty.x, to.x, tx.y, ty.y, to.y);

	const tove::PathRef transformed_path = tove::tove_make_shared<tove::Path>();
	transformed_path->set(tove_path, tove_transform);
	p_tove_graphics->addPath(transformed_path);
}

void VGPath::set_inherited_dirty(Node *p_node) {
	const int n = p_node->get_child_count();
	for (int i = 0; i < n; i++) {
		Node *child = p_node->get_child(i);
		if (child->is_class_ptr(get_class_ptr_static())) {
			VGPath *path = Object::cast_to<VGPath>(child);
			if (path->inherits_renderer()) {
				path->set_dirty();
				set_inherited_dirty(path);
			}
		} else {
			set_inherited_dirty(child);
		}
	}
}

void VGPath::compose_graphics(const tove::GraphicsRef &p_tove_graphics,
		const Transform2D &p_transform, const Node *p_node) {
	const int n = p_node->get_child_count();
	for (int i = 0; i < n; i++) {
		Node *child = p_node->get_child(i);

		Transform2D t;
		if (child->is_class_ptr(CanvasItem::get_class_ptr_static())) {
			t = p_transform * Object::cast_to<CanvasItem>(child)->get_transform();
		} else {
			t = p_transform;
		}

		compose_graphics(p_tove_graphics, t, child);
	}

	if (p_node->is_class_ptr(get_class_ptr_static())) {
		const VGPath *path = Object::cast_to<VGPath>(p_node);
		p_tove_graphics->addPath(new_transformed_path(
				path->get_tove_path(), p_transform));
	}
}

bool VGPath::inherits_renderer() const {
	return renderer.is_null();
}

VGPath *VGPath::get_root_path() {
	VGPath *root = this;
	Node *node = get_parent();
	while (node) {
		if (node->is_class_ptr(get_class_ptr_static())) {
			root = Object::cast_to<VGPath>(node);
		}
		node = node->get_parent();
	}
	return root;
}

Ref<VGRenderer> VGPath::get_inherited_renderer() const {
	if (!inherits_renderer()) {
		return renderer;
	}

	Node *node = get_parent();
	while (node) {
		if (node->is_class_ptr(get_class_ptr_static())) {
			VGPath *path = Object::cast_to<VGPath>(node);
			if (path->renderer.is_valid()) {
				return path->renderer;
			}
		}
		node = node->get_parent();
	}
	return Ref<VGRenderer>();
}

void VGPath::update_mesh_representation() {
	if (!dirty) {
		return;
	}
	dirty = false;

	if (!is_empty()) {
		Ref<VGRenderer> renderer = get_inherited_renderer();
		if (renderer.is_valid()) {
			Ref<Material> ignored_material; // ignored
			Ref<Texture> ignored_texture; // ignored
			renderer->render_mesh(mesh, ignored_material, ignored_texture, this, false, false);
			texture = renderer->render_texture(this, false);
		}
	}
}

void VGPath::update_tove_fill_color() {
	tove_path->setFillColor(to_tove_paint(fill_color));
}

void VGPath::update_tove_line_color() {
	tove_path->setLineColor(to_tove_paint(line_color));
}

void VGPath::create_fill_color() {
	tove::PaintRef paint_ref = tove_path->getFillColor();
	if (!paint_ref) {
		return;
	}
	Ref<VGColor> vgcolor;
	vgcolor.instance();
	Color c;
	ToveRGBA rgba;
	paint_ref->getRGBA(rgba, 1.0f);
	c.r = rgba.r;
	c.g = rgba.g;
	c.b = rgba.b;
	c.a = rgba.a;
	vgcolor->set_color(c);
	set_fill_color(vgcolor);
}

void VGPath::create_line_color() {
	set_line_color(from_tove_paint(tove_path->getLineColor()));
}

void VGPath::_renderer_changed() {
	set_dirty();
	set_inherited_dirty(this);
}

void VGPath::_bubble_change() {
	Node *node = get_parent();
	while (node) {
		if (node->is_class_ptr(get_class_ptr_static())) {
			Object::cast_to<VGPath>(node)->subtree_graphics = tove::GraphicsRef();
		}
		node = node->get_parent();
	}
}

void VGPath::_transform_changed(Node *p_node) {
	if (p_node->is_class_ptr(get_class_ptr_static())) {
		VGPath *path = Object::cast_to<VGPath>(p_node);
		Ref<VGRenderer> renderer = path->get_inherited_renderer();
		if (renderer.is_valid() && renderer->is_dirty_on_transform_change()) {
			path->set_dirty();
		}
	}
}

void VGPath::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			update_mesh_representation();
			if (!is_empty()) {
				draw_mesh(mesh, texture, Ref<Texture>());
			}
		} break;
		case NOTIFICATION_PARENTED: {
			_bubble_change();
			if (inherits_renderer()) {
				set_dirty();
				set_inherited_dirty(this);
			}
		} break;
		case NOTIFICATION_UNPARENTED: {
			set_dirty();
			_bubble_change();
		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (is_inside_tree()) {
				_bubble_change();
				_transform_changed(this);
			}
		} break;
	}
}

void VGPath::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_renderer", "renderer"), &VGPath::set_renderer);
	ClassDB::bind_method(D_METHOD("get_renderer"), &VGPath::get_renderer);

	ClassDB::bind_method(D_METHOD("set_fill_color", "color"), &VGPath::set_fill_color);
	ClassDB::bind_method(D_METHOD("get_fill_color"), &VGPath::get_fill_color);

	ClassDB::bind_method(D_METHOD("set_line_color", "color"), &VGPath::set_line_color);
	ClassDB::bind_method(D_METHOD("get_line_color"), &VGPath::get_line_color);

	ClassDB::bind_method(D_METHOD("set_line_width", "width"), &VGPath::set_line_width);
	ClassDB::bind_method(D_METHOD("get_line_width"), &VGPath::get_line_width);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "renderer", PROPERTY_HINT_RESOURCE_TYPE, "VGRenderer"), "set_renderer", "get_renderer");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fill_color", PROPERTY_HINT_RESOURCE_TYPE, "VGPaint"), "set_fill_color", "get_fill_color");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "line_color", PROPERTY_HINT_RESOURCE_TYPE, "VGPaint"), "set_line_color", "get_line_color");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "line_width", PROPERTY_HINT_RANGE, "0,100,0.01"), "set_line_width", "get_line_width");

	ClassDB::bind_method(D_METHOD("insert_curve", "subpath", "t"), &VGPath::insert_curve);
	ClassDB::bind_method(D_METHOD("remove_curve", "subpath", "curve"), &VGPath::remove_curve);
	ClassDB::bind_method(D_METHOD("set_points", "subpath", "points"), &VGPath::set_points);

	ClassDB::bind_method(D_METHOD("_renderer_changed"), &VGPath::_renderer_changed);
	ClassDB::bind_method(D_METHOD("recenter"), &VGPath::recenter);

	ClassDB::bind_method(D_METHOD("import_svg", "path"), &VGPath::import_svg);
}

bool VGPath::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;
	set_dirty();

	if (name == "name") {
		String s = p_value;
		CharString t = s.utf8();
		tove_path->setName(t.get_data());
	} else if (name == "line_width") {
		tove_path->setLineWidth(p_value);
	} else if (name == "fill_rule") {
		if (p_value == "nonzero") {
			tove_path->setFillRule(TOVE_FILLRULE_NON_ZERO);
		} else if (p_value == "evenodd") {
			tove_path->setFillRule(TOVE_FILLRULE_EVEN_ODD);
		} else {
			return false;
		}
	} else if (name.begins_with("subpaths/")) {
		int subpath = name.get_slicec('/', 1).to_int();
		String subwhat = name.get_slicec('/', 2);

		if (tove_path->getNumSubpaths() == subpath) {
			tove::SubpathRef tove_subpath = tove::tove_make_shared<tove::Subpath>();
			tove_path->addSubpath(tove_subpath);
		}

		ERR_FAIL_INDEX_V(subpath, tove_path->getNumSubpaths(), false);
		tove::SubpathRef tove_subpath = tove_path->getSubpath(subpath);

		if (subwhat == "closed") {
			tove_subpath->setIsClosed(p_value);
		}
		if (subwhat == "points") {
			PoolVector2Array pts = p_value;
			const int n = pts.size();
			float *buf = new float[n * 2];
			for (int i = 0; i < n; i++) {
				const Vector2 &p = pts[i];
				buf[2 * i + 0] = p.x;
				buf[2 * i + 1] = p.y;
			}
			tove_subpath->setPoints(buf, n, false);
			delete[] buf;
		} else {
			return false;
		}
	} else {
		return false;
	}

	return true;
}

bool VGPath::_get(const StringName &p_name, Variant &r_ret) const {
	String name = p_name;

	if (name == "name") {
		r_ret = String::utf8(tove_path->getName());
	} else if (name == "line_width") {
		r_ret = tove_path->getLineWidth();
	} else if (name == "fill_rule") {
		switch (tove_path->getFillRule()) {
			case TOVE_FILLRULE_NON_ZERO: {
				r_ret = "nonzero";
			} break;
			case TOVE_FILLRULE_EVEN_ODD: {
				r_ret = "evenodd";
			} break;
			default: {
				return false;
			} break;
		}
	} else if (name.begins_with("subpaths/")) {
		int subpath = name.get_slicec('/', 1).to_int();
		String subwhat = name.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(subpath, tove_path->getNumSubpaths(), false);
		tove::SubpathRef tove_subpath = tove_path->getSubpath(subpath);
		if (subwhat == "closed") {
			r_ret = tove_subpath->isClosed();
		} else if (subwhat == "points") {
			const int n = tove_subpath->getNumPoints();
			PoolVector2Array out;
			out.resize(n);
			{
				const float *pts = tove_subpath->getPoints();
				PoolVector2Array::Write out_write = out.write();
				for (int i = 0; i < n; i++) {
					out_write[i] = Vector2(pts[2 * i + 0], pts[2 * i + 1]);
				}
			}
			r_ret = out;
		} else {
			return false;
		}
	} else {
		return false;
	}

	return true;
}

void VGPath::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::STRING, "name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
	p_list->push_back(PropertyInfo(Variant::REAL, "line_width", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
	p_list->push_back(PropertyInfo(Variant::INT, "fill_rule", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));

	for (int j = 0; j < tove_path->getNumSubpaths(); j++) {
		const String subpath_prefix = "subpaths/" + itos(j);

		p_list->push_back(PropertyInfo(Variant::BOOL, subpath_prefix + "/closed", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::POOL_VECTOR2_ARRAY, subpath_prefix + "/points", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
	}
}

Ref<VGRenderer> VGPath::get_renderer() {
	return renderer;
}

void VGPath::set_renderer(const Ref<VGRenderer> &p_renderer) {
	if (renderer.is_valid()) {
		renderer->disconnect("changed", this, "_renderer_changed");
	}
	renderer = p_renderer;
	if (renderer.is_valid()) {
		renderer->connect("changed", this, "_renderer_changed");
	}

	set_inherited_dirty(this);
	set_dirty();
}

Ref<VGPaint> VGPath::get_fill_color() const {
	return fill_color;
}

void VGPath::set_fill_color(const Ref<VGPaint> &p_paint) {
	if (fill_color == p_paint) {
		return;
	}

	if (fill_color.is_valid()) {
		fill_color->remove_change_receptor(this);
	}
	fill_color = p_paint;
	if (fill_color.is_valid()) {
		fill_color->add_change_receptor(this);
	}

	update_tove_fill_color();
	set_dirty();
}

Ref<VGPaint> VGPath::get_line_color() const {
	return line_color;
}

void VGPath::set_line_color(const Ref<VGPaint> &p_paint) {
	if (line_color == p_paint) {
		return;
	}

	if (line_color.is_valid()) {
		line_color->remove_change_receptor(this);
	}
	line_color = p_paint;
	if (line_color.is_valid()) {
		line_color->add_change_receptor(this);
	}

	update_tove_line_color();
	set_dirty();
}

float VGPath::get_line_width() const {
	return tove_path->getLineWidth();
}

void VGPath::set_line_width(const float p_line_width) {
	tove_path->setLineWidth(p_line_width);
	set_dirty();
}

bool VGPath::is_inside(const Point2 &p_point) const {
	return tove_path->isInside(p_point.x, p_point.y);
}

VGPath *VGPath::find_clicked_child(const Point2 &p_point) {
	const int n = get_child_count();
	for (int i = 0; i < n; i++) {
		Node *child = get_child(i);
		if (child->is_class_ptr(get_class_ptr_static())) {
			VGPath *path = Object::cast_to<VGPath>(child);
			if (path->is_visible()) {
				Point2 p = (path->get_transform() * vg_transform).affine_inverse().xform(p_point);
				if (path->is_inside(p)) {
					return path;
				}
			}
		}
	}

	return nullptr;
}

Rect2 VGPath::_edit_get_rect() const {
	return tove_bounds_to_rect2(get_subtree_graphics()->getBounds());
}

bool VGPath::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	/*if (const_cast<VGPath*>(this)->get_root_path() != this) {
		return false;
	}*/
	return get_subtree_graphics()->hit(p_point.x, p_point.y) != tove::PathRef();
}

void VGPath::_edit_set_position(const Point2 &p_position) {
	set_position(p_position);
	update();
}

void VGPath::_edit_set_scale(const Size2 &p_scale) {
	/*Transform2D t;
	t.scale(p_scale);
	set_vg_transform(t);
	update();*/
}

void VGPath::_changed_callback(Object *p_changed, const char *p_prop) {
	if (fill_color.ptr() == p_changed) {
		update_tove_fill_color();
		set_dirty();
	}

	if (line_color.ptr() == p_changed) {
		update_tove_line_color();
		set_dirty();
	}
}

void VGPath::set_points(int p_subpath, Array p_points) {
	ERR_FAIL_INDEX(p_subpath, tove_path->getNumSubpaths());
	tove::SubpathRef tove_subpath = tove_path->getSubpath(p_subpath);
	const int n = p_points.size();
	float *p = new float[2 * n];
	for (int i = 0; i < n; i++) {
		const Vector2 q = p_points[i];
		p[2 * i + 0] = q.x;
		p[2 * i + 1] = q.y;
	}
	try {
		tove_subpath->setPoints(p, n, false);
		set_dirty();
	} catch (...) {
		delete[] p;
		throw;
	}
	delete[] p;
}

void VGPath::insert_curve(int p_subpath, float p_t) {
	ERR_FAIL_INDEX(p_subpath, tove_path->getNumSubpaths());
	tove::SubpathRef tove_subpath = tove_path->getSubpath(p_subpath);
	tove_subpath->insertCurveAt(p_t);
	set_dirty();
}

void VGPath::remove_curve(int p_subpath, int p_curve) {
	ERR_FAIL_INDEX(p_subpath, tove_path->getNumSubpaths());
	tove::SubpathRef tove_subpath = tove_path->getSubpath(p_subpath);
	tove_subpath->removeCurve(p_curve);
	set_dirty();
}

void VGPath::set_dirty(bool p_children) {
	if (p_children) {
		set_inherited_dirty(this);
		return;
	}

	Node *node = this;
	while (node) {
		if (node->is_class_ptr(get_class_ptr_static())) {
			Object::cast_to<VGPath>(node)->subtree_graphics = tove::GraphicsRef();
		}
		node = node->get_parent();
	}

	dirty = true;
	_change_notify("path_shape");
	update();
}

bool VGPath::is_empty() const {
	const int n = tove_path->getNumSubpaths();
	for (int i = 0; i < n; i++) {
		if (tove_path->getSubpath(i)->getNumPoints() > 0) {
			return false;
		}
	}
	return true;
}

int VGPath::get_num_subpaths() const {
	return tove_path->getNumSubpaths();
}

tove::SubpathRef VGPath::get_subpath(int p_subpath) const {
	return tove_path->getSubpath(p_subpath);
}

tove::PathRef VGPath::get_tove_path() const {
	return tove_path;
}

tove::GraphicsRef VGPath::get_subtree_graphics() const {
	if (!subtree_graphics) {
		subtree_graphics = tove::tove_make_shared<tove::Graphics>();
		compose_graphics(subtree_graphics, Transform2D(), this);
	}
	return subtree_graphics;
}

Node2D *VGPath::create_mesh_node() {
	Ref<VGRenderer> renderer = get_inherited_renderer();
	if (renderer.is_valid()) {
		if (renderer->prefer_sprite()) {
			Sprite *sprite = memnew(Sprite);
			sprite->set_texture(renderer->render_texture(this, true));
			Transform2D xform = get_transform();
			VGPath *current = this;
			while (current) {
				xform = current->get_transform() * xform;
				current = Object::cast_to<VGPath>(current->get_parent());
			}
			Transform2D t;
			t.scale(xform.get_scale());
			Size2 s = xform.get_scale();
			float scale = MAX(s.width, s.height);
			sprite->set_transform(get_transform() * t.affine_inverse());
			sprite->set_name(get_name());
			sprite->set_centered(false);

			return sprite;
		} else {
			MeshInstance2D *mesh_inst = memnew(MeshInstance2D);
			Ref<ArrayMesh> mesh;
			mesh.instance();
			Ref<Material> material;
			Ref<Texture> texture;
			renderer->render_mesh(mesh, material, texture, this, true, false);
			mesh_inst->set_mesh(mesh);
			if (material.is_valid()) {
				mesh_inst->set_material(material);
			}
			if (texture.is_valid()) {
				mesh_inst->set_texture(texture);
			}
			mesh_inst->set_name(get_name());
			return mesh_inst;
		}
	}

	return NULL;
}

void VGPath::set_tove_path(tove::PathRef p_path) {
	tove_path = p_path;
	create_fill_color();
	create_line_color();
	set_dirty();
}

/*void VGPath::set_vg_transform(const Transform2D &p_transform) {
	vg_transform = p_transform;
	set_dirty();
}*/

VGPath::VGPath() {
	tove_path = tove::tove_make_shared<tove::Path>();
	set_notify_transform(true);
}

VGPath::VGPath(tove::PathRef p_path) {
	set_notify_transform(true);
	set_tove_path(p_path);
}

VGPath::~VGPath() {
	if (fill_color.is_valid()) {
		fill_color->remove_change_receptor(this);
	}
	if (line_color.is_valid()) {
		line_color->remove_change_receptor(this);
	}
}

VGPath *VGPath::create_from_svg(Ref<Resource> p_resource) {
	if (!p_resource->get_path().ends_with(".svg")) {
		return NULL;
	}

	VGPath *root = memnew(VGPath(tove::tove_make_shared<tove::Path>()));

	Ref<VGMeshRenderer> renderer;
	renderer.instance();
	root->set_renderer(renderer);

	return root;
}

Node *createVectorSprite(Ref<Resource> p_resource) {
	VGPath *path = VGPath::create_from_svg(p_resource);
	if (path) {
		return path;
	} else {
		return memnew(Sprite); // not a vector file
	}
}

void configureVectorSprite(Node *p_child, Ref<Resource> p_resource) {
#ifdef TOOLS_ENABLED
	if (p_child->is_class_ptr(VGPath::get_class_ptr_static())) {
		EditorData *editor_data = EditorNode::get_singleton()->get_scene_tree_dock()->get_editor_data();
		editor_data->get_undo_redo().add_do_method(p_child, "import_svg", p_resource->get_path());
		editor_data->get_undo_redo().add_do_reference(p_child);
	}
#endif
}

void VGPathAnimation::set_data(String p_json) {
	data = p_json;
	lottie = rlottie::Animation::loadFromData(get_data().utf8().ptrw(), "", "", false);
}

String VGPathAnimation::get_data() const {
	return data;
}

int32_t VGPathAnimation::get_frame() const {
	return frame;
}

void VGPathAnimation::set_frame(int p_frame) {
	frame = p_frame;
	set_dirty();
}

void VGPathAnimation::_notification(int p_what) {
	if (p_what == VGPath::NOTIFICATION_DRAW) {
		ERR_FAIL_COND(!lottie);
		size_t w = 0;
		size_t h = 0;
		lottie->size(w, h);
		ERR_FAIL_COND(w == 0);
		ERR_FAIL_COND(h == 0);
		ERR_FAIL_COND(!lottie->totalFrame());
		clear();
		const LOTLayerNode *tree = lottie->renderTree(get_frame(), w, h);
		_visit_layer_node(tree, get_owner(), this);
	}
}

void VGPathAnimation::clear() {
	for (int32_t i = 0; i < get_child_count(); i++) {
		VGPath * path = cast_to<VGPath>(get_child(i));
		ERR_CONTINUE(!path);
		path->set_visible(false);
		path->queue_delete();
	}
}

void VGPathAnimation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &VGPathAnimation::clear);
	ClassDB::bind_method(D_METHOD("set_frame", "frame"), &VGPathAnimation::set_frame);
	ClassDB::bind_method(D_METHOD("get_frame"), &VGPathAnimation::get_frame);
	ClassDB::bind_method(D_METHOD("set_data", "json"), &VGPathAnimation::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &VGPathAnimation::get_data);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "data"), "set_data", "get_data");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "frame"), "set_frame", "get_frame");
}

void VGPathAnimation::_visit_render_node(const LOTLayerNode *layer, Node *p_owner, VGPath *p_current_node) {
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

void VGPathAnimation::_read_gradient(LOTNode *node, VGPath *path) {
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

inline void VGPathAnimation::_visit_layer_node(const LOTLayerNode *layer, Node *p_owner, VGPath *p_current_node) {
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
