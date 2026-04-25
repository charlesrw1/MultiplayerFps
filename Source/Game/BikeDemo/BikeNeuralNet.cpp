#include "BikeHeaders.h"
#include "Framework/MathLib.h"
#include "GameEnginePublic.h"
#include "Debug.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>

// ============================================================
// BikeNNRecorder
// ============================================================

BikeNNRecorder g_nn_recorder;

void BikeNNRecorder::open(const char* path)
{
	close();
	file_ = fopen(path, "wb");
	if (!file_)
		sys_print(Warning, "BikeNNRecorder: failed to open '%s' for writing\n", path);
	else
		sys_print(Debug, "BikeNNRecorder: recording to '%s'\n", path);
	sample_count = 0;
	frame_count  = 0;
}

void BikeNNRecorder::close()
{
	if (file_) {
		fclose(file_);
		file_ = nullptr;
		sys_print(Debug, "BikeNNRecorder: closed — %d samples written\n", sample_count);
	}
}

void BikeNNRecorder::try_record(const BikeObservation& obs, float steer, float power, float brake)
{
	if (!enabled || !file_) return;
	++frame_count;
	if (frame_count % frame_skip != 0) return;

	// 60-float obs + 3-float action = 63 floats per sample
	float buf[BIKE_OBS_DIM + 3];
	memcpy(buf, obs.v, sizeof(float) * BIKE_OBS_DIM);
	buf[BIKE_OBS_DIM + 0] = steer;
	buf[BIKE_OBS_DIM + 1] = power;
	buf[BIKE_OBS_DIM + 2] = brake;
	fwrite(buf, sizeof(float), BIKE_OBS_DIM + 3, file_);
	++sample_count;
}

// ============================================================
// BikeNNFeatures::extract
// ============================================================

BikeNNFeatures BikeNNFeatures::extract(BikeObject* bike, const BikeCourse* course)
{
	BikeNNFeatures out;
	const BikeWaypoint wp  = course->sample(bike->course_dist_m);
	const glm::vec3 up     = glm::vec3(0.f, 1.f, 0.f);
	const glm::vec3 p_right = glm::normalize(glm::cross(wp.forward, up));

	// 0: lateral offset from racing line (positive = right of line)
	out.v[0] = bike->lateral_pos - wp.racing_line_lateral;

	// 1: heading error — how far the bike is pointing away from path tangent
	out.v[1] = glm::dot(bike->bike_direction, p_right);

	// 2: speed m/s
	out.v[2] = bike->speed;

	// 3: signed angle to racing-line lookahead point
	const float lookahead_dist = 1.5f + bike->speed * 0.5f;
	const glm::vec3 look_pt    = course->racing_line_lookahead(bike->course_dist_m, lookahead_dist);
	const glm::vec3 bike_right = glm::normalize(glm::cross(bike->bike_direction, up));
	const glm::vec3 to_tgt     = look_pt - bike->get_ws_position();
	const float dist_to_tgt    = glm::length(to_tgt);
	out.v[3] = (dist_to_tgt > 0.01f)
		? glm::atan(glm::dot(glm::normalize(to_tgt), bike_right),
		            glm::dot(glm::normalize(to_tgt), bike->bike_direction))
		: 0.f;

	// 4-6: curvature (1/radius) at 10 m, 25 m, 50 m ahead
	out.v[4] = 1.f / glm::max(course->min_turn_radius_ahead(bike->course_dist_m, 10.f),  1.f);
	out.v[5] = 1.f / glm::max(course->min_turn_radius_ahead(bike->course_dist_m, 25.f),  1.f);
	out.v[6] = 1.f / glm::max(course->min_turn_radius_ahead(bike->course_dist_m, 50.f),  1.f);

	return out;
}

// ============================================================
// BikeSensorGrid::compute
// ============================================================

constexpr float BikeSensorGrid::ANGLES[BikeSensorGrid::NUM_SENSORS];

