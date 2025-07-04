#include "TopDownPlayer.h"
#include "UI/UILoader.h"
#include "Animation/Runtime/RuntimeNodesNew2.h"
#include "Animation/Runtime/Animation.h"

static agBuilder make_player_tree(const Model* model)
{
	agBuilder out;
	agClipNode* clip = out.alloc<agClipNode>();
	clip->set_clip(model, "run_forward_unequip");
	clip->looping = true;

	agClipNode* runUpper = out.alloc<agClipNode>();
	runUpper->set_clip(model, "stand_rifle_run_n");
	runUpper->looping = true;

	agBlendMasked* masked = out.alloc<agBlendMasked>();
	masked->init_mask_for_model(model, 0.f);
	masked->set_one_bone_weight(model, "mixamorig:Spine", 0.1f);
	masked->set_one_bone_weight(model, "mixamorig:Spine1", 0.4f);
	masked->set_all_children_weights(model, "mixamorig:Spine2", 0.8f);
	masked->meshspace_blend = true;
	masked->input0 = clip;
	masked->input1 = runUpper;
	masked->alpha = 1.f;

	agClipNode* leaning = out.alloc<agClipNode>();
	leaning->set_clip(model, "RUN_LEAN_LEFT");

	agAddNode* add = out.alloc<agAddNode>();
	add->input0 = masked;
	add->input1 = leaning;
	add->alpha = StringName("flLean");

	agClipNode* stand = out.alloc<agClipNode>();
	stand->set_clip(model, "stand_rifle_aim_l");
	agClipNode* turn_l = out.alloc<agClipNode>();
	turn_l->set_clip(model, "turn_left");

	agBlendByInt* blendInt = out.alloc<agBlendByInt>();
	blendInt->easing = Easing::CubicEaseOut;
	blendInt->integer = StringName("iState");
	blendInt->inputs.push_back(add);
	blendInt->inputs.push_back(stand);
	blendInt->inputs.push_back(turn_l);

	out.set_root(blendInt);
	return out;
}

#include "Scripting/ScriptManager.h"

void TopDownPlayer::start() {
	mesh = get_owner()->get_component<MeshComponent>();
	assert(mesh);
	assert(mesh->get_model()&&mesh->get_model()->get_skel());
	//auto tree = make_player_tree(*mesh->get_model());
	agBuilder tree;
	auto factory = class_cast<PlayerAgFactory>(ScriptManager::inst->allocate_class("PlayerAgFactoryImpl"));
	if (factory) {
		factory->create(mesh->get_model(), &tree);
	}
	auto animator = mesh->create_animator(&tree);

	animator->set_float_variable("flLean", GetTime());
	animator->set_int_variable("iState",0);



	capsule = get_owner()->get_component<CapsuleComponent>();
	assert(mesh && capsule);
	{
		auto cameraobj = eng->get_level()->spawn_entity();
		the_camera = cameraobj->create_component<CameraComponent>();
		the_camera->set_is_enabled(true);
		ASSERT(CameraComponent::get_scene_camera() == the_camera);
	}

	if (shotgunSoundAsset && !shotgunSoundAsset->did_load_fail())
		cachedShotgunSound = eng->get_level()->spawn_prefab(shotgunSoundAsset)->get_component<SoundComponent>();


	ccontroller = std::make_unique<CharacterController>(capsule);
	ccontroller->set_position(get_ws_position());
	ccontroller->capsule_height = capsule->height;
	ccontroller->capsule_radius = capsule->radius;

	velocity = {};
	ccontroller->set_position(glm::vec3(0, 0.0, 0));
}

