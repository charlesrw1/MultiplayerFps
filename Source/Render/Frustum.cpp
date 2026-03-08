#include "Frustum.h"
#include "DrawLocal.h"

void build_frustum_for_cascade(Frustum& f, int index)
{
	f = build_frustom_for_ortho(draw.shadowmap.matricies[index]);
	f.ortho_max_extent = draw.shadowmap.max_extents[index];
}

Frustum build_frustom_for_ortho(const glm::mat4& ortho_viewproj)
{
	Frustum f;
	auto inv = glm::inverse(ortho_viewproj);
	const glm::vec3 front = -inv[2];
	const glm::vec3 side = inv[0];
	const glm::vec3 up = inv[1];
	glm::vec3 corners[8];
	for (int i = 0; i < 8; i++) {
		glm::vec3 v = glm::vec3(1, 1, 1);
		if (i % 2 == 1)v.x = -1;
		if (i % 4 >= 2) v.y = -1;
		if (i / 4 == 1) v.z = 0;
		corners[i] = inv * glm::vec4(v, 1.f);
	}
	glm::vec3 n = glm::normalize(corners[2] - corners[0]);
	f.top_plane = glm::vec4(n, -glm::dot(n, corners[0]));

	f.bot_plane = glm::vec4(-n, glm::dot(n, corners[2]));
	n = glm::normalize(corners[1] - corners[0]);
	f.right_plane = glm::vec4(n, -glm::dot(n, corners[0]));
	f.left_plane = glm::vec4(-n, glm::dot(n, corners[1]));
	f.is_ortho = true;
	return f;
}

void build_a_frustum_for_perspective(Frustum& f, const View_Setup& view, glm::vec3* arrow_origin)
{
	if (view.is_ortho) {
		f = build_frustom_for_ortho(view.viewproj);
		f.is_ortho = true;
		return;
	}

	const float fakeFar = 5.0;
	const float fakeNear = 0.0;
	const float aratio = view.width / (float)view.height;

	auto inv = glm::inverse(view.view);

	const glm::vec3 front = -inv[2];
	const glm::vec3 side = inv[0];
	const glm::vec3 up = inv[1];

	const float halfVSide = fakeFar * tanf(view.fov * .5f);
	const float halfHSide = halfVSide * aratio;

	glm::vec3 corners[4];
	corners[0] = front * fakeFar + halfHSide * side + halfVSide * up;
	corners[1] = front * fakeFar + halfHSide * side - halfVSide * up;
	corners[2] = front * fakeFar - halfHSide * side - halfVSide * up;
	corners[3] = front * fakeFar - halfHSide * side + halfVSide * up;

	if (arrow_origin) {
		arrow_origin[3] = front * fakeFar + halfVSide * up; // bottom
		arrow_origin[1] = front * fakeFar - halfVSide * up; // up
		arrow_origin[0] = front * fakeFar + halfHSide * side;	// right
		arrow_origin[2] = front * fakeFar - halfHSide * side;	// left
		for (int i = 0; i < 4; i++)
			arrow_origin[i] = arrow_origin[i] * 0.5f + view.origin;
	}

	glm::vec3 normals[4];
	normals[0] = -glm::normalize(glm::cross(corners[0], up));	//right
	normals[1] = -glm::normalize(glm::cross(corners[1], side));	// up
	normals[2] = glm::normalize(glm::cross(corners[2], up));	// left
	normals[3] = glm::normalize(glm::cross(corners[3], side)); // bottom

	f.top_plane = glm::vec4(normals[1], -glm::dot(normals[1], view.origin));
	f.bot_plane = glm::vec4(normals[3], -glm::dot(normals[3], view.origin));
	f.right_plane = glm::vec4(normals[0], -glm::dot(normals[0], view.origin));
	f.left_plane = glm::vec4(normals[2], -glm::dot(normals[2], view.origin));

	f.fov = view.fov;
	f.origin = view.origin;
}

Frustum build_frustum_for_light(RL_Internal& light)
{
	auto& p = light.light.position;
	auto& n = light.light.normal;

	Frustum frustum;
	View_Setup setup;
	glm::vec3 up = glm::vec3(0, 1, 0);
	if (glm::abs(glm::dot(up, n)) > 0.999)
		up = glm::vec3(1, 0, 0);
	setup.view = glm::lookAt(p, p + n, up);
	setup.origin = p;
	setup.width = setup.height = 1;	//aratio=1
	setup.fov = glm::radians(light.light.conemax) * 2.0;
	build_a_frustum_for_perspective(frustum, setup, nullptr);

	glm::vec4 backplane = glm::vec4(-n, 0.0);
	backplane.w = glm::dot(n, p + n * light.light.radius);

	frustum.back_plane = backplane;

	return frustum;
}