void BikeSensorGrid::compute(BikeObject* self, const std::vector<BikeObject*>& all_riders)
{
	for (int i = 0; i < NUM_SENSORS; ++i) {
		dist_norm[i]      = 1.0f;
		rel_speed_norm[i] = 0.0f;
	}

	// Horizontal ego frame
	const glm::vec3 up      = glm::vec3(0.f, 1.f, 0.f);
	const glm::vec2 fwd_h   = glm::normalize(glm::vec2(self->bike_direction.x, self->bike_direction.z));
	if (glm::length(fwd_h) < 1e-4f) return;
	// right = cross(fwd, up) projected horizontal: (-fz, fx)
	const glm::vec2 right_h = glm::vec2(-fwd_h.y, fwd_h.x);

	const glm::vec3 self_pos = self->get_ws_position();

	static constexpr float PI = 3.14159265f;

	for (BikeObject* other : all_riders) {
		if (other == self) continue;

		const glm::vec3 delta = other->get_ws_position() - self_pos;
		const glm::vec2 delta_h(delta.x, delta.z);
		const float dist = glm::length(delta_h);
		if (dist < 0.01f) continue;

		const float fwd_comp   = glm::dot(delta_h, fwd_h);
		const float right_comp = glm::dot(delta_h, right_h);
		const float bearing    = std::atan2(right_comp, fwd_comp);

		for (int i = 0; i < NUM_SENSORS; ++i) {
			float ad = bearing - ANGLES[i];
			if (ad >  PI) ad -= 2.f * PI;
			if (ad < -PI) ad += 2.f * PI;
			if (std::abs(ad) > HALF_WIDTH) continue;

			const float max_range = (i >= 7) ? MAX_RANGE_REAR : MAX_RANGE_FWD;
			if (dist > max_range) continue;

			const float d_norm = dist / max_range;
			if (d_norm < dist_norm[i]) {
				dist_norm[i]      = d_norm;
				rel_speed_norm[i] = glm::clamp((other->speed - self->speed) / 10.f, -1.f, 1.f);
			}
		}
	}
}

// ============================================================
// BikeObservation::extract
// ============================================================

BikeObservation BikeObservation::extract(BikeObject* bike, const BikeCourse* course,
                                          const std::vector<BikeObject*>& all_riders)
{
	BikeObservation out;
	const float d = bike->course_dist_m;
	const BikeWaypoint wp = course->sample(d);

	const glm::vec3 up    = glm::vec3(0.f, 1.f, 0.f);
	const float road_hw   = wp.road_half_width;

	// ----- 0-4: Self state -----
	out.v[0] = bike->speed / 20.f;
	out.v[1] = bike->lateral_pos / road_hw;
	out.v[2] = glm::dot(bike->bike_direction, wp.right);    // sin(heading_err)
	out.v[3] = glm::dot(bike->bike_direction, wp.forward);  // cos(heading_err)
	out.v[4] = bike->draft_factor;

	// ----- 5-6: Current road margins -----
	out.v[5] = (road_hw + bike->lateral_pos)  / road_hw;  // left margin
	out.v[6] = (road_hw - bike->lateral_pos)  / road_hw;  // right margin

	// ----- 7-31: Track lookahead (5 samples × 5 features) -----
	static constexpr float LOOK_DISTS[5] = { 5.f, 10.f, 20.f, 35.f, 60.f };
	for (int i = 0; i < 5; ++i) {
		const BikeWaypoint wa = course->sample(d + LOOK_DISTS[i]);
		const BikeWaypoint wb = course->sample(d + LOOK_DISTS[i] + 3.f);
		// signed curvature: cross(wa.fwd, wb.fwd).y / step gives 1/R in rad/m
		const float curv = glm::clamp(glm::cross(wa.forward, wb.forward).y / 3.f, -1.f, 1.f);
		const float hw_a = wa.road_half_width;
		out.v[7 + i * 5 + 0] = curv;
		out.v[7 + i * 5 + 1] = wa.racing_line_lateral / hw_a;
		out.v[7 + i * 5 + 2] = (hw_a + wa.racing_line_lateral) / hw_a;  // left margin
		out.v[7 + i * 5 + 3] = (hw_a - wa.racing_line_lateral) / hw_a;  // right margin
		out.v[7 + i * 5 + 4] = wa.gradient * 5.f;  // scale radians: ±0.2 rad → ±1.0
	}

	// ----- 32-49: Rider sensor grid -----
	BikeSensorGrid sensors;
	sensors.compute(bike, all_riders);
	for (int i = 0; i < BikeSensorGrid::NUM_SENSORS; ++i) {
		out.v[32 + i * 2 + 0] = sensors.dist_norm[i];
		out.v[32 + i * 2 + 1] = sensors.rel_speed_norm[i];
	}

	// ----- 50-52: Wind (relative to bike heading) -----
	{
		const glm::vec2 fwd_h  = glm::normalize(glm::vec2(bike->bike_direction.x, bike->bike_direction.z));
		const glm::vec2 wdir_h = glm::vec2(g_wind.wind_direction.x, g_wind.wind_direction.z);
		// convention: angle=0 → headwind (wind blows against bike front)
		out.v[50] = -glm::dot(wdir_h, fwd_h);          // cos: +1=headwind, -1=tailwind
		out.v[51] =  glm::dot(wdir_h, glm::vec2(-fwd_h.y, fwd_h.x));  // sin: +1=from left
		out.v[52] = g_wind.wind_speed / 10.f;
	}

	// ----- 53-55: Group context -----
	out.v[53] = bike->pos_in_group_norm;
	out.v[54] = bike->group_rank_norm;
	out.v[55] = bike->group_size_norm;

	// ----- 56-59: Strategic context -----
	const int n_riders = (int)all_riders.size();
	out.v[56] = bike->strategic_state.desired_effort_fraction;
	out.v[57] = bike->strategic_state.target_lateral / road_hw;
	out.v[58] = (n_riders > 1) ? (float)(bike->race_position - 1) / (float)(n_riders - 1) : 0.f;
	out.v[59] = (float)bike->strategic_state.tactical_objective / 4.f;

	return out;
}