void TopDownPlayer::update() {

	mesh->get_animator()->set_float_variable("flLean", 0.f);
	const int astate = int(GetTime() * 0.5 * g_slomo.get_float()) % 3;
	mesh->get_animator()->set_int_variable("iState", astate);

	did_move = false;
	if (is_in_car)
		return;

	update_view_angles();

	if (Input::was_key_pressed(SDL_SCANCODE_T)) {
	//	return;
		using_third_person_movement = !using_third_person_movement;

		//mesh->get_animator()->play_animation(jumpSeq);
	}
	if (Input::was_key_pressed(SDL_SCANCODE_Z)) {
		eng->log_to_fullscreen_gui(Info, "entering ragdoll");
		ragdoll_enabled = !ragdoll_enabled;

		TopDownUtil::enable_ragdoll_shared(get_owner(), last_ws, ragdoll_enabled);

		MeshComponent* mesh = get_owner()->get_cached_mesh_component();
		if (mesh && mesh->get_animator()) {
			AnimatorObject* animator = mesh->get_animator();
			if (!ragdoll_enabled) {
				int index = mesh->get_index_of_bone(StringName("mixamorig:Hips"));
				glm::mat4 ws = get_ws_transform() * animator->get_global_bonemats().at(index);	//root
				glm::vec3 pos = ws[3];
				pos.y = 0;
				get_owner()->set_ws_position(pos);
				ccontroller->set_position(pos);
				animator->set_update_owner_position_to_root(false);
			}
			else {
				animator->set_update_owner_position_to_root(true);
				//set_ws_transform(glm::mat4(1.f));
			}
		}
	}

	if (ragdoll_enabled) {
		update_view();
		last_ws = get_ws_transform();
		return;
	}



	if (shoot_cooldown > 0.0)shoot_cooldown -= eng->get_dt();

	if (Input::is_mouse_down(0) || Input::get_con_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>0.5)
		shoot_gun();

	if (has_had_update) {

		if (using_third_person_movement) {

		}
		else {
			{
				glm::ivec2 mouse;
				SDL_GetMouseState(&mouse.x, &mouse.y);

				Ray r;
				r.dir = TopDownUtil::unproject_mouse_to_ray(CameraComponent::get_scene_camera()->last_vs, mouse.x, mouse.y);
				r.pos = view_pos;
				glm::vec3 intersect(0.f);
				ray_plane_intersect(r, glm::vec3(0, 1, 0), glm::vec3(0.8f), intersect);
				auto mypos = get_ws_position();
				lookdir = intersect - mypos;
				lookdir.y = 0;
				if (glm::length(lookdir) < 0.000001) lookdir = glm::vec3(1, 0, 0);
				else lookdir = glm::normalize(lookdir);
				mouse_pos = intersect;
			}
		}

	}

	glm::vec2 move = {};
	if (Input::is_key_down(SDL_SCANCODE_W))
		move.y += 1;
	if (Input::is_key_down(SDL_SCANCODE_S))
		move.y -= 1;
	if (Input::is_key_down(SDL_SCANCODE_A))
		move.x += 1;
	if (Input::is_key_down(SDL_SCANCODE_D))
		move.x -= 1;


										//	move.x += Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX);
												//	move.y += Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTY);


	float len = glm::length(move);
	if (len > 1.0)
		move = glm::normalize(move);

	if (len > 0.01)
		did_move = true;
	float dt = eng->get_dt();
	uint32_t flags = 0;
	glm::vec3 outvel;

	glm::vec3 move_front = glm::vec3(0, 0, 1);
	glm::vec3 move_side = glm::vec3(1, 0, 0);
	if (is_jumping) {
		move_front = glm::normalize(glm::vec3(-1, 0, 1));
		move_side = -glm::cross(move_front, glm::vec3(0, 1, 0));
	}
	if (using_third_person_movement) {
		move_front = get_front_dir();
		move_front.y = 0;
		move_front = glm::normalize(move_front);
		move_side = -(glm::cross(move_front, glm::vec3(0, 1, 0)));
		lookdir = move_front;
	}
	glm::vec3 displacement = (move_front * move.y + move_side * move.x) * move_speed * dt;
	ccontroller->move(displacement, dt, 0.005f, flags, outvel);
	auto mypos = get_ws_position() + displacement;

	auto font = g_assets.find_sync<GuiFont>("eng/fonts/monospace12.fnt").get();
	int start = 10;
	auto draw_text = [&](const char* s) {
		string str = s;
		TextShape shape;
		Rect2d size = GuiHelpers::calc_text_size(std::string_view(str), font);
		shape.rect.x = 20;
		shape.rect.y = start+size.h;
		shape.font = font;
		shape.color = COLOR_WHITE;
		shape.with_drop_shadow = true;
		shape.drop_shadow_ofs = 1;
		shape.text = str;
		UiSystem::inst->window.draw(shape);
		start += size.h;
	};
	draw_text(string_format("move= %f %f", move.x, move.y));
	draw_text(string_format("speed= %f", move_speed));
	draw_text(string_format("vel= %f", outvel.x));
	draw_text(string_format("displacement= %f %f %f", displacement.x, displacement.y, displacement.z));

	mesh->get_animator()->debug_print(start);


	float angle = -atan2(-lookdir.x, lookdir.z);
	auto q = glm::angleAxis(angle, glm::vec3(0, 1, 0));


	last_ws = get_ws_transform();
	get_owner()->set_ws_transform(mypos, q, get_owner()->get_ls_scale());

	has_had_update = true;

	update_view();
}

void TopDownPlayer::update_view() {
	auto pos = get_ws_position();
	//pos = glm::mix(pos, mouse_pos, 0.15);
	glm::vec3 camera_pos;
	if (is_jumping)
		camera_pos = glm::vec3(pos.x + 3.0, pos.y + 1.0, pos.z - 3.0);
	else
		camera_pos = glm::vec3(pos.x, pos.y + 12.0, pos.z - 1.0);
	glm::vec3 camera_dir = glm::normalize(camera_pos - (pos + glm::vec3(0, 1, 0)));

	if (using_third_person_movement) {
		auto front = get_front_dir();
		camera_pos = pos + glm::vec3(0, 2, 0) - front * 3.0f;
		camera_dir = -front;
	}

	if (ragdoll_enabled || !using_third_person_movement)
		this->view_pos = damp_dt_independent(camera_pos, this->view_pos, 0.002, eng->get_dt());
	else
		this->view_pos = camera_pos;

	auto finalpos = shake.evaluate(this->view_pos, camera_dir, eng->get_dt());

	auto viewMat = glm::lookAt(finalpos, finalpos - camera_dir, glm::vec3(0, 1, 0));

	the_camera->get_owner()->set_ws_transform(glm::inverse(viewMat));
	if (ragdoll_enabled) {
		glm::vec3 linvel = glm::vec3(get_ws_transform()[3] - last_ws[3]) / (float)eng->get_dt();
		float speed = glm::length(linvel);
		float desire_fov = glm::mix(50.0, 60.0, glm::min(speed * 0.1, 1.0));
		fov = damp_dt_independent(desire_fov, fov, 0.002f, (float)eng->get_dt());
	}
	else {
		fov = damp_dt_independent(50.0f, fov, 0.002f, (float)eng->get_dt());
	}
	the_camera->fov = fov;
}
