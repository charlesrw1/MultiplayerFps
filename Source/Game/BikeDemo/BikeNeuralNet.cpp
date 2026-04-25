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

void BikeNNRecorder::try_record(const BikeNNFeatures& f, float steer_label)
{
	if (!enabled || !file_) return;
	++frame_count;
	if (frame_count % frame_skip != 0) return;

	// 7 feature floats + 1 label = 8 floats per sample
	float buf[BIKENN_INPUT_DIM + 1];
	memcpy(buf, f.v, sizeof(float) * BIKENN_INPUT_DIM);
	buf[BIKENN_INPUT_DIM] = steer_label;
	fwrite(buf, sizeof(float), BIKENN_INPUT_DIM + 1, file_);
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