// ============================================================
// BikeNNInput — load + forward + evaluate
// ============================================================

bool BikeNNInput::load_weights(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) {
		sys_print(Warning, "BikeNNInput: cannot open weights '%s'\n", path);
		return false;
	}

	bool ok = true;
	ok &= fread(mean_, sizeof(float), BIKENN_INPUT_DIM, f)                         == (size_t)BIKENN_INPUT_DIM;
	ok &= fread(std_,  sizeof(float), BIKENN_INPUT_DIM, f)                         == (size_t)BIKENN_INPUT_DIM;
	ok &= fread(W1_,   sizeof(float), BIKENN_HIDDEN_DIM * BIKENN_INPUT_DIM, f)     == (size_t)(BIKENN_HIDDEN_DIM * BIKENN_INPUT_DIM);
	ok &= fread(b1_,   sizeof(float), BIKENN_HIDDEN_DIM, f)                        == (size_t)BIKENN_HIDDEN_DIM;
	ok &= fread(W2_,   sizeof(float), BIKENN_HIDDEN_DIM, f)                        == (size_t)BIKENN_HIDDEN_DIM;
	ok &= fread(&b2_,  sizeof(float), 1, f)                                        == (size_t)1;
	fclose(f);

	weights_loaded = ok;
	if (ok)
		sys_print(Debug, "BikeNNInput: loaded weights from '%s'\n", path);
	else
		sys_print(Warning, "BikeNNInput: truncated weights file '%s'\n", path);
	return ok;
}

float BikeNNInput::forward(const BikeNNFeatures& raw) const
{
	// Normalize inputs
	float x[BIKENN_INPUT_DIM];
	for (int i = 0; i < BIKENN_INPUT_DIM; ++i)
		x[i] = (raw.v[i] - mean_[i]) / (std_[i] + 1e-8f);

	// Layer 1: tanh(W1 x + b1)
	float h[BIKENN_HIDDEN_DIM];
	for (int i = 0; i < BIKENN_HIDDEN_DIM; ++i) {
		float s = b1_[i];
		for (int j = 0; j < BIKENN_INPUT_DIM; ++j)
			s += W1_[i][j] * x[j];
		h[i] = std::tanh(s);
	}

	// Layer 2: tanh(W2 · h + b2)
	float out = b2_;
	for (int j = 0; j < BIKENN_HIDDEN_DIM; ++j)
		out += W2_[j] * h[j];
	return std::tanh(out);
}

void BikeNNInput::evaluate(BikeObject* my_bike)
{
	if (!course || !course->is_built) return;
	const float dt = eng->get_dt();

	float steer = 0.f;
	if (weights_loaded) {
		const BikeNNFeatures f = BikeNNFeatures::extract(my_bike, course);
		steer = forward(f);
	}
	dbg_steer = steer;

	actual_power_command = damp_dt_independent(
		target_power_watts, actual_power_command, POWER_SLEW, dt);

	BikeObject::ControlInput ci;
	ci.steer        = glm::clamp(steer, -1.f, 1.f);
	ci.power        = actual_power_command;
	ci.brake_amount = 0.f;
	my_bike->update_tick(ci);
}

// ============================================================
// BikeNNTrainer — C++ Adam trainer (no external deps)
// ============================================================

namespace {

// Flat parameter layout for easy Adam bookkeeping:
//   [0 .. 32*7-1]       W1 (row-major)
//   [32*7 .. 32*7+31]   b1
//   [32*7+32 .. 32*7+63] W2
//   [32*7+64]            b2
static constexpr int NN_PARAM_COUNT =
	BIKENN_HIDDEN_DIM * BIKENN_INPUT_DIM  // W1
	+ BIKENN_HIDDEN_DIM                   // b1
	+ BIKENN_HIDDEN_DIM                   // W2
	+ 1;                                  // b2

struct FlatNet {
	float p[NN_PARAM_COUNT] = {};

	float* W1() { return p; }
	float* b1() { return p + BIKENN_HIDDEN_DIM * BIKENN_INPUT_DIM; }
	float* W2() { return b1() + BIKENN_HIDDEN_DIM; }
	float* b2() { return W2() + BIKENN_HIDDEN_DIM; }

	const float* W1() const { return p; }
	const float* b1() const { return p + BIKENN_HIDDEN_DIM * BIKENN_INPUT_DIM; }
	const float* W2() const { return b1() + BIKENN_HIDDEN_DIM; }
	const float* b2() const { return W2() + BIKENN_HIDDEN_DIM; }
};

// Xavier uniform init: uniform(-limit, limit), limit = sqrt(6/(fan_in+fan_out))
static float xavier_sample(float fan_in, float fan_out)
{
	const float limit = std::sqrt(6.f / (fan_in + fan_out));
	return limit * (2.f * (float)rand() / (float)RAND_MAX - 1.f);
}

static void init_weights(FlatNet& net)
{
	for (int i = 0; i < BIKENN_HIDDEN_DIM; ++i)
		for (int j = 0; j < BIKENN_INPUT_DIM; ++j)
			net.W1()[i * BIKENN_INPUT_DIM + j] = xavier_sample((float)BIKENN_INPUT_DIM, (float)BIKENN_HIDDEN_DIM);
	memset(net.b1(), 0, sizeof(float) * BIKENN_HIDDEN_DIM);

	for (int j = 0; j < BIKENN_HIDDEN_DIM; ++j)
		net.W2()[j] = xavier_sample((float)BIKENN_HIDDEN_DIM, 1.f);
	*net.b2() = 0.f;
}

// Forward pass — fills h[] (hidden activations) and returns scalar output.
static float net_forward(const FlatNet& net, const float* x,
                          float h[BIKENN_HIDDEN_DIM])
{
	const float* W1 = net.W1();
	const float* b1 = net.b1();
	for (int i = 0; i < BIKENN_HIDDEN_DIM; ++i) {
		float s = b1[i];
		for (int j = 0; j < BIKENN_INPUT_DIM; ++j)
			s += W1[i * BIKENN_INPUT_DIM + j] * x[j];
		h[i] = std::tanh(s);
	}
	float out = *net.b2();
	const float* W2 = net.W2();
	for (int j = 0; j < BIKENN_HIDDEN_DIM; ++j)
		out += W2[j] * h[j];
	return std::tanh(out);
}

// Backward pass — accumulates gradients into grad (same layout as FlatNet).
// h[] must be the hidden activations from the matching forward call.
static void net_backward(const FlatNet& net, const float* x,
                          const float h[BIKENN_HIDDEN_DIM],
                          float out, float target,
                          float grad[NN_PARAM_COUNT])
{
	// dL/d_pre_out — tanh derivative * MSE gradient
	const float d_out     = out - target;                  // dL/d_out (MSE, pre-averaged)
	const float d_pre_out = d_out * (1.f - out * out);    // tanh'

	// Gradients for W2 and b2
	float* dW2 = grad + (int)(net.W2() - net.p);
	float* db2 = grad + (int)(net.b2() - net.p);
	for (int j = 0; j < BIKENN_HIDDEN_DIM; ++j)
		dW2[j] += d_pre_out * h[j];
	*db2 += d_pre_out;

	// Backprop through hidden layer
	float* dW1 = grad;
	float* db1 = grad + BIKENN_HIDDEN_DIM * BIKENN_INPUT_DIM;
	const float* W2 = net.W2();
	for (int i = 0; i < BIKENN_HIDDEN_DIM; ++i) {
		const float dh = W2[i] * d_pre_out;
		const float dp = dh * (1.f - h[i] * h[i]);  // tanh'
		db1[i] += dp;
		for (int j = 0; j < BIKENN_INPUT_DIM; ++j)
			dW1[i * BIKENN_INPUT_DIM + j] += dp * x[j];
	}
}

} // anonymous namespace

float BikeNNTrainer::train_and_save(const char* data_path, const char* weights_path,
                                     const TrainParams& p)
{
	// ---- Load data ----
	FILE* f = fopen(data_path, "rb");
	if (!f) {
		sys_print(Warning, "BikeNNTrainer: cannot open '%s'\n", data_path);
		return -1.f;
	}
	fseek(f, 0, SEEK_END);
	const long file_bytes = ftell(f);
	fseek(f, 0, SEEK_SET);

	const int floats_per_sample = BIKENN_INPUT_DIM + 1;
	const int n_samples = (int)(file_bytes / (floats_per_sample * sizeof(float)));
	if (n_samples < 10) {
		fclose(f);
		sys_print(Warning, "BikeNNTrainer: too few samples (%d) in '%s'\n", n_samples, data_path);
		return -1.f;
	}

	std::vector<float> raw(n_samples * floats_per_sample);
	fread(raw.data(), sizeof(float), raw.size(), f);
	fclose(f);
	sys_print(Debug, "BikeNNTrainer: %d samples loaded\n", n_samples);

	// ---- Compute feature mean / std ----
	float mean[BIKENN_INPUT_DIM] = {};
	float std_dev[BIKENN_INPUT_DIM] = {};

	for (int s = 0; s < n_samples; ++s)
		for (int i = 0; i < BIKENN_INPUT_DIM; ++i)
			mean[i] += raw[s * floats_per_sample + i];
	for (int i = 0; i < BIKENN_INPUT_DIM; ++i)
		mean[i] /= (float)n_samples;

	for (int s = 0; s < n_samples; ++s)
		for (int i = 0; i < BIKENN_INPUT_DIM; ++i) {
			const float d = raw[s * floats_per_sample + i] - mean[i];
			std_dev[i] += d * d;
		}
	for (int i = 0; i < BIKENN_INPUT_DIM; ++i)
		std_dev[i] = std::sqrt(std_dev[i] / (float)n_samples + 1e-8f);

	// Normalize feature columns in-place
	std::vector<float> X(n_samples * BIKENN_INPUT_DIM);
	std::vector<float> Y(n_samples);
	for (int s = 0; s < n_samples; ++s) {
		for (int i = 0; i < BIKENN_INPUT_DIM; ++i)
			X[s * BIKENN_INPUT_DIM + i] = (raw[s * floats_per_sample + i] - mean[i]) / std_dev[i];
		Y[s] = raw[s * floats_per_sample + BIKENN_INPUT_DIM];
	}
	raw.clear();
	raw.shrink_to_fit();

	// ---- Build shuffled index list ----
	std::vector<int> indices(n_samples);
	std::iota(indices.begin(), indices.end(), 0);

	// ---- Init network and Adam state ----
	FlatNet net, adam_m, adam_v;
	init_weights(net);
	memset(&adam_m, 0, sizeof(adam_m));
	memset(&adam_v, 0, sizeof(adam_v));

	constexpr float ADAM_B1  = 0.9f;
	constexpr float ADAM_B2  = 0.999f;
	constexpr float ADAM_EPS = 1e-8f;
	int adam_t = 0;

	float final_loss = 0.f;

	// ---- Training loop ----
	for (int epoch = 0; epoch < p.epochs; ++epoch) {
		// Shuffle indices
		for (int i = n_samples - 1; i > 0; --i) {
			const int j = rand() % (i + 1);
			std::swap(indices[i], indices[j]);
		}

		double epoch_loss = 0.0;
		const int n_batches = (n_samples + p.batch_size - 1) / p.batch_size;

		for (int b = 0; b < n_batches; ++b) {
			const int start = b * p.batch_size;
			const int end   = std::min(start + p.batch_size, n_samples);
			const int bsize = end - start;

			float grad[NN_PARAM_COUNT] = {};
			float batch_loss = 0.f;

			for (int bi = start; bi < end; ++bi) {
				const int s = indices[bi];
				const float* x = &X[s * BIKENN_INPUT_DIM];
				const float   y = Y[s];

				float h[BIKENN_HIDDEN_DIM];
				const float out = net_forward(net, x, h);

				const float err = out - y;
				batch_loss += err * err;

				net_backward(net, x, h, out, y, grad);
			}

			epoch_loss += batch_loss;
			++adam_t;

			// Adam update (scale grad by 1/bsize for mean loss)
			const float scale     = 1.f / (float)bsize;
			const float bc1       = 1.f - std::pow(ADAM_B1, (float)adam_t);
			const float bc2       = 1.f - std::pow(ADAM_B2, (float)adam_t);
			for (int i = 0; i < NN_PARAM_COUNT; ++i) {
				const float g = grad[i] * scale;
				adam_m.p[i] = ADAM_B1 * adam_m.p[i] + (1.f - ADAM_B1) * g;
				adam_v.p[i] = ADAM_B2 * adam_v.p[i] + (1.f - ADAM_B2) * g * g;
				const float m_hat = adam_m.p[i] / bc1;
				const float v_hat = adam_v.p[i] / bc2;
				net.p[i] -= p.lr * m_hat / (std::sqrt(v_hat) + ADAM_EPS);
			}
		}

		final_loss = (float)(epoch_loss / n_samples);
		if ((epoch + 1) % 100 == 0 || epoch == 0)
			sys_print(Debug, "  epoch %4d / %d   mse=%.6f\n", epoch + 1, p.epochs, final_loss);
	}

	sys_print(Debug, "BikeNNTrainer: training done  final_mse=%.6f\n", final_loss);

	// ---- Save weights ----
	FILE* wf = fopen(weights_path, "wb");
	if (!wf) {
		sys_print(Warning, "BikeNNTrainer: cannot write weights to '%s'\n", weights_path);
		return -1.f;
	}
	fwrite(mean,       sizeof(float), BIKENN_INPUT_DIM, wf);
	fwrite(std_dev,    sizeof(float), BIKENN_INPUT_DIM, wf);
	fwrite(net.W1(),   sizeof(float), BIKENN_HIDDEN_DIM * BIKENN_INPUT_DIM, wf);
	fwrite(net.b1(),   sizeof(float), BIKENN_HIDDEN_DIM, wf);
	fwrite(net.W2(),   sizeof(float), BIKENN_HIDDEN_DIM, wf);
	fwrite(net.b2(),   sizeof(float), 1, wf);
	fclose(wf);

	sys_print(Debug, "BikeNNTrainer: weights saved to '%s'\n", weights_path);
	return final_loss;
}

// ============================================================
// BikePPOInput — LSTM actor inference (60-float obs, 3 outputs)
// ============================================================

void BikePPOInput::reset_hidden_state()
{
	memset(h_, 0, sizeof(h_));
	memset(c_, 0, sizeof(c_));
}

bool BikePPOInput::load_weights(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) {
		sys_print(Warning, "BikePPOInput: cannot open weights '%s'\n", path);
		return false;
	}

	bool ok = true;
	ok &= fread(norm_mean_, sizeof(float), BIKE_OBS_DIM,   f) == (size_t)BIKE_OBS_DIM;
	ok &= fread(norm_std_,  sizeof(float), BIKE_OBS_DIM,   f) == (size_t)BIKE_OBS_DIM;
	ok &= fread(W1_,        sizeof(float), H * BIKE_OBS_DIM, f) == (size_t)(H * BIKE_OBS_DIM);
	ok &= fread(b1_,        sizeof(float), H,               f) == (size_t)H;
	ok &= fread(W_ih_,      sizeof(float), 4 * H * H,       f) == (size_t)(4 * H * H);
	ok &= fread(W_hh_,      sizeof(float), 4 * H * H,       f) == (size_t)(4 * H * H);
	ok &= fread(b_ih_,      sizeof(float), 4 * H,           f) == (size_t)(4 * H);
	ok &= fread(b_hh_,      sizeof(float), 4 * H,           f) == (size_t)(4 * H);
	ok &= fread(W2_,        sizeof(float), H * H,           f) == (size_t)(H * H);
	ok &= fread(b2_,        sizeof(float), H,               f) == (size_t)H;
	ok &= fread(W_steer_,   sizeof(float), H,               f) == (size_t)H;
	ok &= fread(&b_steer_,  sizeof(float), 1,               f) == (size_t)1;
	ok &= fread(W_power_,   sizeof(float), H,               f) == (size_t)H;
	ok &= fread(&b_power_,  sizeof(float), 1,               f) == (size_t)1;
	ok &= fread(W_brake_,   sizeof(float), H,               f) == (size_t)H;
	ok &= fread(&b_brake_,  sizeof(float), 1,               f) == (size_t)1;
	fclose(f);

	weights_loaded = ok;
	if (ok) {
		reset_hidden_state();
		sys_print(Debug, "BikePPOInput: loaded weights from '%s'\n", path);
	} else {
		sys_print(Warning, "BikePPOInput: truncated weights file '%s'\n", path);
	}
	return ok;
}

static inline float elu(float x)  { return x > 0.f ? x : std::exp(x) - 1.f; }
static inline float sigmoid(float x) { return 1.f / (1.f + std::exp(-x)); }

void BikePPOInput::forward(const float* obs_norm, float& out_steer, float& out_power, float& out_brake)
{
	// --- Linear1: BIKE_OBS_DIM → H, ELU ---
	float a1[H];
	for (int i = 0; i < H; ++i) {
		float s = b1_[i];
		for (int j = 0; j < BIKE_OBS_DIM; ++j)
			s += W1_[i][j] * obs_norm[j];
		a1[i] = elu(s);
	}

	// --- LSTM step: H → H ---
	// gates[0:H]=i, [H:2H]=f, [2H:3H]=g, [3H:4H]=o  (PyTorch gate order)
	float gates[4 * H];
	for (int i = 0; i < 4 * H; ++i) {
		float s = b_ih_[i] + b_hh_[i];
		for (int j = 0; j < H; ++j)
			s += W_ih_[i][j] * a1[j] + W_hh_[i][j] * h_[j];
		gates[i] = s;
	}
	for (int i = 0; i < H; ++i) {
		const float gate_i = sigmoid(gates[i]);
		const float gate_f = sigmoid(gates[H  + i]);
		const float gate_g = std::tanh (gates[2*H + i]);
		const float gate_o = sigmoid(gates[3*H + i]);
		c_[i] = gate_f * c_[i] + gate_i * gate_g;
		h_[i] = gate_o * std::tanh(c_[i]);
	}

	// --- Linear2: H → H, ELU ---
	float a2[H];
	for (int i = 0; i < H; ++i) {
		float s = b2_[i];
		for (int j = 0; j < H; ++j)
			s += W2_[i][j] * h_[j];
		a2[i] = elu(s);
	}

	// --- Output heads ---
	float raw_steer = b_steer_, raw_power = b_power_, raw_brake = b_brake_;
	for (int j = 0; j < H; ++j) {
		raw_steer += W_steer_[j] * a2[j];
		raw_power += W_power_[j] * a2[j];
		raw_brake += W_brake_[j] * a2[j];
	}
	out_steer = std::tanh(raw_steer);
	out_power = sigmoid(raw_power);
	out_brake = sigmoid(raw_brake);
}

void BikePPOInput::evaluate(BikeObject* my_bike)
{
	if (!course || !course->is_built || !all_riders) return;

	float steer = 0.f, power_frac = 0.5f, brake = 0.f;

	if (weights_loaded) {
		const BikeObservation obs = BikeObservation::extract(my_bike, course, *all_riders);

		// Normalize using saved training stats
		float obs_norm[BIKE_OBS_DIM];
		for (int i = 0; i < BIKE_OBS_DIM; ++i)
			obs_norm[i] = (obs.v[i] - norm_mean_[i]) / (norm_std_[i] + 1e-8f);

		forward(obs_norm, steer, power_frac, brake);
	}

	dbg_steer = steer;
	dbg_power = power_frac;
	dbg_brake = brake;

	const float effective_ftp = my_bike->stamina.effective_ftp;
	const float power_w = power_frac * effective_ftp * 1.3f;

	BikeObject::ControlInput ci;
	ci.steer        = glm::clamp(steer, -1.f, 1.f);
	ci.power        = power_w;
	ci.brake_amount = glm::clamp(brake, 0.f, 1.f);
	my_bike->update_tick(ci);
}
